#pragma once

#include "Models/Campaign/Campaign.hpp"

namespace MapWorldExpansion
{
constexpr int maximumCellsPerSide = 120;

// Growth is east/south only: existing cell coordinates and local-map anchors
// retain their physical position. Imported images need a separate crop/extend
// workflow and are deliberately excluded here.
bool available(const MapModel &world, int eastCells, int southCells);
bool apply(Campaign &campaign, const EntityId &worldId, int eastCells, int southCells);
}
