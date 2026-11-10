#pragma once
#include <array>
// Native vector assets in the existing illustrated atlas palette. Coordinates
// describe fractured rock shelves, not mountain silhouettes rotated sideways.
// The renderer fits these banks around the authoritative road/river curve.
namespace gm::map_render
{
struct MountainCrossingAsset
{
    std::array<float, 7> ridge;
    float depth;
};
inline constexpr std::array<MountainCrossingAsset, 6> mountainCrossingAssets{
    {{{{.54f, .64f, .71f, .65f, .77f, .61f, .54f}}, .32f},
     {{{.54f, .58f, .66f, .73f, .63f, .55f, .54f}}, .38f},
     {{{.54f, .71f, .64f, .78f, .72f, .62f, .54f}}, .29f},
     {{{.54f, .62f, .75f, .69f, .61f, .67f, .54f}}, .42f},
     {{{.54f, .66f, .59f, .73f, .68f, .57f, .54f}}, .35f},
     {{{.54f, .59f, .71f, .64f, .77f, .63f, .54f}}, .31f}}};
} // namespace gm::map_render
