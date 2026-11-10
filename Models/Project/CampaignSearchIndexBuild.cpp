#include "CampaignSearchIndex.hpp"

namespace
{
std::string joinList(const std::vector<std::string> &values)
{
    std::string result;
    for (const std::string &value : values)
    {
        if (!value.empty())
        {
            if (!result.empty()) result += " ";
            result += value;
        }
    }
    return result;
}

void append(std::string &target, const std::string &value)
{
    if (value.empty()) return;
    if (!target.empty()) target += "\n";
    target += value;
}

std::string fallbackTitle(const std::string &title, const std::string &fallback)
{
    return title.empty() ? fallback : title;
}

std::string actionHintForType(const std::string &type)
{
    if (type == "Player") return "Open Character Sheets or Player Overview, edit tags, favorite, or inspect backlinks.";
    if (type == "NPC" || type == "Monster" || type == "Faction" || type == "Deity") return "Open NPC & Monster Overview, edit tags, favorite, or inspect backlinks.";
    if (type == "Spell") return "Open Spell Library, copy the reference ID, edit tags, favorite, or inspect backlinks.";
    if (type == "Story Arc" || type == "Story Beat") return "Open Story Workspace, edit tags, favorite, or inspect backlinks.";
    if (type == "Map") return "Open Map Viewer, edit tags, favorite, or inspect backlinks.";
    if (type == "Encounter") return "Open Encounter Builder, edit tags, favorite, or inspect backlinks.";
    if (type == "Item") return "Open Item Catalog, edit tags, favorite, or inspect backlinks.";
    if (type == "Random Table") return "Open Compendium to edit or roll this table, edit tags, favorite, or inspect backlinks.";
    if (type == "Shop") return "Open the World shop inspector, edit tags, favorite, or inspect backlinks.";
    if (type == "Dungeon") return "Open the World dungeon structure inspector, edit tags, favorite, or inspect backlinks.";
    if (type == "Audio") return "Open Audio Library, edit tags, favorite, or inspect backlinks.";
    if (type == "Session") return "Open Live Session Dashboard, edit tags, favorite, or inspect backlinks.";
    if (type == "Objective") return "Open the saved objective in Live table tools.";
    if (type == "Clue") return "Open the saved clue in Live table tools.";
    return "Open the related workspace, edit tags, favorite, or inspect backlinks.";
}

const char *objectiveStatusLabel(StoryObjectiveStatus status)
{
    switch (status)
    {
    case StoryObjectiveStatus::Planned: return "Planned";
    case StoryObjectiveStatus::Active: return "Active";
    case StoryObjectiveStatus::Completed: return "Completed";
    case StoryObjectiveStatus::Failed: return "Failed";
    }
    return "Objective";
}

const char *clueStatusLabel(StoryClueStatus status)
{
    switch (status)
    {
    case StoryClueStatus::Hidden: return "Hidden";
    case StoryClueStatus::Discovered: return "Discovered";
    case StoryClueStatus::Shared: return "Shared";
    }
    return "Clue";
}

void addDocument(std::vector<CampaignSearchDocument> &documents, const EntityId &id,
                 const std::string &type, const std::string &title, const std::string &subtitle,
                 const std::string &body, const std::vector<std::string> &tags, const std::string &actionHint = "")
{
    if (!id.empty())
        documents.push_back(CampaignSearchDocument{id, type, title, subtitle, body, tags,
                                                    actionHint.empty() ? actionHintForType(type) : actionHint});
}
} // namespace

