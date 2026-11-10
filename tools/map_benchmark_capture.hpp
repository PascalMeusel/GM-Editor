#pragma once
#include <GLFW/glfw3.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

// Called after the measured run, on the application thread's main back buffer.
inline void captureMapBenchmarkFramebuffer(const std::filesystem::path &path)
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
    if (width <= 0 || height <= 0)
        throw std::runtime_error("No benchmark framebuffer");
    const std::size_t stride = (static_cast<std::size_t>(width) * 3 + 3) & ~std::size_t(3);
    std::vector<unsigned char> pixels(stride * height);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError() != GL_NO_ERROR)
        throw std::runtime_error("Benchmark framebuffer read failed");
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            std::swap(pixels[y * stride + x * 3], pixels[y * stride + x * 3 + 2]);
    std::ofstream bitmap(path, std::ios::binary);
    auto write = [&](std::uint32_t value, int bytes = 4) {
        for (int i = 0; i < bytes; ++i)
            bitmap.put(static_cast<char>((value >> (8 * i)) & 255));
    };
    bitmap.write("BM", 2);
    write(static_cast<std::uint32_t>(54 + pixels.size()));
    write(0);
    write(54);
    write(40);
    write(width);
    write(height);
    write(1, 2);
    write(24, 2);
    write(0);
    write(static_cast<std::uint32_t>(pixels.size()));
    write(2835);
    write(2835);
    write(0);
    write(0);
    bitmap.write(reinterpret_cast<const char *>(pixels.data()), pixels.size());
    if (!bitmap)
        throw std::runtime_error("Benchmark framebuffer write failed");
}
