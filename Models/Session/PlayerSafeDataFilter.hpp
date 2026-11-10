#pragma once

#include "Models/Campaign/Campaign.hpp"
#include "Models/Session/TabletopToolkit/TabletopToolkitService.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "Models/Session/PlayerSafeDataTypes.hpp"

inline std::vector<PlayerSafeInventoryItemSummary> buildPlayerSafeInventorySummaries(const Campaign &campaign);

inline bool playerSafeContainsId(const std::vector<EntityId> &ids, const EntityId &id)
{
    // Empty/stale entity IDs can occur in legacy or partially imported data.
    // They are never an explicit reveal and therefore fail closed.
    return !id.empty() && std::find(ids.begin(), ids.end(), id) != ids.end();
}

inline const MapModel *findPlayerSafePublishedMap(const Campaign &campaign, const EntityId &mapId)
{
    if (mapId.empty())
        return nullptr;
    const auto found = std::find_if(campaign.maps.begin(), campaign.maps.end(), [&](const MapModel &map) {
        return map.metadata.id == mapId && map.visibility == MapVisibility::PlayersVisible;
    });
    return found == campaign.maps.end() ? nullptr : &*found;
}

inline bool playerSafeMapBackgroundArtIsFullyRevealed(const MapModel &map);

inline bool localPlayerDisplayHasPublicPayload(const Campaign &campaign)
{
    const LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    if (display.showMap && findPlayerSafePublishedMap(campaign, display.publicMapId) != nullptr)
        return true;
    if (display.showSharedArt)
    {
        const MapModel *map = findPlayerSafePublishedMap(campaign, display.publicMapId);
        const bool hasManagedArt = map && map->importedReference.enabled &&
                                   (!map->importedReference.workingProxyProjectRelativePath.empty() ||
                                    !map->importedReference.projectRelativePath.empty());
        // An unmanaged legacy path carries no explicit publication consent.
        // It must first be adopted as a managed reference and reviewed.
        if (hasManagedArt && playerSafeMapBackgroundArtIsFullyRevealed(*map))
            return true;
    }
    if (!display.publicSceneId.empty())
    {
        for (const StoryThreadModel &thread : campaign.storyThreads)
            if (std::any_of(thread.beats.begin(), thread.beats.end(), [&](const StoryBeatModel &beat) {
                    return beat.metadata.id == display.publicSceneId;
                }))
                return true;
    }
    if (display.showInitiative && !display.publicEncounterId.empty() &&
        std::any_of(campaign.encounters.begin(), campaign.encounters.end(), [&](const EncounterModel &encounter) {
            return encounter.metadata.id == display.publicEncounterId;
        }))
        return true;
    if (display.showVisibleNpcs && std::any_of(campaign.npcs.begin(), campaign.npcs.end(), [&](const NPCModel &npc) {
            return playerSafeContainsId(display.visibleNpcIds, npc.metadata.id);
        }))
        return true;
    if (display.showParty && (std::any_of(campaign.characters.begin(), campaign.characters.end(), [&](const CharacterModel &character) {
            return playerSafeContainsId(display.visibleCharacterIds, character.metadata.id);
        }) || !buildPlayerSafeInventorySummaries(campaign).empty()))
        return true;
    if (display.showPublicHandouts && std::any_of(campaign.handouts.begin(), campaign.handouts.end(), [&](const HandoutModel &handout) {
            return playerSafeContainsId(display.publicHandoutIds, handout.metadata.id);
        }))
        return true;
    if (!TabletopToolkit::playerSafeObjectives(campaign).empty() ||
        !TabletopToolkit::playerSafeClues(campaign).empty() ||
        !TabletopToolkit::playerSafeBoundaries(campaign).empty() ||
        !TabletopToolkit::playerSafeWarnings(campaign).empty())
        return true;
    return false;
}

inline bool playerSafeMapCellIsRevealed(const MapModel &map, int x, int y)
{
    if (x < 0 || y < 0 || x >= map.width || y >= map.height)
        return false;
    const std::size_t index = static_cast<std::size_t>(y * map.width + x);
    return index < map.cells.size() && !map.cells[index].fogged;
}

inline bool playerSafeMapPositionIsRevealed(const MapModel &map, float x, float y)
{
    if (!std::isfinite(x) || !std::isfinite(y))
        return false;
    return playerSafeMapCellIsRevealed(map,
                                       static_cast<int>(std::floor(x)),
                                       static_cast<int>(std::floor(y)));
}

