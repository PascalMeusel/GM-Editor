#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace MapPngExportRaster
{
struct Pixel
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

Pixel alphaBlend(Pixel destination, Pixel source, std::uint8_t opacity);
void putPixel(std::vector<Pixel> &pixels, int width, int height, int x, int y, Pixel color);
void fillRect(std::vector<Pixel> &pixels, int width, int height,
              int x0, int y0, int x1, int y1, Pixel color);
void fillCircle(std::vector<Pixel> &pixels, int width, int height,
                int centerX, int centerY, int radius, Pixel color);
void fillTriangle(std::vector<Pixel> &pixels, int width, int height,
                  int ax, int ay, int bx, int by, int cx, int cy, Pixel color);
void blendTriangle(std::vector<Pixel> &pixels, int width, int height,
                   int ax, int ay, int bx, int by, int cx, int cy, Pixel color);
void drawLine(std::vector<Pixel> &pixels, int width, int height,
              int x0, int y0, int x1, int y1, int thickness, Pixel color);
void blendCircle(std::vector<Pixel> &pixels, int width, int height,
                 int centerX, int centerY, int radius, Pixel color);
void drawCartographicLine(std::vector<Pixel> &pixels, int width, int height,
                          int x0, int y0, int x1, int y1, int thickness, Pixel color);
void drawCartographicPolylineOnce(std::vector<Pixel> &pixels, int width, int height,
                                  const std::vector<std::pair<int, int>> &points,
                                  int thickness, Pixel color);
void drawCartographicCircle(std::vector<Pixel> &pixels, int width, int height,
                            int centerX, int centerY, int radius,
                            int thickness, Pixel color);
}
