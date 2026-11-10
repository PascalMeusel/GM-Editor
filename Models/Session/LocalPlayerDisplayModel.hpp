#pragma once

#include "Models/Core/EntityMetadata.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

struct LocalPlayerDisplayCameraState
{
    // Normalized coordinates keep the public camera stable when a map is
    // resized or rendered on a different monitor.
    float normalizedCenterX = 0.5f;
    float normalizedCenterY = 0.5f;
    float zoom = 1.0f;
    bool followGmCamera = true;
};

// Toolkit-independent description of a visible GM map canvas. Keeping the
// conversion here lets the editor and viewer publish the same camera semantics
// without granting either surface access to player-visible content.
struct LocalPlayerDisplayCanvasCameraInput
{
    float mapOriginX = 0.0f;
    float mapOriginY = 0.0f;
    float mapPixelWidth = 0.0f;
    float mapPixelHeight = 0.0f;
    float visibleMinimumX = 0.0f;
    float visibleMinimumY = 0.0f;
    float visibleMaximumX = 0.0f;
    float visibleMaximumY = 0.0f;
    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    float cellSize = 0.0f;
    int mapWidth = 0;
    int mapHeight = 0;
};

struct LocalPlayerDisplayPingState
{
    // Runtime-only. Pings are deliberately not restored after an application
    // restart or campaign reload.
    bool active = false;
    float normalizedX = 0.5f;
    float normalizedY = 0.5f;
    float remainingSeconds = 0.0f;
};

struct LocalPlayerDisplayModel
{
    EntityMetadata metadata{"local_player_display"};
    bool enabled = false;
    // Opening the detached window is not publication. Both values are
    // runtime-only and are reset to the fail-closed state on every load.
    bool published = false;
    bool blackout = true;
    bool mapOnly = true;
    // -1 selects a dedicated display automatically. The preference is kept
    // even while a display is disconnected so that reconnecting it restores
    // the intended table layout. Publication itself is never persisted.
    int preferredMonitorIndex = -1;
    bool fullscreenMapOnly = true;
    LocalPlayerDisplayCameraState publicCamera;
    LocalPlayerDisplayPingState ping;
    std::uint64_t publishedPayloadFingerprint = 0;
    // Runtime-only content review proof. Unlike publishedPayloadFingerprint,
    // this also changes when the selected map, scene, initiative, NPC, or
    // handout contents change after the GM reviewed them.
    std::uint64_t publishedReviewFingerprint = 0;
    EntityId publicSceneId;
    EntityId publicMapId;
    EntityId publicEncounterId;
    bool showMap = true;
    bool showInitiative = true;
    bool showVisibleNpcs = true;
    // Party summaries are an explicit opt-in channel. An empty selection never
    // reveals every campaign character by implication.
    bool showParty = false;
    // A separate GM opt-in: party identity/HP never implies spell-slot reveal.
    bool showPartySpellSlots = false;
    // Inventory is a separate, explicit reveal. Existing campaigns remain off.
    bool showPartyInventory = false;
    bool showPublicHandouts = true;
    bool showSharedArt = true;
    std::vector<EntityId> visibleNpcIds;
    std::vector<EntityId> visibleCharacterIds;
    std::vector<EntityId> publicHandoutIds;
    std::vector<EntityId> sharedArtAssetIds;
    // Presentation preference only: it is not reviewed content and therefore
    // deliberately stays out of payload/review/source fingerprints.
    float playerTextScale = 1.15f;
};

struct LocalPlayerDisplayWindowPlacement
{
    int monitorIndex = -1;
    bool fullscreen = false;
    bool fallbackToMainWindow = true;
};

