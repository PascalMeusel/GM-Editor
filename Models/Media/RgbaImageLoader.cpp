#include "Models/Media/RgbaImageLoaderSupport.hpp"

static int readJpegExifOrientationStream(std::istream &stream)
{
    unsigned char first = 0;
    unsigned char second = 0;
    if (!stream.read(reinterpret_cast<char *>(&first), 1) ||
        !stream.read(reinterpret_cast<char *>(&second), 1) || first != 0xffu || second != 0xd8u)
        return 1;
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
            (static_cast<std::uint16_t>(lengthBytes[0]) << 8) | lengthBytes[1]);
        if (length < 2u) return 1;
        const std::size_t payloadBytes = static_cast<std::size_t>(length - 2u);
        if (marker == 0xe1u)
        {
            std::vector<unsigned char> payload(payloadBytes);
            if (!payload.empty() && !stream.read(reinterpret_cast<char *>(payload.data()),
                                                 static_cast<std::streamsize>(payload.size())))
                return 1;
            const int orientation = parseExifOrientation(payload);
            if (orientation != 1) return orientation;
        }
        else
        {
            stream.seekg(static_cast<std::streamoff>(payloadBytes), std::ios::cur);
        }
    }
    return 1;
}

int readJpegExifOrientation(const std::filesystem::path &path)
{
    std::ifstream stream(path,std::ios::binary);
    return readJpegExifOrientationStream(stream);
}
static int inputOrientation(const std::filesystem::path &path,const std::vector<unsigned char> *encoded)
{
    if (!encoded) return readJpegExifOrientation(path);
    gm::image_decode::MemoryInput stream(*encoded);
    return readJpegExifOrientationStream(stream);
}

bool applyRgbaExifOrientation(LoadedRgbaImage &image, int orientation)
{
    if (orientation < 1 || orientation > 8) return false;
    std::size_t sourceBytes = 0;
    if (!checkedPixelBytes(image.width, image.height, sourceBytes) || image.pixels.size() < sourceBytes) return false;
    if (image.sourceWidth <= 0 || image.sourceHeight <= 0)
    {
        image.sourceWidth = image.width;
        image.sourceHeight = image.height;
    }
    image.exifOrientation = orientation;
    if (orientation == 1) return true;
    const int sourceWidth = image.width;
    const int sourceHeight = image.height;
    const int targetWidth = orientation >= 5 ? sourceHeight : sourceWidth;
    const int targetHeight = orientation >= 5 ? sourceWidth : sourceHeight;
    std::size_t targetBytes = 0;
    if (!checkedPixelBytes(targetWidth, targetHeight, targetBytes)) return false;
    std::vector<unsigned char> oriented;
    try
    {
        oriented.resize(targetBytes);
    }
    catch (...)
    {
        return false;
    }
    for (int y = 0; y < targetHeight; ++y)
    {
        for (int x = 0; x < targetWidth; ++x)
        {
            int sx = x;
            int sy = y;
            switch (orientation)
            {
            case 2: sx = sourceWidth - 1 - x; sy = y; break;
            case 3: sx = sourceWidth - 1 - x; sy = sourceHeight - 1 - y; break;
            case 4: sx = x; sy = sourceHeight - 1 - y; break;
            case 5: sx = y; sy = x; break;
            case 6: sx = y; sy = sourceHeight - 1 - x; break;
            case 7: sx = sourceWidth - 1 - y; sy = sourceHeight - 1 - x; break;
            case 8: sx = sourceWidth - 1 - y; sy = x; break;
            default: break;
            }
            const std::size_t sourceOffset = static_cast<std::size_t>((sy * sourceWidth + sx) * 4);
            const std::size_t targetOffset = static_cast<std::size_t>((y * targetWidth + x) * 4);
            std::copy_n(image.pixels.data() + sourceOffset, 4, oriented.data() + targetOffset);
        }
    }
    image.width = targetWidth;
    image.height = targetHeight;
    image.pixels = std::move(oriented);
    return true;
}

bool loadRgbaImage(const std::filesystem::path &path, LoadedRgbaImage &image)
{
    return loadRgbaImage(path, image, RgbaImageLoadOptions{}, nullptr);
}

