#include "Models/Map/PngExport/PngTextRenderer.hpp"

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "extern/imgui/imstb_truetype.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace MapPngExportRaster
{
struct PngLabelFont::Impl
{
    std::vector<unsigned char> bytes;
    stbtt_fontinfo info{};
    bool ready = false;
};

PngLabelFont::PngLabelFont() : impl_(std::make_unique<Impl>()) {}
PngLabelFont::~PngLabelFont() = default;
PngLabelFont::PngLabelFont(PngLabelFont &&) noexcept = default;
PngLabelFont &PngLabelFont::operator=(PngLabelFont &&) noexcept = default;

namespace
{
void appendFontCandidates(std::vector<std::filesystem::path> &candidates,
                          std::filesystem::path directory, bool atlasSerif)
{
    if (directory.empty())
        return;
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error))
        directory = directory.parent_path();
    for (int depth = 0; depth < 8 && !directory.empty(); ++depth)
    {
        const std::filesystem::path relative = atlasSerif
            ? "SourceSerif4/SourceSerif4-Regular.ttf" : "Inter/Inter-Regular.ttf";
        candidates.push_back(directory / "Resources" / "Fonts" / relative);
        candidates.push_back(directory / "Fonts" / relative);
        const std::filesystem::path parent = directory.parent_path();
        if (parent == directory)
            break;
        directory = parent;
    }
}

std::vector<int> decodeUtf8(std::string_view text)
{
    std::vector<int> codepoints;
    codepoints.reserve(text.size());
    for (std::size_t index = 0; index < text.size();)
    {
        const unsigned char first = static_cast<unsigned char>(text[index]);
        if (first < 0x80)
        {
            codepoints.push_back(first);
            ++index;
            continue;
        }
        int value = 0;
        int continuationCount = 0;
        if ((first & 0xe0u) == 0xc0u)
        {
            value = first & 0x1fu;
            continuationCount = 1;
        }
        else if ((first & 0xf0u) == 0xe0u)
        {
            value = first & 0x0fu;
            continuationCount = 2;
        }
        else if ((first & 0xf8u) == 0xf0u)
        {
            value = first & 0x07u;
            continuationCount = 3;
        }
        else
        {
            codepoints.push_back('?');
            ++index;
            continue;
        }
        if (index + static_cast<std::size_t>(continuationCount) >= text.size())
        {
            codepoints.push_back('?');
            break;
        }
        bool valid = true;
        for (int continuation = 1; continuation <= continuationCount; ++continuation)
        {
            const unsigned char byte = static_cast<unsigned char>(text[index + continuation]);
            if ((byte & 0xc0u) != 0x80u)
            {
                valid = false;
                break;
            }
            value = (value << 6) | (byte & 0x3fu);
        }
        const bool overlong = (continuationCount == 1 && value < 0x80) ||
                              (continuationCount == 2 && value < 0x800) ||
                              (continuationCount == 3 && value < 0x10000);
        if (!valid || overlong || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
        {
            codepoints.push_back('?');
            ++index;
            continue;
        }
        codepoints.push_back(value);
        index += static_cast<std::size_t>(continuationCount + 1);
    }
    return codepoints;
}
}

std::filesystem::path pngExporterExecutableDirectory()
{
#ifdef _WIN32
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length > 0 && length < buffer.size())
        return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
#elif defined(__linux__)
    std::error_code error;
    const std::filesystem::path executable = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error && !executable.empty())
        return executable.parent_path();
#endif
    return {};
}

const PngLabelFont *pngLabelFont(bool atlasSerif)
{
    const auto load = [](bool serif) {
        auto loaded = std::make_shared<PngLabelFont>();
        std::vector<std::filesystem::path> candidates;
        std::error_code error;
        appendFontCandidates(candidates, std::filesystem::current_path(error),serif);
        appendFontCandidates(candidates, pngExporterExecutableDirectory(),serif);
        std::filesystem::path sourcePath(__FILE__);
        if (sourcePath.is_relative())
            sourcePath = std::filesystem::current_path(error) / sourcePath;
        appendFontCandidates(candidates, sourcePath.parent_path(),serif);

        std::vector<std::filesystem::path> checked;
        for (const std::filesystem::path &candidate : candidates)
        {
            const std::filesystem::path normalized = candidate.lexically_normal();
            if (std::find(checked.begin(), checked.end(), normalized) != checked.end())
                continue;
            checked.push_back(normalized);
            std::ifstream input(normalized, std::ios::binary | std::ios::ate);
            if (!input)
                continue;
            const std::streamoff size = input.tellg();
            if (size <= 0 || size > 32 * 1024 * 1024)
                continue;
            loaded->impl_->bytes.resize(static_cast<std::size_t>(size));
            input.seekg(0, std::ios::beg);
            if (!input.read(reinterpret_cast<char *>(loaded->impl_->bytes.data()), size))
            {
                loaded->impl_->bytes.clear();
                continue;
            }
            const int offset = stbtt_GetFontOffsetForIndex(loaded->impl_->bytes.data(), 0);
            if (offset >= 0 && stbtt_InitFont(&loaded->impl_->info, loaded->impl_->bytes.data(), offset) != 0)
            {
                loaded->impl_->ready = true;
                break;
            }
            loaded->impl_->bytes.clear();
        }
        return loaded;
    };
    static const std::shared_ptr<PngLabelFont> sans = load(false);
    static const std::shared_ptr<PngLabelFont> serif = load(true);
    const auto &font = atlasSerif && serif && serif->impl_->ready ? serif : sans;
    return font && font->impl_->ready ? font.get() : nullptr;
}

