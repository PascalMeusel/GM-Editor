#include "Models/Map/PngExport/PngRasterCanvas.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace MapPngExportRaster
{
Pixel alphaBlend(Pixel destination, Pixel source, std::uint8_t opacity)
{
    const int sourceAlpha = static_cast<int>(source.a) * opacity / 255;
    const int inverse = 255 - sourceAlpha;
    return {
        static_cast<std::uint8_t>((source.r * sourceAlpha + destination.r * inverse) / 255),
        static_cast<std::uint8_t>((source.g * sourceAlpha + destination.g * inverse) / 255),
        static_cast<std::uint8_t>((source.b * sourceAlpha + destination.b * inverse) / 255),
        255};
}

void putPixel(std::vector<Pixel> &pixels, int width, int height, int x, int y, Pixel color)
{
    if (x < 0 || y < 0 || x >= width || y >= height)
        return;
    pixels[static_cast<std::size_t>(y * width + x)] = color;
}

void fillRect(std::vector<Pixel> &pixels, int width, int height,
              int x0, int y0, int x1, int y1, Pixel color)
{
    for (int y = std::max(0, y0); y < std::min(height, y1); ++y)
        for (int x = std::max(0, x0); x < std::min(width, x1); ++x)
            putPixel(pixels, width, height, x, y, color);
}

void fillCircle(std::vector<Pixel> &pixels, int width, int height,
                int centerX, int centerY, int radius, Pixel color)
{
    const int radiusSquared = radius * radius;
    for (int y = centerY - radius; y <= centerY + radius; ++y)
        for (int x = centerX - radius; x <= centerX + radius; ++x)
            if ((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY) <= radiusSquared)
                putPixel(pixels, width, height, x, y, color);
}

void fillTriangle(std::vector<Pixel> &pixels, int width, int height,
                  int ax, int ay, int bx, int by, int cx, int cy, Pixel color)
{
    const int minX = std::min({ax, bx, cx});
    const int maxX = std::max({ax, bx, cx});
    const int minY = std::min({ay, by, cy});
    const int maxY = std::max({ay, by, cy});
    const auto edge = [](int x0, int y0, int x1, int y1, int x, int y) {
        return (x - x0) * (y1 - y0) - (y - y0) * (x1 - x0);
    };
    const int orientation = edge(ax, ay, bx, by, cx, cy);
    for (int y = minY; y <= maxY; ++y)
        for (int x = minX; x <= maxX; ++x)
        {
            const int e0 = edge(ax, ay, bx, by, x, y);
            const int e1 = edge(bx, by, cx, cy, x, y);
            const int e2 = edge(cx, cy, ax, ay, x, y);
            if ((orientation >= 0 && e0 >= 0 && e1 >= 0 && e2 >= 0) ||
                (orientation < 0 && e0 <= 0 && e1 <= 0 && e2 <= 0))
                putPixel(pixels, width, height, x, y, color);
        }
}

void blendTriangle(std::vector<Pixel> &pixels, int width, int height,
                   int ax, int ay, int bx, int by, int cx, int cy, Pixel color)
{
    const int minX = std::max(0, std::min({ax, bx, cx}));
    const int maxX = std::min(width - 1, std::max({ax, bx, cx}));
    const int minY = std::max(0, std::min({ay, by, cy}));
    const int maxY = std::min(height - 1, std::max({ay, by, cy}));
    const auto edge = [](int x0, int y0, int x1, int y1, int x, int y) {
        return (x - x0) * (y1 - y0) - (y - y0) * (x1 - x0);
    };
    const int orientation = edge(ax, ay, bx, by, cx, cy);
    for (int y = minY; y <= maxY; ++y)
        for (int x = minX; x <= maxX; ++x)
        {
            const int e0 = edge(ax, ay, bx, by, x, y);
            const int e1 = edge(bx, by, cx, cy, x, y);
            const int e2 = edge(cx, cy, ax, ay, x, y);
            if ((orientation >= 0 && e0 >= 0 && e1 >= 0 && e2 >= 0) ||
                (orientation < 0 && e0 <= 0 && e1 <= 0 && e2 <= 0))
            {
                Pixel &destination = pixels[static_cast<std::size_t>(y * width + x)];
                destination = alphaBlend(destination, color, 255);
            }
        }
}

void drawLine(std::vector<Pixel> &pixels, int width, int height,
              int x0, int y0, int x1, int y1, int thickness, Pixel color)
{
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;)
    {
        fillCircle(pixels, width, height, x0, y0, std::max(1, thickness / 2), color);
        if (x0 == x1 && y0 == y1)
            break;
        const int doubled = error * 2;
        if (doubled >= dy)
        {
            error += dy;
            x0 += sx;
        }
        if (doubled <= dx)
        {
            error += dx;
            y0 += sy;
        }
    }
}

