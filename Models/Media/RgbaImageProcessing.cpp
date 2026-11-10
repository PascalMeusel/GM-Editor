#include "Models/Media/RgbaImageLoaderSupport.hpp"

bool resizeRgbaImage(const LoadedRgbaImage &source, int width, int height, LoadedRgbaImage &result)
{
    result = LoadedRgbaImage{};
    std::size_t sourceBytes = 0;
    std::size_t targetBytes = 0;
    if (!checkedPixelBytes(source.width, source.height, sourceBytes) || source.pixels.size() < sourceBytes ||
        !checkedPixelBytes(width, height, targetBytes)) return false;
    try
    {
        result.width = width;
        result.height = height;
        result.sourceWidth = width;
        result.sourceHeight = height;
        result.exifOrientation = 1;
        result.pixels.resize(targetBytes);
        for (int y = 0; y < height; ++y)
        {
            std::int64_t syFixed =
                (static_cast<std::int64_t>(2 * y + 1) * source.height * 65536ll) / (2ll * height) - 32768ll;
            syFixed = std::clamp<std::int64_t>(syFixed, 0, static_cast<std::int64_t>(source.height - 1) << 16);
            const int y0 = static_cast<int>(syFixed >> 16);
            const int y1 = std::min(y0 + 1, source.height - 1);
            const std::uint32_t wy = static_cast<std::uint32_t>(syFixed & 0xffffll);
            for (int x = 0; x < width; ++x)
            {
                std::int64_t sxFixed =
                    (static_cast<std::int64_t>(2 * x + 1) * source.width * 65536ll) / (2ll * width) - 32768ll;
                sxFixed = std::clamp<std::int64_t>(sxFixed, 0, static_cast<std::int64_t>(source.width - 1) << 16);
                const int x0 = static_cast<int>(sxFixed >> 16);
                const int x1 = std::min(x0 + 1, source.width - 1);
                const std::uint32_t wx = static_cast<std::uint32_t>(sxFixed & 0xffffll);
                const std::size_t targetOffset = static_cast<std::size_t>((y * width + x) * 4);
                for (int channel = 0; channel < 4; ++channel)
                {
                    const std::uint64_t top =
                        static_cast<std::uint64_t>(source.pixels[static_cast<std::size_t>((y0 * source.width + x0) * 4 + channel)]) * (65536u - wx) +
                        static_cast<std::uint64_t>(source.pixels[static_cast<std::size_t>((y0 * source.width + x1) * 4 + channel)]) * wx;
                    const std::uint64_t bottom =
                        static_cast<std::uint64_t>(source.pixels[static_cast<std::size_t>((y1 * source.width + x0) * 4 + channel)]) * (65536u - wx) +
                        static_cast<std::uint64_t>(source.pixels[static_cast<std::size_t>((y1 * source.width + x1) * 4 + channel)]) * wx;
                    const std::uint64_t combined = top * (65536u - wy) + bottom * wy;
                    result.pixels[targetOffset + static_cast<std::size_t>(channel)] =
                        static_cast<unsigned char>((combined + (1ull << 31)) >> 32);
                }
            }
        }
        return true;
    }
    catch (...)
    {
        result = LoadedRgbaImage{};
        return false;
    }
}

bool saveRgbaImageAsPng(const std::filesystem::path &path,
                        const LoadedRgbaImage &image,
                        std::string *errorCode)
{
    setError(errorCode, "");
    std::size_t byteCount = 0;
    if (!checkedPixelBytes(image.width, image.height, byteCount) || image.pixels.size() < byteCount)
    {
        setError(errorCode, "image.invalid_dimensions");
        return false;
    }
#if defined(_WIN32)
    (void)gdiplusSession();
    Gdiplus::Bitmap bitmap(image.width, image.height, PixelFormat32bppARGB);
    if (bitmap.GetLastStatus() != Gdiplus::Ok)
    {
        setError(errorCode, "image.encode_failed");
        return false;
    }
    Gdiplus::Rect rect(0, 0, image.width, image.height);
    Gdiplus::BitmapData data{};
    if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &data) != Gdiplus::Ok)
    {
        setError(errorCode, "image.encode_failed");
        return false;
    }
    unsigned char *scan0 = static_cast<unsigned char *>(data.Scan0);
    const int stride = data.Stride;
    for (int y = 0; y < image.height; ++y)
    {
        unsigned char *target = stride >= 0 ? scan0 + y * stride : scan0 + (image.height - 1 - y) * (-stride);
        for (int x = 0; x < image.width; ++x)
        {
            const std::size_t sourceOffset = static_cast<std::size_t>((y * image.width + x) * 4);
            const std::size_t targetOffset = static_cast<std::size_t>(x * 4);
            target[targetOffset] = image.pixels[sourceOffset + 2];
            target[targetOffset + 1] = image.pixels[sourceOffset + 1];
            target[targetOffset + 2] = image.pixels[sourceOffset];
            target[targetOffset + 3] = image.pixels[sourceOffset + 3];
        }
    }
    bitmap.UnlockBits(&data);
    CLSID encoder{};
    if (!pngEncoderClsid(encoder) || bitmap.Save(path.wstring().c_str(), &encoder, nullptr) != Gdiplus::Ok)
    {
        setError(errorCode, "image.encode_failed");
        return false;
    }
    return true;
#elif defined(TTRPG_HAS_LIBPNG)
    std::FILE *file = std::fopen(path.string().c_str(), "wb");
    if (file == nullptr)
    {
        setError(errorCode, "image.encode_failed");
        return false;
    }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png ? png_create_info_struct(png) : nullptr;
    std::vector<png_bytep> rows;
    if (png == nullptr || info == nullptr)
    {
        if (png != nullptr) png_destroy_write_struct(&png, nullptr);
        std::fclose(file);
        setError(errorCode, "image.encode_failed");
        return false;
    }
    if (setjmp(png_jmpbuf(png)))
    {
        png_destroy_write_struct(&png, &info);
        std::fclose(file);
        setError(errorCode, "image.encode_failed");
        return false;
    }
    png_init_io(png, file);
    png_set_IHDR(png, info, static_cast<png_uint_32>(image.width), static_cast<png_uint_32>(image.height),
                 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    rows.resize(static_cast<std::size_t>(image.height));
    for (int y = 0; y < image.height; ++y)
        rows[static_cast<std::size_t>(y)] = const_cast<png_bytep>(
            image.pixels.data() + static_cast<std::size_t>(y * image.width * 4));
    png_write_image(png, rows.data());
    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    std::fclose(file);
    return true;
#else
    (void)path;
    setError(errorCode, "image.encoder_unavailable");
    return false;
#endif
}