// Applies a whole-map fog action only to the exact public map selected for the
// detached display. Malformed grids and non-public maps are rejected instead
// of being resized or implicitly published. Any real change invalidates the
// reviewed publication immediately.
inline bool setAllPublicDisplayMapFog(Campaign &campaign, bool fogged)
{
    LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    if (display.publicMapId.empty())
        return false;

    const auto found = std::find_if(campaign.maps.begin(), campaign.maps.end(), [&](const MapModel &map) {
        return map.metadata.id == display.publicMapId && map.visibility == MapVisibility::PlayersVisible;
    });
    if (found == campaign.maps.end() || found->width <= 0 || found->height <= 0)
        return false;

    const std::size_t width = static_cast<std::size_t>(found->width);
    const std::size_t height = static_cast<std::size_t>(found->height);
    if (height != 0 && width > (std::numeric_limits<std::size_t>::max)() / height)
        return false;
    const std::size_t requiredCells = width * height;
    if (found->cells.size() != requiredCells)
        return false;

    const bool changed = std::any_of(found->cells.begin(), found->cells.end(), [&](const MapCellModel &cell) {
        return cell.fogged != fogged;
    });
    if (!changed)
        return true;

    for (MapCellModel &cell : found->cells)
        cell.fogged = fogged;
    found->metadata.touch();
    ++found->revision;
    blackoutLocalPlayerDisplay(display);
    return true;
}

inline bool startPlayerSafeDisplayPing(Campaign &campaign,
                                       float normalizedX,
                                       float normalizedY,
                                       float durationSeconds = 3.0f)
{
    LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    if (!localPlayerDisplayIsPublished(display) || display.publishedReviewFingerprint == 0 || !display.showMap ||
        !std::isfinite(normalizedX) || !std::isfinite(normalizedY) ||
        normalizedX < 0.0f || normalizedX > 1.0f ||
        normalizedY < 0.0f || normalizedY > 1.0f ||
        !std::isfinite(durationSeconds) || durationSeconds <= 0.0f)
        return false;

    const MapModel *map = findPlayerSafePublishedMap(campaign, display.publicMapId);
    if (map == nullptr || map->width <= 0 || map->height <= 0)
        return false;

    // A normalized coordinate of exactly one denotes the far edge. Move it
    // infinitesimally into the final cell before applying the fog check.
    const float safeX = std::min(normalizedX, std::nextafter(1.0f, 0.0f));
    const float safeY = std::min(normalizedY, std::nextafter(1.0f, 0.0f));
    if (!playerSafeMapPositionIsRevealed(*map,
                                         safeX * static_cast<float>(map->width),
                                         safeY * static_cast<float>(map->height)))
        return false;

    display.ping.active = true;
    display.ping.normalizedX = normalizedX;
    display.ping.normalizedY = normalizedY;
    display.ping.remainingSeconds = durationSeconds;
    return true;
}

inline void advancePlayerSafeDisplayPing(LocalPlayerDisplayModel &display, float elapsedSeconds)
{
    if (!display.ping.active)
        return;
    if (!localPlayerDisplayIsPublished(display) || !std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0f)
    {
        display.ping = {};
        return;
    }
    display.ping.remainingSeconds = std::max(0.0f, display.ping.remainingSeconds - elapsedSeconds);
    if (display.ping.remainingSeconds <= 0.0f)
        display.ping = {};
}

inline bool playerSafeMapLayerIsVisible(const MapModel &map, const EntityId &layerId)
{
    if (layerId.empty())
        return true;
    const auto found = std::find_if(map.layers.begin(), map.layers.end(), [&](const MapLayerModel &layer) {
        return layer.metadata.id == layerId;
    });
    // A stale layer reference is ambiguous and must not become an accidental
    // reveal on the detached player-facing window.
    return found != map.layers.end() && found->visible;
}

inline bool playerSafeMapBackgroundArtIsFullyRevealed(const MapModel &map)
{
    if (map.visibility != MapVisibility::PlayersVisible)
        return false;
    if (map.width <= 0 || map.height <= 0)
        return false;
    const std::size_t requiredCells = static_cast<std::size_t>(map.width) * static_cast<std::size_t>(map.height);
    if (map.cells.size() < requiredCells)
        return false;
    return std::none_of(map.cells.begin(), map.cells.begin() + static_cast<std::ptrdiff_t>(requiredCells), [](const MapCellModel &cell) {
        return cell.fogged;
    });
}

inline std::vector<PlayerSafeMapToken> buildPlayerSafeMapTokens(const MapModel &map)
{
    std::vector<PlayerSafeMapToken> tokens;
    for (const MapTokenModel &token : map.tokens)
    {
        if (token.hiddenFromPlayers ||
            !playerSafeMapLayerIsVisible(map, token.layerId) ||
            !playerSafeMapPositionIsRevealed(map, token.x, token.y))
            continue;

        tokens.push_back({token.name, token.kind, token.x, token.y, token.size});
    }
    return tokens;
}

inline std::vector<PlayerSafeMapFeature> buildPlayerSafeMapFeatures(const MapModel &map)
{
    std::vector<PlayerSafeMapFeature> features;
    for (const MapFeatureModel &feature : map.features)
    {
        if (feature.gmOnly || !feature.discovered ||
            !playerSafeMapCellIsRevealed(map, feature.x, feature.y))
            continue;

        features.push_back({feature.kind, feature.name, feature.description, feature.x, feature.y});
    }
    return features;
}

