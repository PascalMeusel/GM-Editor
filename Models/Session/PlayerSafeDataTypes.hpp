#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <string>
#include <vector>

struct PlayerSafeMapToken
{
    std::string name;
    MapTokenKind kind = MapTokenKind::Marker;
    float x = 0.0f;
    float y = 0.0f;
    float size = 1.0f;
};

struct PlayerSafeMapFeature
{
    MapFeatureKind kind = MapFeatureKind::Pin;
    std::string name;
    std::string description;
    int x = 0;
    int y = 0;
};

struct PlayerSafeInitiativeEntry
{
    std::string displayName;
    int initiative = 0;
    bool active = false;
};

struct PlayerSafeNpcSummary
{
    std::string name;
    std::string role;
};

// This character summary intentionally omits notes, backstory, inventory,
// portrait/file paths, and arbitrary model metadata. Optional inventory
// sharing uses the separate, narrower PlayerSafeInventoryItemSummary contract.
struct PlayerSafeCharacterSummary
{
    EntityId entityId;
    std::string name;
    std::string characterClass;
    int level = 0;
    int currentHitPoints = 0;
    int maxHitPoints = 0;
    int temporaryHitPoints = 0;
    int speedFeet = 0;
    std::vector<std::string> conditions;
    struct SpellSlot { int level = 0; int available = 0; int maximum = 0; };
    std::vector<SpellSlot> spellSlots;
};

// Deliberately omits item IDs, descriptions, notes, value, state, and assets.
struct PlayerSafeInventoryItemSummary
{
    // partyOwned distinguishes campaign stock from a character who happens to
    // be named "Party Inventory". ownerName is meaningful only otherwise.
    std::string ownerName;
    std::string itemName;
    int quantity = 0;
    bool partyOwned = false;
};

struct PlayerSafeStoryScene
{
    bool available = false;
    std::string title;
    std::string playerSummary;
    std::string readAloudText;
};

struct PlayerSafeHandout
{
    // The handout entity ID is the stable identity of its managed attachment.
    // Raw source paths are deliberately not part of this player-safe type.
    EntityId assetId;
    std::string title;
    std::string body;
    std::string projectRelativePath;
    std::string contentHash;
};
