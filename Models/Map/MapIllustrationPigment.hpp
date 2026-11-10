#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gm::map_render
{
// Smooth pigment variation in map coordinates. Neither viewport size nor
// private map features enter this sampler, so zooming and player filtering
// cannot change its pattern. Recipe 10 opts in; previous paintings stay pinned.
inline float illustrationPigmentNoise(unsigned seed, float x, float y)
{
    const int ix = static_cast<int>(std::floor(x));
    const int iy = static_cast<int>(std::floor(y));
    const auto lattice = [seed](int px, int py) {
        unsigned h = seed ^ static_cast<unsigned>(px) * 0x9e3779b9u ^
                     static_cast<unsigned>(py) * 0x85ebca6bu;
        h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
        return static_cast<float>(h & 0xffffu) / 32767.5f - 1.f;
    };
    float tx = x - ix, ty = y - iy;
    tx = tx * tx * (3.f - 2.f * tx);
    ty = ty * ty * (3.f - 2.f * ty);
    const float top = lattice(ix, iy) * (1.f - tx) + lattice(ix + 1, iy) * tx;
    const float bottom = lattice(ix, iy + 1) * (1.f - tx) + lattice(ix + 1, iy + 1) * tx;
    return top * (1.f - ty) + bottom * ty;
}

inline float illustrationPigmentWash(unsigned seed, float x, float y)
{
    return illustrationPigmentNoise(seed ^ 0x3e517u, x * .71f, y * .71f) * 1.8f +
           illustrationPigmentNoise(seed ^ 0x58b61u, x * .23f, y * .23f) * 2.6f;
}
} // namespace gm::map_render
