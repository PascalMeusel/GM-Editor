#pragma once
#include "Models/Media/RgbaImageLoader.hpp"

#include <cstdio>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <gdiplus.h>
#elif defined(TTRPG_HAS_LIBPNG)
#include <png.h>
#endif

#if defined(TTRPG_HAS_LIBJPEG)
extern "C"
{
#include <jpeglib.h>
}
#include <csetjmp>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <cwchar>
#include <fstream>
#include <limits>
#include <memory>
#include <utility>
#include "Models/Media/RgbaImageMemoryInput.hpp"

namespace
{
#if defined(_WIN32)
class GdiplusSession
{
public:
    GdiplusSession()
    {
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::GdiplusStartup(&_token, &input, nullptr);
    }
    ~GdiplusSession()
    {
        if (_token != 0) Gdiplus::GdiplusShutdown(_token);
    }

private:
    ULONG_PTR _token = 0;
};

GdiplusSession &gdiplusSession()
{
    static GdiplusSession session;
    return session;
}

bool pngEncoderClsid(CLSID &result)
{
    UINT count = 0;
    UINT bytes = 0;
    if (Gdiplus::GetImageEncodersSize(&count, &bytes) != Gdiplus::Ok || bytes == 0) return false;
    std::vector<unsigned char> storage(bytes);
    auto *encoders = reinterpret_cast<Gdiplus::ImageCodecInfo *>(storage.data());
    if (Gdiplus::GetImageEncoders(count, bytes, encoders) != Gdiplus::Ok) return false;
    for (UINT index = 0; index < count; ++index)
    {
        if (encoders[index].MimeType != nullptr && std::wcscmp(encoders[index].MimeType, L"image/png") == 0)
        {
            result = encoders[index].Clsid;
            return true;
        }
    }
    return false;
}
#endif

void setError(std::string *errorCode, const char *value)
{
    if (errorCode != nullptr) *errorCode = value;
}

bool checkedPixelBytes(int width, int height, std::size_t &bytes)
{
    if (width <= 0 || height <= 0) return false;
    const std::uint64_t pixels = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    if (pixels > std::numeric_limits<std::size_t>::max() / 4u) return false;
    bytes = static_cast<std::size_t>(pixels * 4u);
    return true;
}

bool dimensionsAllowed(int width, int height, const RgbaImageLoadOptions &options, std::string *errorCode)
{
    if (width <= 0 || height <= 0)
    {
        setError(errorCode, "image.invalid_dimensions");
        return false;
    }
    const std::uint64_t pixels = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    if ((options.maximumSourceEdge > 0 &&
         (width > options.maximumSourceEdge || height > options.maximumSourceEdge)) ||
        (options.maximumSourcePixels > 0 && pixels > options.maximumSourcePixels))
    {
        setError(errorCode, "image.too_large");
        return false;
    }
    std::size_t ignored = 0;
    if (!checkedPixelBytes(width, height, ignored))
    {
        setError(errorCode, "image.too_large");
        return false;
    }
    return true;
}

bool probePngDimensions(std::istream &stream, int &width, int &height)
{
    std::array<unsigned char, 24> header{};
    if (!stream.read(reinterpret_cast<char *>(header.data()), static_cast<std::streamsize>(header.size())))
        return false;
    static constexpr std::array<unsigned char, 8> signature{{0x89u, 'P', 'N', 'G', 0x0du, 0x0au, 0x1au, 0x0au}};
    if (!std::equal(signature.begin(), signature.end(), header.begin()) ||
        header[12] != 'I' || header[13] != 'H' || header[14] != 'D' || header[15] != 'R')
        return false;
    const std::uint32_t encodedWidth = (static_cast<std::uint32_t>(header[16]) << 24u) |
                                       (static_cast<std::uint32_t>(header[17]) << 16u) |
                                       (static_cast<std::uint32_t>(header[18]) << 8u) |
                                       static_cast<std::uint32_t>(header[19]);
    const std::uint32_t encodedHeight = (static_cast<std::uint32_t>(header[20]) << 24u) |
                                        (static_cast<std::uint32_t>(header[21]) << 16u) |
                                        (static_cast<std::uint32_t>(header[22]) << 8u) |
                                        static_cast<std::uint32_t>(header[23]);
    if (encodedWidth == 0 || encodedHeight == 0)
        return false;
    width = encodedWidth > static_cast<std::uint32_t>(std::numeric_limits<int>::max())
                ? std::numeric_limits<int>::max()
                : static_cast<int>(encodedWidth);
    height = encodedHeight > static_cast<std::uint32_t>(std::numeric_limits<int>::max())
                 ? std::numeric_limits<int>::max()
                 : static_cast<int>(encodedHeight);
    return true;
}

bool jpegStartOfFrame(unsigned char marker)
{
    return marker == 0xc0u || marker == 0xc1u || marker == 0xc2u || marker == 0xc3u ||
           marker == 0xc5u || marker == 0xc6u || marker == 0xc7u || marker == 0xc9u ||
           marker == 0xcau || marker == 0xcbu || marker == 0xcdu || marker == 0xceu ||
           marker == 0xcfu;
}

bool probeJpegDimensions(std::istream &stream, int &width, int &height)
{
    unsigned char first = 0;
    unsigned char second = 0;
    if (!stream.read(reinterpret_cast<char *>(&first), 1) ||
        !stream.read(reinterpret_cast<char *>(&second), 1) || first != 0xffu || second != 0xd8u)
        return false;
    while (stream)
    {
        unsigned char prefix = 0;
        if (!stream.read(reinterpret_cast<char *>(&prefix), 1)) break;
        if (prefix != 0xffu) continue;
        unsigned char marker = 0xffu;
        while (marker == 0xffu && stream.read(reinterpret_cast<char *>(&marker), 1)) {}
        if (!stream || marker == 0xd9u || marker == 0xdau) break;
        if (marker == 0x00u || marker == 0x01u || (marker >= 0xd0u && marker <= 0xd8u)) continue;
        unsigned char lengthBytes[2]{};
        if (!stream.read(reinterpret_cast<char *>(lengthBytes), 2)) break;
        const std::uint16_t length = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(lengthBytes[0]) << 8u) | lengthBytes[1]);
        if (length < 2u) return false;
        if (jpegStartOfFrame(marker))
        {
            unsigned char frame[5]{};
            if (length < 7u || !stream.read(reinterpret_cast<char *>(frame), 5)) return false;
            height = static_cast<int>((static_cast<unsigned int>(frame[1]) << 8u) | frame[2]);
            width = static_cast<int>((static_cast<unsigned int>(frame[3]) << 8u) | frame[4]);
            return width > 0 && height > 0;
        }
        stream.seekg(static_cast<std::streamoff>(length - 2u), std::ios::cur);
    }
    return false;
}