static bool loadRgbaImageInput(const std::filesystem::path &path,
                   const std::vector<unsigned char> *encoded,LoadedRgbaImage &image,
                   const RgbaImageLoadOptions &options,
                   std::string *errorCode)
{
    image = LoadedRgbaImage{};
    setError(errorCode, "");
    try
    {
        // GDI+ may allocate decoder-owned buffers while constructing Bitmap.
        // Inspect PNG/JPEG headers first so untrusted oversized imports are
        // rejected before entering a platform decoder or allocating RGBA data.
        if (options.maximumSourceEdge > 0 || options.maximumSourcePixels > 0)
        {
            int probedWidth = 0;
            int probedHeight = 0;
            if (probeEncodedDimensions(path, probedWidth, probedHeight,encoded))
            {
                image.sourceWidth = probedWidth;
                image.sourceHeight = probedHeight;
                if (!dimensionsAllowed(probedWidth, probedHeight, options, errorCode))
                    return false;
            }
        }
#if defined(_WIN32)
        (void)gdiplusSession();
        auto stream=encoded?gm::image_decode::encodedStream(*encoded):gm::image_decode::OwnedStream{};
        if (encoded && !stream) { setError(errorCode,"image.decode_failed"); return false; }
        // Stream is declared first, so Bitmap is destroyed before its input.
        auto owner=encoded?std::make_unique<Gdiplus::Bitmap>(stream.get(),false)
                          :std::make_unique<Gdiplus::Bitmap>(path.wstring().c_str(),false);
        auto &bitmap=*owner;
        if (bitmap.GetLastStatus() != Gdiplus::Ok)
        {
            setError(errorCode, "image.decode_failed");
            return false;
        }
        const int sourceWidth = static_cast<int>(bitmap.GetWidth());
        const int sourceHeight = static_cast<int>(bitmap.GetHeight());
        image.sourceWidth = sourceWidth;
        image.sourceHeight = sourceHeight;
        if (!dimensionsAllowed(sourceWidth, sourceHeight, options, errorCode)) return false;
        const int orientation = inputOrientation(path,encoded);
        image.width = sourceWidth;
        image.height = sourceHeight;
        image.sourceWidth = sourceWidth;
        image.sourceHeight = sourceHeight;
        image.exifOrientation = orientation;
        std::size_t byteCount = 0;
        checkedPixelBytes(sourceWidth, sourceHeight, byteCount);
        image.pixels.resize(byteCount);
        Gdiplus::Rect rect(0, 0, sourceWidth, sourceHeight);
        Gdiplus::BitmapData data{};
        if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) != Gdiplus::Ok)
        {
            image = LoadedRgbaImage{};
            setError(errorCode, "image.decode_failed");
            return false;
        }
        const unsigned char *scan0 = static_cast<const unsigned char *>(data.Scan0);
        const int stride = data.Stride;
        for (int y = 0; y < sourceHeight; ++y)
        {
            const unsigned char *source = stride >= 0 ? scan0 + y * stride
                                                       : scan0 + (sourceHeight - 1 - y) * (-stride);
            for (int x = 0; x < sourceWidth; ++x)
            {
                const std::size_t sourceIndex = static_cast<std::size_t>(x * 4);
                const std::size_t targetIndex = static_cast<std::size_t>((y * sourceWidth + x) * 4);
                image.pixels[targetIndex] = source[sourceIndex + 2];
                image.pixels[targetIndex + 1] = source[sourceIndex + 1];
                image.pixels[targetIndex + 2] = source[sourceIndex];
                image.pixels[targetIndex + 3] = source[sourceIndex + 3];
            }
        }
        bitmap.UnlockBits(&data);
        if (options.applyExifOrientation && !applyRgbaExifOrientation(image, orientation))
        {
            image = LoadedRgbaImage{};
            setError(errorCode, "image.orientation_failed");
            return false;
        }
        return true;