void blendCircle(std::vector<Pixel> &pixels, int width, int height,
                 int centerX, int centerY, int radius, Pixel color)
{
    const int safeRadius = std::max(0, radius);
    for (int y = centerY - safeRadius; y <= centerY + safeRadius; ++y)
        for (int x = centerX - safeRadius; x <= centerX + safeRadius; ++x)
        {
            if (x < 0 || y < 0 || x >= width || y >= height)
                continue;
            const int deltaX = x - centerX;
            const int deltaY = y - centerY;
            if (deltaX * deltaX + deltaY * deltaY > safeRadius * safeRadius)
                continue;
            Pixel &destination = pixels[static_cast<std::size_t>(y * width + x)];
            destination = alphaBlend(destination, color, 255);
        }
}

void drawCartographicLine(std::vector<Pixel> &pixels, int width, int height,
                          int x0, int y0, int x1, int y1,
                          int thickness, Pixel color)
{
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    const int radius = thickness <= 1 ? 0 : std::max(1, (thickness - 1) / 2);
    for (;;)
    {
        blendCircle(pixels, width, height, x0, y0, radius, color);
        if (x0 == x1 && y0 == y1)
            break;
        const int doubled = error * 2;
        if (doubled >= dy)
        {
            error += dy;
            x0 += sx;
        }
        if (doubled <= dx)
        {
            error += dx;
            y0 += sy;
        }
    }
}

void drawCartographicPolylineOnce(std::vector<Pixel> &pixels, int width, int height,
                                  const std::vector<std::pair<int, int>> &points,
                                  int thickness, Pixel color)
{
    if (points.size() < 2 || width <= 0 || height <= 0 || thickness <= 0 || color.a == 0)
        return;
    const int radius = thickness <= 1 ? 0 : std::max(1, (thickness - 1) / 2);
    int minX = points.front().first, maxX = minX;
    int minY = points.front().second, maxY = minY;
    for (const auto &point : points)
    {
        minX = std::min(minX, point.first);
        maxX = std::max(maxX, point.first);
        minY = std::min(minY, point.second);
        maxY = std::max(maxY, point.second);
    }
    const int left = std::max(0, minX - radius);
    const int top = std::max(0, minY - radius);
    const int right = std::min(width - 1, maxX + radius);
    const int bottom = std::min(height - 1, maxY + radius);
    if (left > right || top > bottom)
        return;
    const int maskWidth = right - left + 1;
    std::vector<std::uint8_t> coverage(static_cast<std::size_t>(maskWidth) * (bottom - top + 1), 0);
    const auto coverDisc = [&](int centerX, int centerY) {
        for (int y = std::max(top, centerY - radius); y <= std::min(bottom, centerY + radius); ++y)
            for (int x = std::max(left, centerX - radius); x <= std::min(right, centerX + radius); ++x)
            {
                const int dx = x - centerX, dy = y - centerY;
                if (dx * dx + dy * dy <= radius * radius)
                    coverage[static_cast<std::size_t>(y - top) * maskWidth + x - left] = 1;
            }
    };
    for (std::size_t index = 1; index < points.size(); ++index)
    {
        int x0 = points[index - 1].first, y0 = points[index - 1].second;
        const int x1 = points[index].first, y1 = points[index].second;
        if (std::max(x0, x1) + radius < left || std::min(x0, x1) - radius > right ||
            std::max(y0, y1) + radius < top || std::min(y0, y1) - radius > bottom)
            continue;
        const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;
        for (;;)
        {
            coverDisc(x0, y0);
            if (x0 == x1 && y0 == y1)
                break;
            const int doubled = error * 2;
            if (doubled >= dy)
            {
                error += dy;
                x0 += sx;
            }
            if (doubled <= dx)
            {
                error += dx;
                y0 += sy;
            }
        }
    }
    for (int y = top; y <= bottom; ++y)
        for (int x = left; x <= right; ++x)
            if (coverage[static_cast<std::size_t>(y - top) * maskWidth + x - left])
            {
                Pixel &destination = pixels[static_cast<std::size_t>(y) * width + x];
                destination = alphaBlend(destination, color, 255);
            }
}

void drawCartographicCircle(std::vector<Pixel> &pixels, int width, int height,
                            int centerX, int centerY, int radius,
                            int thickness, Pixel color)
{
    constexpr int segmentCount = 32;
    constexpr float tau = 6.28318530718f;
    for (int segment = 0; segment < segmentCount; ++segment)
    {
        const float angleA = tau * static_cast<float>(segment) / static_cast<float>(segmentCount);
        const float angleB = tau * static_cast<float>(segment + 1) / static_cast<float>(segmentCount);
        drawCartographicLine(
            pixels, width, height,
            centerX + static_cast<int>(std::lround(std::cos(angleA) * radius)),
            centerY + static_cast<int>(std::lround(std::sin(angleA) * radius)),
            centerX + static_cast<int>(std::lround(std::cos(angleB) * radius)),
            centerY + static_cast<int>(std::lround(std::sin(angleB) * radius)),
            thickness, color);
    }
}
}
