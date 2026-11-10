#pragma once
#include "Models/Campaign/Campaign.hpp"
#include "Models/Map/MapStructuredCreation.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "Models/Session/InWorldTimeService.hpp"
#include <cstdio>

namespace gm::qa
{
inline constexpr const char *secretMarker = "GM_ONLY_SECRET_7319";
inline constexpr const char *publicMarker = "PUBLIC_REVIEW_4826";
inline void identify(EntityMetadata &metadata, const std::string &id)
{
    metadata.id = "qa-session-v1/" + id;
    metadata.createdAtUtc = metadata.updatedAtUtc = "2026-01-15T18:00:00Z";
}

// Generated source data only: no campaign database, personal notes or new art.
inline Campaign threeSceneCampaign(bool german = false)
{
    Campaign campaign;
    identify(campaign.metadata, "campaign");
    identify(campaign.localPlayerDisplay.metadata, "player-display");
    identify(campaign.partyTravel.metadata, "party-travel");
    campaign.name = german ? "Drei Szenen am Fluss" : "Three scenes by the river";
    campaign.ruleSet = std::make_shared<DND5e>();
    campaign.description = german ? "Generierte technische Testkampagne." : "Generated engineering test campaign.";
    InWorldTime::setDateTime(campaign.clock, {1492, 4, 12, 9, 0}, false);
    campaign.clock.anchorUtc = "2026-01-15T18:00:00Z";
    identify(campaign.partyInventory.metadata, "party-inventory");
    campaign.partyInventory.name = german ? "Gruppenvorräte" : "Party supplies";

    ItemModel blade;
    identify(blade.metadata, "blade");
    blade.name = german ? "Wachschwert" : "Watch sword";
    blade.category = "Weapon";
    blade.itemType = "Weapon";
    blade.weaponDamage = "1d8";
    blade.weaponDamageType = "Slashing";
    blade.baseValue = 1500;
    ItemModel potion;
    identify(potion.metadata, "potion");
    potion.name = german ? "Heiltrank" : "Healing draught";
    potion.category = "Adventuring Gear";
    potion.consumable = true;
    potion.stackable = true;
    potion.baseValue = 5000;
    campaign.customItems = {blade, potion};

    SpellModel spell;
    identify(spell.metadata, "spell");
    spell.name = german ? "Lichtfunke" : "Light spark";
    spell.level = 1;
    spell.description = "Generated spell for resource persistence.";
    spell.resolution = SpellResolutionType::SpellAttack;
    spell.damage = "1d6";
    spell.metadata.customProperties["source"] = "Generated QA content";
    campaign.customSpells.push_back(spell);

    for (int index = 0; index < 2; ++index)
    {
        CharacterModel character;
        identify(character.metadata, "character-" + std::to_string(index));
        std::snprintf(character.nameBuffer, sizeof(character.nameBuffer), "%s", index ? "Mira" : "Arin");
        character.selectedClass = index ? "Wizard" : "Fighter";
        character.selectedRace = "Human";
        character.level = 3;
        character.maxHitPoints = character.currentHitPoints = index ? 18 : 28;
        for (const auto &attribute : campaign.ruleSet->getAttributes())
            character.attributes[attribute] = 12;
        character.attributes[index ? "Intelligence" : "Strength"] = 16;
        character.gmNotes = secretMarker;
        character.playerNotes = publicMarker;
        identify(character.inventory.metadata, "inventory-" + std::to_string(index));
        character.inventory.ownerEntityId = character.metadata.id;
        InventoryEntryModel entry(index ? potion.metadata.id : blade.metadata.id, index ? 2 : 1, !index);
        identify(entry.metadata, "inventory-entry-" + std::to_string(index));
        character.inventory.entries.push_back(entry);
        ResourceTrackerModel resource;
        identify(resource.metadata, "resource-" + std::to_string(index));
        resource.name = "Resolve";
        resource.maximum = 2;
        character.resourceTrackers.push_back(resource);
        if (index)
        {
            character.spellcasting.enabled = true;
            character.spellcasting.slotMaximums[1] = 3;
            character.spellcasting.spells.push_back({spell.metadata.id, true, "Generated QA", {}});
        }
        campaign.characters.push_back(character);
    }

    FactionModel faction;
    identify(faction.metadata, "faction");
    faction.name = german ? "Flusswache" : "River watch";
    campaign.factions.push_back(faction);
    NPCModel merchant;
    identify(merchant.metadata, "merchant");
    merchant.name = "Lenna";
    merchant.factionId = faction.metadata.id;
    identify(merchant.inventory.metadata, "merchant-inventory");
    merchant.inventory.ownerEntityId = merchant.metadata.id;
    campaign.npcs.push_back(merchant);
    ShopModel shop;
    identify(shop.metadata, "shop");
    shop.name = german ? "Lennas Vorräte" : "Lenna's supplies";
    shop.merchantName = merchant.name;
    shop.stock.emplace_back(potion.metadata.id, 4);
    campaign.shops.push_back(shop);
    WorldPlaceModel place;
    identify(place.metadata, "place");
    place.name = "Riverford";
    place.metadata.customProperties["gm.context.shop"] = shop.metadata.id;
    campaign.worldPlaces.push_back(place);

    const MapCreationIntent intents[] = {MapCreationIntent::WorldOverview, MapCreationIntent::RegionExploration,
                                         MapCreationIntent::TacticalEncounter};
    for (int index = 0; index < 3; ++index)
    {
        MapModel map;
        applyMapCreationIntent(map, intents[index], MapSizePreset::Small);
        identify(map.metadata, "map-" + std::to_string(index));
        map.name = index == 0 ? "Riverlands" : index == 1 ? "Riverford region" : "Old bridge";
        map.width = index == 2 ? 12 : 24;
        map.height = index == 2 ? 10 : 18;
        map.visibility = MapVisibility::PlayersVisible;
        map.revision = 1;
        resizeMapCells(map);
        ensureDefaultMapLayer(map);
        for (std::size_t layer = 0; layer < map.layers.size(); ++layer)
            identify(map.layers[layer].metadata, "map-" + std::to_string(index) + "-layer-" + std::to_string(layer));
        for (int y = 0; y < map.height; ++y)
            for (int x = 0; x < map.width; ++x)
            {
                auto &cell = map.cells[static_cast<std::size_t>(y * map.width + x)];
                cell.kind = y == map.height / 2 ? MapTileKind::Water : MapTileKind::Dirt;
                if (x == map.width / 2)
                    cell.kind = y == map.height / 2 ? MapTileKind::Bridge : MapTileKind::Road;
            }
        MapTokenModel hidden;
        identify(hidden.metadata, "secret-token-" + std::to_string(index));
        hidden.name = secretMarker;
        hidden.hiddenFromPlayers = true;
        hidden.layerId = map.layers.front().metadata.id;
        hidden.x = 2;
        hidden.y = 2;
        map.tokens.push_back(hidden);
        campaign.maps.push_back(map);
    }

    EncounterModel encounter;
    identify(encounter.metadata, "encounter");
    encounter.name = german ? "Hinterhalt an der Brücke" : "Bridge ambush";
    encounter.mapId = campaign.maps[2].metadata.id;
    encounter.tacticalMapId = encounter.mapId;
    encounter.battlefieldWidth = campaign.maps[2].width;
    encounter.battlefieldHeight = campaign.maps[2].height;
    encounter.useMapCells = true;
    encounter.battlefieldCells = campaign.maps[2].cells;
    for (int index = 0; index < 2; ++index)
    {
        EncounterParticipantModel participant;
        identify(participant.metadata, "combatant-" + std::to_string(index));
        participant.kind = CombatantKind::Player;
        participant.sourceEntityId = campaign.characters[index].metadata.id;
        participant.displayName = campaign.characters[index].nameBuffer;
        participant.currentHitPoints = participant.maxHitPoints = campaign.characters[index].maxHitPoints;
        participant.friendly = true;
        participant.initiative = 15 - index * 5;
        participant.x = participant.startingX = 4 + index;
        participant.y = participant.startingY = 2;
        encounter.participants.push_back(participant);
    }
    campaign.encounters.push_back(encounter);

    StoryThreadModel thread;
    identify(thread.metadata, "thread");
    thread.title = german ? "Ein Tag am Fluss" : "A day by the river";
    thread.status = StoryStatus::Planned;
    SessionModel session;
    identify(session.metadata, "session");
    session.title = campaign.name;
    session.scheduledAtUtc = "2026-01-15T18:00:00Z";
    session.roughNotes = secretMarker;
    const char *titles[] = {"Arrival in Riverford", "Bridge ambush", "The old watchtower"};
    const char *germanTitles[] = {"Ankunft in Riverford", "Hinterhalt an der Brücke", "Der alte Wachturm"};
    for (int index = 0; index < 3; ++index)
    {
        StoryBeatModel scene;
        identify(scene.metadata, "scene-" + std::to_string(index));
        scene.title = german ? germanTitles[index] : titles[index];
        scene.status = StoryStatus::Planned;
        scene.playerSummary = publicMarker;
        scene.readAloudText =
            german ? "Der Fluss rauscht unter der alten Brücke." : "The river runs beneath the old bridge.";
        scene.gmNotes = secretMarker;
        scene.summary = "Generated preparation notes.";
        const auto link = [&](const EntityId &id, const char *type, const std::string &label) {
            StoryLinkModel value;
            identify(value.metadata, "scene-" + std::to_string(index) + "-link-" + std::to_string(scene.links.size()));
            value.targetEntityId = id;
            value.targetEntityType = type;
            value.label = label;
            scene.links.push_back(value);
        };
        link(campaign.maps[index].metadata.id, "Map", campaign.maps[index].name);
        link(campaign.characters[0].metadata.id, "Player", campaign.characters[0].nameBuffer);
        if (index == 0)
            link(merchant.metadata.id, "NPC", merchant.name);
        if (index == 1)
            link(encounter.metadata.id, "Encounter", encounter.name);
        thread.beats.push_back(scene);
        SessionPlanEntry entry;
        identify(entry.metadata, "plan-entry-" + std::to_string(index));
        entry.storyBeatId = scene.metadata.id;
        session.planEntries.push_back(entry);
    }
    session.participantCharacterIds = {campaign.characters[0].metadata.id, campaign.characters[1].metadata.id};
    session.encounterIds.push_back(encounter.metadata.id);
    campaign.storyThreads.push_back(thread);
    campaign.sessions.push_back(session);
    return campaign;
}
}