PngTextMetrics measurePngText(const PngLabelFont &font, std::string_view text, float pixelHeight)
{
    PngTextMetrics metrics;
    metrics.codepoints = decodeUtf8(text);
    metrics.scale = stbtt_ScaleForPixelHeight(&font.impl_->info, pixelHeight);
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&font.impl_->info, &ascent, &descent, &lineGap);
    metrics.baseline = static_cast<int>(std::ceil(ascent * metrics.scale));
    metrics.height = std::max(1, static_cast<int>(std::ceil((ascent - descent) * metrics.scale)));
    float pen = 0.0f;
    for (std::size_t index = 0; index < metrics.codepoints.size(); ++index)
    {
        int advance = 0;
        int bearing = 0;
        stbtt_GetCodepointHMetrics(&font.impl_->info, metrics.codepoints[index], &advance, &bearing);
        pen += advance * metrics.scale;
        if (index + 1 < metrics.codepoints.size())
            pen += stbtt_GetCodepointKernAdvance(&font.impl_->info, metrics.codepoints[index], metrics.codepoints[index + 1]) * metrics.scale;
    }
    metrics.width = std::max(1, static_cast<int>(std::ceil(pen)));
    return metrics;
}

void drawPngText(std::vector<Pixel> &pixels, int width, int height,
                 int left, int top, const PngLabelFont &font,
                 const PngTextMetrics &metrics, Pixel color)
{
    float pen = static_cast<float>(left);
    const int baseline = top + metrics.baseline;
    for (std::size_t index = 0; index < metrics.codepoints.size(); ++index)
    {
        const int codepoint = metrics.codepoints[index];
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetCodepointBitmapBox(&font.impl_->info, codepoint, metrics.scale, metrics.scale, &x0, &y0, &x1, &y1);
        const int glyphWidth = std::max(0, x1 - x0);
        const int glyphHeight = std::max(0, y1 - y0);
        if (glyphWidth > 0 && glyphHeight > 0)
        {
            std::vector<unsigned char> bitmap(static_cast<std::size_t>(glyphWidth * glyphHeight));
            stbtt_MakeCodepointBitmap(&font.impl_->info, bitmap.data(), glyphWidth, glyphHeight,
                                      glyphWidth, metrics.scale, metrics.scale, codepoint);
            const int destinationLeft = static_cast<int>(std::floor(pen)) + x0;
            const int destinationTop = baseline + y0;
            for (int glyphY = 0; glyphY < glyphHeight; ++glyphY)
                for (int glyphX = 0; glyphX < glyphWidth; ++glyphX)
                {
                    const int destinationX = destinationLeft + glyphX;
                    const int destinationY = destinationTop + glyphY;
                    if (destinationX < 0 || destinationY < 0 || destinationX >= width || destinationY >= height)
                        continue;
                    const std::uint8_t coverage = bitmap[static_cast<std::size_t>(glyphY * glyphWidth + glyphX)];
                    if (coverage == 0)
                        continue;
                    Pixel &destination = pixels[static_cast<std::size_t>(destinationY * width + destinationX)];
                    destination = alphaBlend(destination, color, coverage);
                }
        }
        int advance = 0;
        int bearing = 0;
        stbtt_GetCodepointHMetrics(&font.impl_->info, codepoint, &advance, &bearing);
        pen += advance * metrics.scale;
        if (index + 1 < metrics.codepoints.size())
            pen += stbtt_GetCodepointKernAdvance(&font.impl_->info, codepoint, metrics.codepoints[index + 1]) * metrics.scale;
    }
}

void drawPngTextWithHalo(std::vector<Pixel> &pixels, int width, int height,
                         int left, int top, const PngLabelFont &font,
                         const PngTextMetrics &metrics, Pixel textColor,
                         Pixel haloColor, int haloRadius)
{
    const int radius = std::clamp(haloRadius, 1, 3);
    for (int offsetY = -radius; offsetY <= radius; ++offsetY)
        for (int offsetX = -radius; offsetX <= radius; ++offsetX)
        {
            if ((offsetX == 0 && offsetY == 0) ||
                offsetX * offsetX + offsetY * offsetY > radius * radius + 1)
                continue;
            drawPngText(pixels, width, height, left + offsetX, top + offsetY,
                        font, metrics, haloColor);
        }
    drawPngText(pixels, width, height, left, top, font, metrics, textColor);
}
}