void CampaignSearchIndex::rebuild(const Campaign &campaign)
{
    _documents.clear();
    _documents.reserve(campaign.characters.size() + campaign.npcs.size() + campaign.customMonsters.size() +
                       campaign.customItems.size() + campaign.customSpells.size() + campaign.maps.size() +
                       campaign.encounters.size() + campaign.storyThreads.size() + campaign.sessions.size() +
                       campaign.audioAssets.size() + campaign.shops.size() + campaign.dungeons.size() +
                       campaign.randomTables.size() + campaign.storyObjectives.size() + campaign.storyClues.size());

    for (const CharacterModel &character : campaign.characters)
    {
        std::string body;
        append(body, character.selectedClass);
        append(body, character.selectedRace);
        append(body, character.background);
        append(body, character.backstory);
        append(body, character.playerNotes);
        append(body, character.gmNotes);
        addDocument(_documents, character.metadata.id, "Player", fallbackTitle(character.nameBuffer, "Unnamed character"), "Player character", body, character.metadata.tags);
    }

    for (const NPCModel &npc : campaign.npcs)
    {
        std::string body;
        append(body, npc.description);
        append(body, npc.faction);
        append(body, npc.role);
        append(body, npc.attitudeTowardParty);
        append(body, npc.secrets);
        append(body, npc.goals);
        append(body, npc.relationships);
        append(body, npc.dialogueNotes);
        addDocument(_documents, npc.metadata.id, "NPC", fallbackTitle(npc.name, "Unnamed NPC"), npcCategoryLabel(npc.category), body, npc.metadata.tags);
    }

    for (const MonsterDefinitionModel &monster : campaign.customMonsters)
    {
        std::string body;
        append(body, monster.type);
        append(body, monster.challengeRating);
        append(body, monster.traits);
        append(body, monster.reactions);
        append(body, monster.legendaryActions);
        append(body, monster.lairActions);
        append(body, joinList(monster.encounterRoleTags));
        addDocument(_documents, monster.metadata.id, "Monster", fallbackTitle(monster.name, "Unnamed monster"), monster.source, body, monster.metadata.tags);
    }

    for (const ItemModel &item : campaign.customItems)
    {
        std::string body;
        append(body, item.description);
        append(body, item.itemType);
        append(body, item.weaponDamage);
        append(body, item.weaponDamageType);
        append(body, joinList(item.weaponProperties));
        append(body, joinList(item.effects));
        addDocument(_documents, item.metadata.id, "Item", fallbackTitle(item.name, "Unnamed item"), getItemCategory(item), body, item.metadata.tags);
    }

    for (const SpellModel &spell : campaign.customSpells)
    {
        std::string body;
        append(body, std::string("Use: ") + spellResolutionLabel(spell.resolution));
        append(body, spell.castingTime + " " + spell.range + " " + spell.duration);
        append(body, spell.description);
        append(body, spell.effectSummary);
        append(body, spell.classes);
        append(body, spell.damage);
        append(body, spell.healing);
        append(body, spell.area);
        append(body, spell.scaling);
        addDocument(_documents,
                    spell.metadata.id,
                    "Spell",
                    fallbackTitle(spell.name, "Unnamed spell"),
                    spellLevelLabel(spell.level) + " / " + spellResolutionLabel(spell.resolution),
                    body,
                    spell.metadata.tags);
    }

    for (const RandomTableModel &table : campaign.randomTables)
    {
        std::string body;
        append(body, table.description);
        append(body, table.selectionMode == RandomTableSelectionMode::DiceRange ? table.diceExpression : "Weighted entries");
        for (const RandomTableEntryModel &entry : table.entries)
        {
            append(body, entry.result);
            append(body, entry.gmNotes);
            append(body, entry.linkedEntityLabel);
        }
        addDocument(_documents, table.metadata.id, "Random Table", fallbackTitle(table.name, "Unnamed random table"),
                    std::to_string(table.entries.size()) + " entries", body, table.metadata.tags);
    }

    for (const MapModel &map : campaign.maps)
    {
        std::string body;
        append(body, map.description);
        append(body, map.backgroundImagePath);
        for (const MapRegionModel &region : map.regions)
            append(body, region.name + " " + region.description);
        for (const MapFeatureModel &feature : map.features)
            append(body, feature.name + " " + feature.description + " " + feature.gmNotes);
        for (const MapRouteModel &route : map.routes)
            append(body, route.name + " " + route.terrain + " " + route.notes);
        addDocument(_documents, map.metadata.id, "Map", fallbackTitle(map.name, "Unnamed map"), mapScopeLabel(map.scope), body, map.metadata.tags);
    }

    for (const EncounterModel &encounter : campaign.encounters)
    {
        std::string body;
        append(body, encounter.description);
        append(body, encounter.mapContextName);
        append(body, encounter.afterCombatSummary);
        append(body, encounter.afterCombatLootNotes);
        append(body, encounter.afterCombatStoryConsequences);
        append(body, encounter.afterCombatSessionNotes);
        for (const EncounterParticipantModel &participant : encounter.participants)
            append(body, participant.displayName + " " + participant.groupName + " " + joinList(participant.conditions));
        addDocument(_documents, encounter.metadata.id, "Encounter", fallbackTitle(encounter.name, "Unnamed encounter"), encounter.theaterOfMind ? "Theater of mind" : "Battlefield", body, encounter.metadata.tags);
    }

    for (const StoryThreadModel &thread : campaign.storyThreads)
    {
        std::string body;
        append(body, thread.summary);
        append(body, thread.stakes);
        append(body, thread.gmNotes);
        for (const StoryLinkModel &link : thread.links)
            append(body, link.label + " " + link.targetEntityType);
        addDocument(_documents,
                    thread.metadata.id,
                    "Story Arc",
                    fallbackTitle(thread.title, "Untitled story"),
                    std::string(storyThreadTypeLabel(thread.type)) + " / " + storyStatusLabel(thread.status),
                    body,
                    thread.metadata.tags);

        for (const StoryBeatModel &beat : thread.beats)
        {
            std::string beatBody;
            append(beatBody, std::string("Status: ") + storyStatusLabel(beat.status));
            append(beatBody, beat.summary);
            append(beatBody, beat.playerSummary);
            append(beatBody, beat.readAloudText);
            append(beatBody, beat.clues);
            append(beatBody, beat.consequences);
            append(beatBody, beat.gmNotes);
            append(beatBody, beat.inWorldDate);
            append(beatBody, thread.title);
            for (const StoryLinkModel &link : beat.links)
                append(beatBody, link.label + " " + link.targetEntityType);
            addDocument(_documents,
                        beat.metadata.id,
                        "Story Beat",
                        fallbackTitle(beat.title, "Untitled beat"),
                        "Scene in " + fallbackTitle(thread.title, "Untitled story") + " / " + storyStatusLabel(beat.status),
                        beatBody,
                        beat.metadata.tags);
        }
    }

    for (const SessionModel &session : campaign.sessions)
    {
        std::string body;
        append(body, session.summary);
        append(body, session.roughNotes);
        for (const SessionPlanEntry &entry : session.planEntries)
            append(body, entry.prepNote);
        append(body, session.recap);
        append(body, session.unresolvedThreads);
        append(body, session.lootAwarded);
        append(body, session.xpMilestones);
        append(body, session.npcsMet);
        append(body, session.locationsVisited);
        append(body, session.openHooks);
        for (const SessionNoteModel &note : session.notes)
            append(body, note.body + " " + note.linkedEntityLabel);
        addDocument(_documents, session.metadata.id, "Session", fallbackTitle(session.title, "Untitled session"), session.scheduledAtUtc, body, session.metadata.tags);
    }

    for (const StoryObjectiveModel &objective : campaign.storyObjectives)
    {
        std::string body;
        append(body, objective.playerSummary);
        append(body, objective.gmNotes);
        append(body, objective.playerVisible ? "Visible to players" : "GM-only");
        addDocument(_documents, objective.metadata.id, "Objective",
                    fallbackTitle(objective.title, "Untitled objective"),
                    std::string("Table objective / ") + objectiveStatusLabel(objective.status), body, objective.metadata.tags);
    }

    for (const StoryClueModel &clue : campaign.storyClues)
    {
        std::string body;
        append(body, clue.playerText);
        append(body, clue.gmTruth);
        append(body, clue.sourceEntityType);
        append(body, clue.discoveredAtUtc);
        append(body, clue.playerVisible ? "Visible to players" : "GM-only");
        addDocument(_documents, clue.metadata.id, "Clue",
                    fallbackTitle(clue.title, "Untitled clue"),
                    std::string("Table clue / ") + clueStatusLabel(clue.status), body, clue.metadata.tags);
    }

    for (const AudioAssetModel &asset : campaign.audioAssets)
    {
        std::string body;
        append(body, asset.originalFileName);
        append(body, asset.originalSourcePath);
        append(body, asset.filePath);
        addDocument(_documents, asset.metadata.id, "Audio", fallbackTitle(asset.name, "Unnamed audio"), audioAssetTypeLabel(asset.type), body, asset.metadata.tags);
    }

    for (const ShopModel &shop : campaign.shops)
    {
        std::string body;
        append(body, shop.merchantName);
        append(body, shop.description);
        append(body, shop.factionReputationNotes);
        addDocument(_documents, shop.metadata.id, "Shop", fallbackTitle(shop.name, "Unnamed shop"), shop.merchantName, body, shop.metadata.tags);
    }

    for (const DungeonModel &dungeon : campaign.dungeons)
    {
        std::string body;
        append(body, dungeon.description);
        append(body, dungeon.gmNotes);
        for (const DungeonRoomModel &room : dungeon.rooms)
            append(body, room.name + " " + room.description + " " + room.keyedDescription + " " + room.readAloudText + " " + room.secrets + " " + room.traps);
        addDocument(_documents, dungeon.metadata.id, "Dungeon", fallbackTitle(dungeon.name, "Unnamed dungeon"), "Dungeon", body, dungeon.metadata.tags);
    }
}
