#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct LoadedRgbaImage
{
    int width = 0;
    int height = 0;
    // Dimensions as encoded in the source before EXIF orientation is applied.
    int sourceWidth = 0;
    int sourceHeight = 0;
    // TIFF/EXIF orientation (1..8). PNG and images without a valid tag use 1.
    int exifOrientation = 1;
    std::vector<unsigned char> pixels;
};

struct RgbaImageLoadOptions
{
    // A zero limit means unlimited. Callers that handle untrusted files should
    // always provide both limits so dimensions are rejected before allocating
    // the RGBA buffer.
    int maximumSourceEdge = 0;
    std::uint64_t maximumSourcePixels = 0;
    bool applyExifOrientation = true;
};

bool loadRgbaImage(const std::filesystem::path &path, LoadedRgbaImage &image);

bool loadRgbaImage(const std::filesystem::path &path,
                   LoadedRgbaImage &image,
                   const RgbaImageLoadOptions &options,
                   std::string *errorCode = nullptr);

// Uses the same decoder, dimension checks and orientation path as file input.
// The caller owns immutable encoded bytes for the duration of this call.
bool loadRgbaImage(const std::vector<unsigned char> &encoded,
                   const std::string &extension,
                   LoadedRgbaImage &image,
                   const RgbaImageLoadOptions &options,
                   std::string *errorCode = nullptr);

// These helpers are public so import and regression tests can verify every
// mirrored EXIF orientation without relying on platform-specific codecs.
int readJpegExifOrientation(const std::filesystem::path &path);
bool applyRgbaExifOrientation(LoadedRgbaImage &image, int orientation);

// Deterministic, fixed-point bilinear resize and PNG writer used for managed
// map working proxies. The source image is never modified.
bool resizeRgbaImage(const LoadedRgbaImage &source, int width, int height, LoadedRgbaImage &result);
bool saveRgbaImageAsPng(const std::filesystem::path &path,
                        const LoadedRgbaImage &image,
                        std::string *errorCode = nullptr);
