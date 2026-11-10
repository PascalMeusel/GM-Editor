#pragma once
#include <GLFW/glfw3.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <algorithm>

namespace gm::qa
{
// Lossless capture of a QA-owned framebuffer. The caller owns and
// isolates the window; this helper does not inspect any other application.
inline void captureFramebuffer(GLFWwindow *window, const std::filesystem::path &path, bool front = false)
{
    struct RestoreContext {
        GLFWwindow *previous = glfwGetCurrentContext();
        ~RestoreContext() { glfwMakeContextCurrent(previous); }
    } restore;
    glfwMakeContextCurrent(window);
    int width, height; glfwGetFramebufferSize(window, &width, &height);
    if (width <= 0 || height <= 0) throw std::runtime_error("No QA framebuffer");
    const int stride = (width * 3 + 3) & ~3;
    std::vector<unsigned char> pixels(stride * height);
    glPixelStorei(GL_PACK_ALIGNMENT, 4); glReadBuffer(front ? GL_FRONT : GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("QA framebuffer read failed");
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
        std::swap(pixels[y * stride + x * 3], pixels[y * stride + x * 3 + 2]);
    std::ofstream out(path, std::ios::binary);
    const auto u16 = [&](unsigned n) { out.put(char(n)); out.put(char(n >> 8)); };
    const auto u32 = [&](unsigned n) { u16(n); u16(n >> 16); };
    out.write("BM", 2); u32(54 + unsigned(pixels.size())); u32(0); u32(54); u32(40);
    u32(width); u32(height); u16(1); u16(24); u32(0); u32(unsigned(pixels.size()));
    u32(2835); u32(2835); u32(0); u32(0);
    out.write(reinterpret_cast<const char *>(pixels.data()), pixels.size());
    if (!out) throw std::runtime_error("Could not write QA capture");
}
}