// Resolve persisted monitor preferences without depending on a windowing
// backend. An unavailable explicit display always falls back to a normal
// window on the main monitor; it must never unexpectedly cover the GM screen.
inline LocalPlayerDisplayWindowPlacement resolveLocalPlayerDisplayWindowPlacement(
    const LocalPlayerDisplayModel &display,
    int monitorCount,
    int mainMonitorIndex)
{
    LocalPlayerDisplayWindowPlacement placement;
    if (monitorCount <= 0)
        return placement;

    const int safeMain = mainMonitorIndex >= 0 && mainMonitorIndex < monitorCount
                             ? mainMonitorIndex
                             : 0;
    if (display.preferredMonitorIndex >= 0)
    {
        if (display.preferredMonitorIndex >= monitorCount)
        {
            placement.monitorIndex = safeMain;
            return placement;
        }
        placement.monitorIndex = display.preferredMonitorIndex;
        placement.fullscreen = display.mapOnly && display.fullscreenMapOnly;
        placement.fallbackToMainWindow = false;
        return placement;
    }

    for (int monitorIndex = 0; monitorIndex < monitorCount; ++monitorIndex)
    {
        if (monitorIndex == safeMain)
            continue;
        placement.monitorIndex = monitorIndex;
        placement.fullscreen = display.mapOnly && display.fullscreenMapOnly;
        placement.fallbackToMainWindow = false;
        return placement;
    }

    placement.monitorIndex = safeMain;
    // Automatic selection with only the editor monitor available deliberately
    // remains windowed. The GM must explicitly select that monitor to allow a
    // full-screen player surface there.
    return placement;
}

// A newly selected public map always starts at its whole-map view. The renderer
// applies this persisted camera to its child scroll each frame, so stale map
// scroll offsets cannot carry to another map.
inline void resetLocalPlayerDisplayCameraForMapChange(LocalPlayerDisplayCameraState &camera)
{
    camera = {};
    camera.followGmCamera = false;
}

inline void normalizeLocalPlayerDisplayCamera(LocalPlayerDisplayCameraState &camera)
{
    camera.normalizedCenterX = std::isfinite(camera.normalizedCenterX)
                                   ? std::clamp(camera.normalizedCenterX, 0.0f, 1.0f)
                                   : 0.5f;
    camera.normalizedCenterY = std::isfinite(camera.normalizedCenterY)
                                   ? std::clamp(camera.normalizedCenterY, 0.0f, 1.0f)
                                   : 0.5f;
    camera.zoom = std::isfinite(camera.zoom) ? std::clamp(camera.zoom, 0.5f, 8.0f) : 1.0f;
}

// Convert the actually visible part of an editor canvas into the normalized
// public-camera representation. Invalid/bootstrap ImGui dimensions fail closed
// and leave the caller's previous camera untouched.
inline bool localPlayerDisplayCameraFromCanvas(
    const LocalPlayerDisplayCanvasCameraInput &input,
    LocalPlayerDisplayCameraState &camera)
{
    const auto finitePositive = [](float value) {
        return std::isfinite(value) && value > 0.0f;
    };
    if (input.mapWidth <= 0 || input.mapHeight <= 0 ||
        !std::isfinite(input.mapOriginX) || !std::isfinite(input.mapOriginY) ||
        !finitePositive(input.mapPixelWidth) || !finitePositive(input.mapPixelHeight) ||
        !std::isfinite(input.visibleMinimumX) || !std::isfinite(input.visibleMinimumY) ||
        !std::isfinite(input.visibleMaximumX) || !std::isfinite(input.visibleMaximumY) ||
        input.visibleMaximumX <= input.visibleMinimumX ||
        input.visibleMaximumY <= input.visibleMinimumY ||
        !finitePositive(input.viewportWidth) || !finitePositive(input.viewportHeight) ||
        !finitePositive(input.cellSize))
        return false;

    const float fitCellSize = std::min(
        input.viewportWidth / static_cast<float>(input.mapWidth),
        input.viewportHeight / static_cast<float>(input.mapHeight));
    if (!finitePositive(fitCellSize))
        return false;

    const float visibleCenterX = (input.visibleMinimumX + input.visibleMaximumX) * 0.5f;
    const float visibleCenterY = (input.visibleMinimumY + input.visibleMaximumY) * 0.5f;
    LocalPlayerDisplayCameraState captured;
    captured.normalizedCenterX = (visibleCenterX - input.mapOriginX) / input.mapPixelWidth;
    captured.normalizedCenterY = (visibleCenterY - input.mapOriginY) / input.mapPixelHeight;
    captured.zoom = input.cellSize / fitCellSize;
    captured.followGmCamera = true;
    normalizeLocalPlayerDisplayCamera(captured);
    camera = captured;
    return true;
}