#else
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        const bool isJpeg = extension == ".jpg" || extension == ".jpeg";
#if defined(TTRPG_HAS_LIBJPEG)
        if (isJpeg)
        {
            std::FILE *file = encoded?nullptr:std::fopen(path.string().c_str(), "rb");
            if (!encoded && file == nullptr)
            {
                setError(errorCode, "image.decode_failed");
                return false;
            }
            jpeg_decompress_struct decoder{};
            JpegErrorManager manager{};
            std::vector<unsigned char> row;
            decoder.err = jpeg_std_error(&manager.base);
            manager.base.error_exit = jpegErrorExit;
            if (setjmp(manager.jump))
            {
                jpeg_destroy_decompress(&decoder);
                if (file) std::fclose(file);
                image = LoadedRgbaImage{};
                setError(errorCode, "image.decode_failed");
                return false;
            }
            jpeg_create_decompress(&decoder);
            if (encoded) jpeg_mem_src(&decoder,encoded->data(),static_cast<unsigned long>(encoded->size()));
            else jpeg_stdio_src(&decoder, file);
            jpeg_read_header(&decoder, TRUE);
            const int sourceWidth = static_cast<int>(decoder.image_width);
            const int sourceHeight = static_cast<int>(decoder.image_height);
            image.sourceWidth = sourceWidth;
            image.sourceHeight = sourceHeight;
            if (!dimensionsAllowed(sourceWidth, sourceHeight, options, errorCode))
            {
                jpeg_destroy_decompress(&decoder);
                if (file) std::fclose(file);
                return false;
            }
            decoder.out_color_space = JCS_RGB;
            jpeg_start_decompress(&decoder);
            image.width = sourceWidth;
            image.height = sourceHeight;
            image.sourceWidth = sourceWidth;
            image.sourceHeight = sourceHeight;
            image.exifOrientation = inputOrientation(path,encoded);
            std::size_t byteCount = 0;
            checkedPixelBytes(sourceWidth, sourceHeight, byteCount);
            image.pixels.resize(byteCount);
            row.resize(static_cast<std::size_t>(sourceWidth) * decoder.output_components);
            while (decoder.output_scanline < decoder.output_height)
            {
                JSAMPROW rowPointer = row.data();
                jpeg_read_scanlines(&decoder, &rowPointer, 1);
                const int y = static_cast<int>(decoder.output_scanline - 1);
                for (int x = 0; x < sourceWidth; ++x)
                {
                    const std::size_t sourceOffset = static_cast<std::size_t>(x) * decoder.output_components;
                    const std::size_t targetOffset = static_cast<std::size_t>((y * sourceWidth + x) * 4);
                    image.pixels[targetOffset] = row[sourceOffset];
                    image.pixels[targetOffset + 1] = decoder.output_components > 1 ? row[sourceOffset + 1] : row[sourceOffset];
                    image.pixels[targetOffset + 2] = decoder.output_components > 2 ? row[sourceOffset + 2] : row[sourceOffset];
                    image.pixels[targetOffset + 3] = 255;
                }
            }
            jpeg_finish_decompress(&decoder);
            jpeg_destroy_decompress(&decoder);
            if (file) std::fclose(file);
            if (options.applyExifOrientation && !applyRgbaExifOrientation(image, image.exifOrientation))
            {
                image = LoadedRgbaImage{};
                setError(errorCode, "image.orientation_failed");
                return false;
            }
            return true;
        }
#else
        if (isJpeg)
        {
            setError(errorCode, "image.jpeg_decoder_unavailable");
            return false;
        }