bool probeEncodedDimensions(const std::filesystem::path &path, int &width, int &height,
                            const std::vector<unsigned char> *encoded)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    std::ifstream file;
    if (!encoded) file.open(path,std::ios::binary);
    const std::vector<unsigned char> empty;
    gm::image_decode::MemoryInput memory(encoded?*encoded:empty);
    std::istream &stream=encoded?static_cast<std::istream &>(memory):file;
    if (extension == ".png") return probePngDimensions(stream, width, height);
    if (extension == ".jpg" || extension == ".jpeg") return probeJpegDimensions(stream, width, height);
    return false;
}

std::uint16_t readU16(const std::vector<unsigned char> &bytes, std::size_t offset, bool little, bool &ok)
{
    if (offset > bytes.size() || bytes.size() - offset < 2)
    {
        ok = false;
        return 0;
    }
    if (little)
        return static_cast<std::uint16_t>(bytes[offset] | (static_cast<std::uint16_t>(bytes[offset + 1]) << 8));
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8) | bytes[offset + 1]);
}

std::uint32_t readU32(const std::vector<unsigned char> &bytes, std::size_t offset, bool little, bool &ok)
{
    if (offset > bytes.size() || bytes.size() - offset < 4)
    {
        ok = false;
        return 0;
    }
    if (little)
        return static_cast<std::uint32_t>(bytes[offset]) |
               (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
               (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    return (static_cast<std::uint32_t>(bytes[offset]) << 24) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
           static_cast<std::uint32_t>(bytes[offset + 3]);
}

int parseExifOrientation(const std::vector<unsigned char> &segment)
{
    static constexpr std::array<unsigned char, 6> signature{{'E', 'x', 'i', 'f', 0, 0}};
    if (segment.size() < 14 || !std::equal(signature.begin(), signature.end(), segment.begin())) return 1;
    const std::size_t tiff = signature.size();
    const bool little = segment[tiff] == 'I' && segment[tiff + 1] == 'I';
    const bool big = segment[tiff] == 'M' && segment[tiff + 1] == 'M';
    if (!little && !big) return 1;
    bool ok = true;
    if (readU16(segment, tiff + 2, little, ok) != 42 || !ok) return 1;
    const std::uint32_t relative = readU32(segment, tiff + 4, little, ok);
    if (!ok || relative > segment.size() - tiff) return 1;
    const std::size_t ifd = tiff + static_cast<std::size_t>(relative);
    const std::uint16_t count = readU16(segment, ifd, little, ok);
    if (!ok) return 1;
    for (std::uint16_t index = 0; index < count; ++index)
    {
        if (index > (std::numeric_limits<std::size_t>::max() - ifd - 2u) / 12u) return 1;
        const std::size_t entry = ifd + 2u + static_cast<std::size_t>(index) * 12u;
        const std::uint16_t tag = readU16(segment, entry, little, ok);
        const std::uint16_t type = readU16(segment, entry + 2u, little, ok);
        const std::uint32_t values = readU32(segment, entry + 4u, little, ok);
        if (!ok) return 1;
        if (tag == 0x0112u && type == 3u && values >= 1u)
        {
            const int orientation = static_cast<int>(readU16(segment, entry + 8u, little, ok));
            return ok && orientation >= 1 && orientation <= 8 ? orientation : 1;
        }
    }
    return 1;
}

#if defined(TTRPG_HAS_LIBJPEG) && !defined(_WIN32)
struct JpegErrorManager
{
    jpeg_error_mgr base;
    std::jmp_buf jump;
};

void jpegErrorExit(j_common_ptr context)
{
    auto *manager = reinterpret_cast<JpegErrorManager *>(context->err);
    std::longjmp(manager->jump, 1);
}
#endif
}
