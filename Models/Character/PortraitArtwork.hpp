#pragma once
#include "Models/Character/PortraitRecipe.hpp"

namespace gm::portraits
{
inline constexpr const char *collectionFolders[] = {"Distinct", "Company"};
static_assert(std::size(collectionFolders) == currentCollection + 1);
// Each sheet has four columns and two rows. Every cell is a distinct identity.
// The persisted expression field is intentionally reused as the identity index
// so existing campaigns remain readable without a schema migration.
inline constexpr const char *artworkFiles[] = {
    "human.png",
    "elf.png",
    "dwarf.png",
    "orc.png",
    "tiefling.png",
    "dragonborn.png",
    "tabaxi.png",
    "undead.png",
    "aasimar.png",
    "gnome.png",
    "goliath.png",
    "halfling.png",
    "half_elf.png",
    "half_orc.png",
    "goblin.png",
    "hobgoblin.png",
    "bugbear.png",
    "kobold.png",
    "aarakocra.png",
    "kenku.png",
    "tortle.png",
    "lizardfolk.png",
    "firbolg.png",
    "genasi.png",
    "gith.png",
    "triton.png",
    "yuan_ti.png",
    "changeling.png",
    "shifter.png",
    "warforged.png",
    "satyr.png",
    "fairy.png",
    "harengon.png",
    "owlin.png",
    "leonin.png",
    "minotaur.png",
    "centaur.png",
    "loxodon.png",
    "giff.png",
    "thri_kreen.png",
    "plasmoid.png",
    "autognome.png",
    "hadozee.png",
    "kalashtar.png",
    "verdan.png",
    "locathah.png",
    "grung.png",
};
static_assert(std::size(artworkFiles) == speciesCount, "Every species needs a painted sheet");
struct ArtworkRegion { float u0, v0, u1, v1; };
inline ArtworkRegion artworkRegion(int identity, int width = 1024, int height = 1024)
{
    identity = std::clamp(identity, 0, 7);
    const float u = (identity % 4) * .25f;
    const float v = (identity / 4) * .5f;
    const float aspect = float(std::max(1, width)) / (2.f * std::max(1, height));
    constexpr float targetAspect = 2.f / 3.f;
    const float cropWidth = .25f * std::min(1.f, targetAspect / aspect);
    const float cropHeight = .5f * std::min(1.f, aspect / targetAspect);
    const float left = u + (.25f - cropWidth) * .5f;
    // Preserve anatomy without stretching. Top-aligned crops retain the crown;
    // a small inset avoids neighbouring cells when sampling thumbnails.
    return {left + .001f, v + .001f, left + cropWidth - .001f, v + cropHeight - .001f};
}
}
