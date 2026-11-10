#include "LegacySurfaceRegistry.hpp"

#include <unordered_set>

namespace gm::ux
{
const std::vector<LegacySurfaceRegistration> &legacySurfaceRegistry()
{
    // This is the executable retirement contract for the UI rebuild. Adding a
    // legacy route requires deciding its single replacement and removal gate.
    static const std::vector<LegacySurfaceRegistration> registry = {
        {"show:player_creator", "Player Creator", "PlayerCreatorController", WorkspaceTarget::Cast, LegacySurfaceDisposition::ContextualAdapter, "Character creation wizard", "Create/edit/archive parity in Cast", true},
        {"show:player_overview", "Player Overview", "PlayerOverviewController", WorkspaceTarget::Cast, LegacySurfaceDisposition::ContextualAdapter, "Party roster", "Roster filters and party management in Cast", true},
        {"show:character_sheet", "Character Sheets", "CharacterSheetController", WorkspaceTarget::Cast, LegacySurfaceDisposition::ContextualAdapter, "Selected-character inspector", "Summary, Sheet, Inventory, Magic, Story and Advanced tabs", true},
        {"show:npc_creator", "NPC Creator", "NPCCreatorController", WorkspaceTarget::Cast, LegacySurfaceDisposition::ContextualAdapter, "Person/creature creation wizard", "Create/edit/archive parity in Cast", true},
        {"show:npc_overview", "NPC and Monster Overview", "NPCOverviewController", WorkspaceTarget::Cast, LegacySurfaceDisposition::ContextualAdapter, "Cast directory", "Person, Creature and Monster directory parity", true},

        {"show:item_creator", "Item Creator", "ItemCreatorController", WorkspaceTarget::Compendium, LegacySurfaceDisposition::ContextualAdapter, "Embedded Compendium item editor", "Production create/edit/archive and EN/DE return-navigation interaction coverage", true},
        {"show:spell_library", "Spell Library", "SpellLibraryController", WorkspaceTarget::Compendium, LegacySurfaceDisposition::ContextualAdapter, "Embedded Compendium spell directory and detail", "Production draft retention, SQLite import/delete retry and read-only catalog selection", true},
        {"show:audio_manager", "Audio Manager", "AudioManagerController", WorkspaceTarget::Compendium, LegacySurfaceDisposition::ContextualAdapter, "Audio library", "Library, cue, playlist and asset-repair parity", true},
        {"show:inventory_manager", "Inventory Manager", "InventoryManagerController", WorkspaceTarget::Cast, LegacySurfaceDisposition::RetiredNavigation, "Context inventory tab", "Inventories available on characters, NPCs, shops, encounters and Live", true},

        {"show:map_creator", "Map Creator", "MapCreatorController", WorkspaceTarget::World, LegacySurfaceDisposition::ContextualAdapter, "Maps: Edit mode", "Shared map canvas edit parity", true},
        {"show:map_viewer", "Map Viewer", "MapViewerController", WorkspaceTarget::World, LegacySurfaceDisposition::ContextualAdapter, "Maps: View/Travel modes", "Shared map canvas view, fog and travel parity", true},
        {"show:dungeon_builder", "Dungeon Builder", "DungeonBuilderController", WorkspaceTarget::World, LegacySurfaceDisposition::RetiredNavigation, "Dungeon map inspector", "Rooms and connections editable as map structures", true},
        {"show:shop_manager", "Shop Manager", "ShopManagerController", WorkspaceTarget::World, LegacySurfaceDisposition::RetiredNavigation, "Shop entity inventory/transactions", "Shop CRUD, stock and transactions in World", true},

        {"show:encounter_builder", "Encounter Builder", "EncounterBuilderController", WorkspaceTarget::Encounters, LegacySurfaceDisposition::ContextualAdapter, "Encounter: Prepare", "Roster, difficulty, map, positions, notes and readiness parity", true},
        {"show:battlefield_creator", "Battlefield Creator", "BattlefieldCreatorController", WorkspaceTarget::Encounters, LegacySurfaceDisposition::RetiredNavigation, "Encounter map setup", "Shared map canvas and starting-position parity", true},
        {"show:battle_tracker", "Battle Tracker", "BattleTrackerController", WorkspaceTarget::Live, LegacySurfaceDisposition::ContextualAdapter, "Live: Combat", "Initiative, turns, resources and condition runtime parity", true},

        {"show:story_workspace", "Story Workspace", "StoryWorkspaceController", WorkspaceTarget::Story, LegacySurfaceDisposition::ContextualAdapter, "Scene board and inspector", "Arc board, timeline, links and Live staging parity", true},

        {"show:live_session_dashboard", "Live Dashboard", "LiveSessionDashboardController", WorkspaceTarget::Live, LegacySurfaceDisposition::RetiredNavigation, "Phase-aware Live surface", "Embedded Setup through Wrap-up surface is the only Live dashboard route", true},
        {"show:gm_screen", "GM Screen", "GMScreenController", WorkspaceTarget::Live, LegacySurfaceDisposition::RetiredNavigation, "Live context inspector", "Notes, public preview, rules references and phase controls are reached through Live", true},
        {"show:presentation_panel", "Presentation Panel", "", WorkspaceTarget::Live, LegacySurfaceDisposition::RetiredNavigation, "Player-display preview panel", "Public content control embedded in Live", true},
        {"panel:game_time", "Game Time", "", WorkspaceTarget::Live, LegacySurfaceDisposition::RetiredNavigation, "Clock panel", "Clock and calendar controls embedded in Live", true},
        {"panel:persistent_mini_map", "Mini Map", "", WorkspaceTarget::Live, LegacySurfaceDisposition::RetiredNavigation, "Context map panel", "Phase-relevant map preview embedded in Live", true},
        {"show:audio_quick_controls", "Audio Quick Controls", "AudioQuickControlsController", WorkspaceTarget::Live, LegacySurfaceDisposition::RetiredNavigation, "Compact mixer and cues", "Playback, volume groups and cues embedded in Live", true},
        {"panel:audio_mini_player", "Audio Mini", "", WorkspaceTarget::Live, LegacySurfaceDisposition::RetiredNavigation, "Compact mixer", "Persistent playback state controlled from Live", true},
        {"show:local_player_display", "Local Player Display", "LocalPlayerDisplayController", WorkspaceTarget::Live, LegacySurfaceDisposition::DetachedOutput, "Detachable player-safe output", "Keep as the sole supported detachable output", true},

        {"show:global_search", "Global Search", "", WorkspaceTarget::Home, LegacySurfaceDisposition::RetiredNavigation, "Universal Ctrl+K surface", "Entity, action and navigation results in one surface", true},
        {"show:command_palette", "Command Palette", "", WorkspaceTarget::Home, LegacySurfaceDisposition::RetiredNavigation, "Universal Ctrl+K surface", "Entity, action and navigation results in one surface", true},
        {"show:knowledge_graph", "Knowledge Graph", "", WorkspaceTarget::Settings, LegacySurfaceDisposition::RetiredNavigation, "Inspector relationships and backlinks", "Readable names, backlinks and related entities in context", true},
    };
    return registry;
}

const LegacySurfaceRegistration *findLegacySurface(std::string_view routeId)
{
    for (const auto &surface : legacySurfaceRegistry())
    {
        if (surface.routeId == routeId)
            return &surface;
    }
    return nullptr;
}

std::vector<const LegacySurfaceRegistration *> legacySurfacesFor(WorkspaceTarget workspace)
{
    std::vector<const LegacySurfaceRegistration *> result;
    for (const auto &surface : legacySurfaceRegistry())
    {
        if (surface.replacementWorkspace == workspace)
            result.push_back(&surface);
    }
    return result;
}

bool isLegacyEntryPointAllowed(const LegacySurfaceRegistration &surface, LegacyEntryPoint entryPoint)
{
    if (entryPoint == LegacyEntryPoint::PrimaryNavigation || entryPoint == LegacyEntryPoint::UniversalSearch)
        return false;
    return surface.disposition == LegacySurfaceDisposition::ContextualAdapter ||
           surface.disposition == LegacySurfaceDisposition::DetachedOutput;
}

bool isLegacyEntryPointAllowed(std::string_view routeId, LegacyEntryPoint entryPoint)
{
    const auto *surface = findLegacySurface(routeId);
    return surface && isLegacyEntryPointAllowed(*surface, entryPoint);
}

LegacySurfaceRegistryValidation validateLegacySurfaceRegistry()
{
    LegacySurfaceRegistryValidation result;
    std::unordered_set<std::string_view> ids;
    std::size_t detachedOutputCount = 0;

    for (const auto &surface : legacySurfaceRegistry())
    {
        if (surface.routeId.empty() || !ids.insert(surface.routeId).second)
            result.errors.emplace_back("Legacy surface route IDs must be non-empty and unique");
        if (surface.legacyName.empty() || surface.replacementMode.empty() || surface.parityGate.empty())
            result.errors.emplace_back(std::string(surface.routeId) + ": replacement and parity metadata are required");
        if (!surface.persistedDataPreserved)
            result.errors.emplace_back(std::string(surface.routeId) + ": UI retirement must not delete persisted data");
        if (surface.disposition == LegacySurfaceDisposition::DetachedOutput)
            ++detachedOutputCount;
    }

    if (detachedOutputCount != 1)
        result.errors.emplace_back("Exactly one legacy surface may remain a detached output");
    result.valid = result.errors.empty();
    return result;
}

std::string_view workspaceTargetName(WorkspaceTarget workspace)
{
    switch (workspace)
    {
    case WorkspaceTarget::Home: return "Home";
    case WorkspaceTarget::Story: return "Story";
    case WorkspaceTarget::Cast: return "Cast";
    case WorkspaceTarget::World: return "World";
    case WorkspaceTarget::Encounters: return "Encounters";
    case WorkspaceTarget::Compendium: return "Compendium";
    case WorkspaceTarget::Live: return "Live";
    case WorkspaceTarget::Settings: return "Settings";
    }
    return "Unknown";
}
}
