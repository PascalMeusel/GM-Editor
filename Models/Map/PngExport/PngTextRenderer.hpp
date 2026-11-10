#pragma once

#include "Models/Map/PngExport/PngRasterCanvas.hpp"

#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

namespace MapPngExportRaster
{
struct PngTextMetrics;

class PngLabelFont final
{
public:
    PngLabelFont();
    ~PngLabelFont();
    PngLabelFont(PngLabelFont &&) noexcept;
    PngLabelFont &operator=(PngLabelFont &&) noexcept;

    PngLabelFont(const PngLabelFont &) = delete;
    PngLabelFont &operator=(const PngLabelFont &) = delete;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    friend const PngLabelFont *pngLabelFont(bool);
    friend struct PngTextMetrics;
    friend PngTextMetrics measurePngText(const PngLabelFont &, std::string_view, float);
    friend void drawPngText(std::vector<Pixel> &, int, int, int, int,
                            const PngLabelFont &, const PngTextMetrics &, Pixel);
};

struct PngTextMetrics
{
    std::vector<int> codepoints;
    float scale = 1.0f;
    int width = 0;
    int height = 0;
    int baseline = 0;
};

std::filesystem::path pngExporterExecutableDirectory();
const PngLabelFont *pngLabelFont(bool atlasSerif = false);
PngTextMetrics measurePngText(const PngLabelFont &font, std::string_view text, float pixelHeight);
void drawPngText(std::vector<Pixel> &pixels, int width, int height,
                 int left, int top, const PngLabelFont &font,
                 const PngTextMetrics &metrics, Pixel color);
void drawPngTextWithHalo(std::vector<Pixel> &pixels, int width, int height,
                         int left, int top, const PngLabelFont &font,
                         const PngTextMetrics &metrics, Pixel textColor,
                         Pixel haloColor, int haloRadius);
}