#endif
#if defined(TTRPG_HAS_LIBPNG)
        using FilePtr = std::unique_ptr<std::FILE, int (*)(std::FILE *)>;
        FilePtr file(encoded?nullptr:std::fopen(path.string().c_str(), "rb"), &std::fclose);
        if (!encoded && !file)
        {
            setError(errorCode, "image.decode_failed");
            return false;
        }
        png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!png)
        {
            setError(errorCode, "image.decode_failed");
            return false;
        }
        png_infop info = png_create_info_struct(png);
        if (!info)
        {
            png_destroy_read_struct(&png, nullptr, nullptr);
            setError(errorCode, "image.decode_failed");
            return false;
        }
        std::vector<unsigned char> decoded;
        std::vector<png_bytep> rows;
        const std::vector<unsigned char> empty;
        gm::image_decode::PngInput input{encoded?*encoded:empty};
        if (setjmp(png_jmpbuf(png)))
        {
            png_destroy_read_struct(&png, &info, nullptr);
            image = LoadedRgbaImage{};
            setError(errorCode, "image.decode_failed");
            return false;
        }
        if (encoded) png_set_read_fn(png,&input,gm::image_decode::readPng);
        else png_init_io(png, file.get());
        png_read_info(png, info);
        const png_uint_32 width = png_get_image_width(png, info);
        const png_uint_32 height = png_get_image_height(png, info);
        if (width <= static_cast<png_uint_32>(std::numeric_limits<int>::max()))
            image.sourceWidth = static_cast<int>(width);
        if (height <= static_cast<png_uint_32>(std::numeric_limits<int>::max()))
            image.sourceHeight = static_cast<int>(height);
        if (width > static_cast<png_uint_32>(std::numeric_limits<int>::max()) ||
            height > static_cast<png_uint_32>(std::numeric_limits<int>::max()) ||
            !dimensionsAllowed(static_cast<int>(width), static_cast<int>(height), options, errorCode))
        {
            png_destroy_read_struct(&png, &info, nullptr);
            return false;
        }
        png_byte colorType = png_get_color_type(png, info);
        const png_byte bitDepth = png_get_bit_depth(png, info);
        if (bitDepth == 16) png_set_strip_16(png);
        if (colorType == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
        if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) png_set_expand_gray_1_2_4_to_8(png);
        if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
        if (colorType == PNG_COLOR_TYPE_RGB || colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_PALETTE)
            png_set_filler(png, 0xff, PNG_FILLER_AFTER);
        if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
        png_read_update_info(png, info);
        const png_size_t rowBytes = png_get_rowbytes(png, info);
        if (rowBytes < width * 4u)
        {
            png_destroy_read_struct(&png, &info, nullptr);
            setError(errorCode, "image.decode_failed");
            return false;
        }
        image.width = static_cast<int>(width);
        image.height = static_cast<int>(height);
        image.sourceWidth = image.width;
        image.sourceHeight = image.height;
        image.exifOrientation = 1;
        decoded.resize(static_cast<std::size_t>(rowBytes * height));
        rows.resize(static_cast<std::size_t>(height));
        for (png_uint_32 y = 0; y < height; ++y)
            rows[static_cast<std::size_t>(y)] = decoded.data() + static_cast<std::size_t>(y * rowBytes);
        png_read_image(png, rows.data());
        png_destroy_read_struct(&png, &info, nullptr);
        std::size_t packedBytes = 0;
        checkedPixelBytes(image.width, image.height, packedBytes);
        image.pixels.resize(packedBytes);
        for (png_uint_32 y = 0; y < height; ++y)
            std::copy_n(decoded.data() + static_cast<std::size_t>(y * rowBytes),
                        static_cast<std::size_t>(width * 4u),
                        image.pixels.data() + static_cast<std::size_t>(y * width * 4u));
        return true;
#else
        (void)path;
        setError(errorCode, "image.decoder_unavailable");
        return false;
#endif
#endif
    }
    catch (const std::bad_alloc &)
    {
        image = LoadedRgbaImage{};
        setError(errorCode, "image.too_large");
        return false;
    }
    catch (...)
    {
        image = LoadedRgbaImage{};
        setError(errorCode, "image.decode_failed");
        return false;
    }
}

bool loadRgbaImage(const std::filesystem::path &path,LoadedRgbaImage &image,
                   const RgbaImageLoadOptions &options,std::string *errorCode)
{ return loadRgbaImageInput(path,nullptr,image,options,errorCode); }

bool loadRgbaImage(const std::vector<unsigned char> &encoded,const std::string &extension,
                   LoadedRgbaImage &image,const RgbaImageLoadOptions &options,std::string *errorCode)
{ return loadRgbaImageInput(std::filesystem::u8path("image"+extension),&encoded,image,options,errorCode); }