inline std::vector<PlayerSafeInitiativeEntry> buildPlayerSafeInitiative(const EncounterModel &encounter)
{
    std::vector<PlayerSafeInitiativeEntry> entries;
    for (int index = 0; index < static_cast<int>(encounter.participants.size()); ++index)
    {
        const EncounterParticipantModel &participant = encounter.participants[static_cast<std::size_t>(index)];
        if (participant.startsHidden ||
            (participant.reinforcementRound > 0 && participant.reinforcementRound > encounter.round))
            continue;

        entries.push_back({participant.displayName, participant.initiative, index == encounter.activeParticipantIndex});
    }
    return entries;
}

inline PlayerSafeNpcSummary buildPlayerSafeNpcSummary(const NPCModel &npc)
{
    return {npc.name, npc.role};
}

inline std::vector<PlayerSafeNpcSummary> buildPlayerSafeNpcSummaries(const Campaign &campaign)
{
    std::vector<PlayerSafeNpcSummary> npcs;
    const LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    for (const NPCModel &npc : campaign.npcs)
    {
        if (!playerSafeContainsId(display.visibleNpcIds, npc.metadata.id))
            continue;

        npcs.push_back(buildPlayerSafeNpcSummary(npc));
    }
    return npcs;
}

inline std::vector<PlayerSafeCharacterSummary> buildPlayerSafeCharacterSummaries(const Campaign &campaign)
{
    std::vector<PlayerSafeCharacterSummary> characters;
    const LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    if (!display.showParty)
        return characters;

    for (const CharacterModel &character : campaign.characters)
    {
        if (!playerSafeContainsId(display.visibleCharacterIds, character.metadata.id))
            continue;
        PlayerSafeCharacterSummary summary{character.metadata.id,
                              character.nameBuffer,
                              character.selectedClass,
                              character.level,
                              character.currentHitPoints,
                              character.maxHitPoints,
                              character.temporaryHitPoints,
                              character.speedFeet,
                              character.conditions};
        if (display.showPartySpellSlots)
            for (std::size_t level = 1; level < std::min<std::size_t>(10, character.spellcasting.slotMaximums.size()); ++level)
            {
                const int maximum = character.spellcasting.slotMaximums[level];
                const int used = level < character.spellcasting.slotUsed.size() ? character.spellcasting.slotUsed[level] : 0;
                if (maximum > 0)
                    summary.spellSlots.push_back({static_cast<int>(level), maximum - std::clamp(used, 0, maximum), maximum});
            }
        characters.push_back(std::move(summary));
    }
    return characters;
}

inline void appendPlayerSafeInventorySummaries(std::vector<PlayerSafeInventoryItemSummary> &result,
    const InventoryModel &inventory, const std::vector<ItemModel> &items,
    const std::string &ownerName, bool partyOwned, const EntityId *selectedItemId = nullptr)
{
    for (const InventoryEntryModel &entry : inventory.entries)
    {
        if (selectedItemId && entry.itemId != *selectedItemId) continue;
        const ItemModel *item = entry.quantity > 0 ? findItemById(items, entry.itemId) : nullptr;
        if (!item || item->name.empty()) continue;
        result.push_back({ownerName, item->name, entry.quantity, partyOwned});
    }
}

inline std::vector<PlayerSafeInventoryItemSummary> buildPlayerSafeInventorySummaries(const Campaign &campaign)
{
    std::vector<PlayerSafeInventoryItemSummary> result;
    const LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    if (!display.showParty || !display.showPartyInventory)
        return result;
    std::vector<ItemModel> items;
    if (campaign.ruleSet) items = campaign.ruleSet->getItems();
    items.insert(items.end(), campaign.customItems.begin(), campaign.customItems.end());
    for (const CharacterModel &character : campaign.characters)
        if (playerSafeContainsId(display.visibleCharacterIds, character.metadata.id))
            appendPlayerSafeInventorySummaries(result, character.inventory, items, character.nameBuffer, false);
    appendPlayerSafeInventorySummaries(result, campaign.partyInventory, items, {}, true);
    return result;
}

inline PlayerSafeStoryScene buildPlayerSafeStoryScene(const Campaign &campaign)
{
    const EntityId &sceneId = campaign.localPlayerDisplay.publicSceneId;
    if (sceneId.empty())
        return {};

    for (const StoryThreadModel &thread : campaign.storyThreads)
    {
        for (const StoryBeatModel &beat : thread.beats)
        {
            if (beat.metadata.id == sceneId)
                return {true, beat.title, beat.playerSummary, beat.readAloudText};
        }
    }
    return {};
}

inline std::vector<PlayerSafeHandout> buildPlayerSafeHandouts(const Campaign &campaign)
{
    std::vector<PlayerSafeHandout> handouts;
    const LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    for (const HandoutModel &handout : campaign.handouts)
    {
        if (!playerSafeContainsId(display.publicHandoutIds, handout.metadata.id))
            continue;

        // File identity and bytes are bound only while creating a reviewed
        // publication snapshot. A generic projection must never carry an
        // absolute or otherwise unreviewed local path into player UI code.
        handouts.push_back({handout.metadata.id, handout.title, handout.body, {}, {}});
    }
    return handouts;
}