inline std::uint64_t localPlayerDisplayPayloadFingerprint(const LocalPlayerDisplayModel &display)
{
    // FNV-1a is sufficient here: this is a runtime change detector, not a
    // security signature. Camera and ping are intentionally excluded so that
    // a GM can move the public camera without republishing content.
    std::uint64_t hash = 1469598103934665603ull;
    const auto appendByte = [&](unsigned char value) {
        hash ^= static_cast<std::uint64_t>(value);
        hash *= 1099511628211ull;
    };
    const auto appendString = [&](const std::string &value) {
        for (unsigned char character : value)
            appendByte(character);
        appendByte(0xffu);
    };
    const auto appendBool = [&](bool value) { appendByte(value ? 1u : 0u); };
    const auto appendIds = [&](const std::vector<EntityId> &ids) {
        for (const EntityId &id : ids)
            appendString(id);
        appendByte(0xfeu);
    };

    appendBool(display.mapOnly);
    appendString(display.publicSceneId);
    appendString(display.publicMapId);
    appendString(display.publicEncounterId);
    appendBool(display.showMap);
    appendBool(display.showInitiative);
    appendBool(display.showVisibleNpcs);
    appendBool(display.showParty);
    appendBool(display.showPartySpellSlots);
    appendBool(display.showPartyInventory);
    appendBool(display.showPublicHandouts);
    appendBool(display.showSharedArt);
    appendIds(display.visibleNpcIds);
    appendIds(display.visibleCharacterIds);
    appendIds(display.publicHandoutIds);
    appendIds(display.sharedArtAssetIds);
    return hash;
}

inline void blackoutLocalPlayerDisplay(LocalPlayerDisplayModel &display)
{
    display.blackout = true;
    display.published = false;
    display.publishedPayloadFingerprint = 0;
    display.publishedReviewFingerprint = 0;
    display.ping = {};
}

// Removing a character must also revoke an explicit party reveal. This keeps
// a deleted ID from surviving in saved player-display state or a publication.
inline bool removeLocalPlayerDisplayCharacterSelection(LocalPlayerDisplayModel &display, const EntityId &characterId)
{
    if (characterId.empty())
        return false;
    const auto previousEnd = display.visibleCharacterIds.end();
    const auto newEnd = std::remove(display.visibleCharacterIds.begin(), previousEnd, characterId);
    if (newEnd == previousEnd)
        return false;
    display.visibleCharacterIds.erase(newEnd, previousEnd);
    blackoutLocalPlayerDisplay(display);
    return true;
}

inline void prepareLocalPlayerDisplayForApplicationStart(LocalPlayerDisplayModel &display)
{
    // Window visibility belongs to this editor session. Retain the GM's
    // selections and placement preferences, but never reopen a saved window.
    display.enabled = false;
    if (display.preferredMonitorIndex < -1)
        display.preferredMonitorIndex = -1;
    if (!display.mapOnly)
        display.fullscreenMapOnly = false;
    normalizeLocalPlayerDisplayCamera(display.publicCamera);
    display.playerTextScale = std::isfinite(display.playerTextScale)
                                  ? std::clamp(display.playerTextScale, 1.0f, 1.8f)
                                  : 1.15f;
    blackoutLocalPlayerDisplay(display);
}

inline void armLocalPlayerDisplay(LocalPlayerDisplayModel &display)
{
    display.enabled = true;
    blackoutLocalPlayerDisplay(display);
}

inline void disableLocalPlayerDisplay(LocalPlayerDisplayModel &display)
{
    display.enabled = false;
    blackoutLocalPlayerDisplay(display);
}

inline bool publishLocalPlayerDisplay(LocalPlayerDisplayModel &display)
{
    if (!display.enabled)
        return false;
    normalizeLocalPlayerDisplayCamera(display.publicCamera);
    display.publishedPayloadFingerprint = localPlayerDisplayPayloadFingerprint(display);
    display.published = true;
    display.blackout = false;
    return true;
}

inline bool localPlayerDisplayIsPublished(const LocalPlayerDisplayModel &display)
{
    return display.enabled && display.published && !display.blackout &&
           display.publishedPayloadFingerprint != 0 &&
           display.publishedPayloadFingerprint == localPlayerDisplayPayloadFingerprint(display);
}
