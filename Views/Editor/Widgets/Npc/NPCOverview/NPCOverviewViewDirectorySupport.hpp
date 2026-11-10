#pragma once
#include "NPCOverviewViewSpellSupport.hpp"

namespace
{
void appendCueText(std::string &text, const std::string &part)
{
    if (part.empty())
        return;
    if (!text.empty())
        text += " | ";
    text += part;
}

std::string npcDirectorySearchText(const Campaign &campaign, const NPCModel &npc)
{
    std::string text;
    appendSearchText(text, npc.name);
    appendSearchText(text, npcCategoryLabel(npc.category));
    appendSearchText(text, factionDisplayName(campaign, npc));
    appendSearchText(text, npc.faction);
    appendSearchText(text, npc.role);
    appendSearchText(text, npc.attitudeTowardParty);
    appendSearchText(text, npc.threatLocation);
    appendSearchText(text, npc.description);
    appendSearchText(text, npc.goals);
    appendSearchText(text, npc.resources);
    appendSearchText(text, npc.relationships);
    appendSearchText(text, npc.dialogueNotes);
    appendSearchText(text, "threat " + std::to_string(npc.threatLevel));
    if (npcStoryReferenceCount(campaign, npc.metadata.id, "NPC") > 0)
        appendSearchText(text, "story linked");
    if (npcEncounterReferenceCount(campaign, npc.metadata.id) > 0)
        appendSearchText(text, "combat linked");
    for (const std::string &tag : npc.metadata.tags)
        appendSearchText(text, tag);
    return text;
}

bool npcIsPlayerVisible(const Campaign &campaign, const NPCModel &npc)
{
    return containsEntityId(campaign.localPlayerDisplay.visibleNpcIds, npc.metadata.id);
}

bool npcHasLiveRelevance(const Campaign &campaign, const NPCModel &npc)
{
    return npcIsPlayerVisible(campaign, npc) ||
           npcEncounterReferenceCount(campaign, npc.metadata.id) > 0 ||
           !npc.conditions.empty() ||
           !npc.resourceTrackers.empty() ||
           npc.spellcasting.enabled;
}

std::string npcTableCue(const NPCModel &npc)
{
    std::string cue;
    appendCueText(cue, npc.role);
    appendCueText(cue, npc.attitudeTowardParty);
    appendCueText(cue, npc.threatLocation);
    return cue.empty() ? "-" : cue;
}

std::string npcPublicStateLabel(const Campaign &campaign, const NPCModel &npc)
{
    if (!npcIsPlayerVisible(campaign, npc))
        return localizedUiLabel("Private");
    if (campaign.localPlayerDisplay.mapOnly || !campaign.localPlayerDisplay.showVisibleNpcs)
        return localizedUiLabel("Hidden");
    return uiTextForLocale("Staged", "Bereitgestellt");
}

std::string npcPublicStateReason(const Campaign &campaign, const NPCModel &npc)
{
    if (!npcIsPlayerVisible(campaign, npc))
        return localizedUiLabel("Not included in the player-safe NPC list.");
    if (campaign.localPlayerDisplay.mapOnly)
        return uiTextForLocale("Selected, but map-only mode excludes NPC text.", "Ausgewählt, aber der reine Kartenmodus schließt NSC-Text aus.");
    if (!campaign.localPlayerDisplay.showVisibleNpcs)
        return localizedUiLabel("Selected for players, but the NPCs toggle is off on the local player display.");
    return uiTextForLocale("Staged as name and role only. Review and publish to update the player window; this status does not confirm its current contents.",
        "Nur Name und Rolle sind bereitgestellt. Prüfen und veröffentlichen aktualisiert das Spielerfenster; dieser Status bestätigt nicht dessen aktuellen Inhalt.");
}

std::string npcLiveHookLabel(const Campaign &campaign, const NPCModel &npc)
{
    const int encounterCount = npcEncounterReferenceCount(campaign, npc.metadata.id);
    if (encounterCount > 0)
        return localizedUiLabel("Combat") + " ×" + std::to_string(encounterCount);
    if (!npc.conditions.empty())
        return localizedUiLabel("Condition");
    if (!npc.resourceTrackers.empty())
        return localizedUiLabel("Resource");
    if (npc.spellcasting.enabled)
        return localizedUiLabel("Magic");
    if (npcIsPlayerVisible(campaign, npc))
        return localizedUiLabel("Player");
    return localizedUiLabel("No live hooks");
}

const char *npcThreatFilterLabel(int filter)
{
    switch (filter)
    {
    case 0:
        return "Threat 0-2";
    case 1:
        return "Threat 3-5";
    case 2:
        return "Threat 6-10";
    default:
        return "Any threat";
    }
}

bool npcMatchesThreatFilter(int threatLevel, int filter)
{
    switch (filter)
    {
    case 0:
        return threatLevel >= 0 && threatLevel <= 2;
    case 1:
        return threatLevel >= 3 && threatLevel <= 5;
    case 2:
        return threatLevel >= 6;
    default:
        return true;
    }
}

std::string npcPlayerSafeLine(const NPCModel &npc)
{
    const std::string name = npc.name.empty() ? localizedUiLabel("<Unnamed NPC>") : npc.name;
    if (npc.role.empty())
        return name;
    return name + " - " + npc.role;
}

bool npcMatchesDirectoryFilters(
    const Campaign &campaign,
    const NPCModel &npc,
    const std::string &query,
    int categoryFilter,
    int threatFilter,
    bool showArchived,
    bool playerVisibleOnly,
    bool liveRelevantOnly,
    bool storyLinkedOnly,
    bool combatLinkedOnly,
    bool archived)
{
    if (!showArchived && archived)
        return false;
    if (categoryFilter >= 0 && static_cast<int>(npc.category) != categoryFilter)
        return false;
    if (!npcMatchesThreatFilter(npc.threatLevel, threatFilter))
        return false;
    if (playerVisibleOnly && !npcIsPlayerVisible(campaign, npc))
        return false;
    if (liveRelevantOnly && !npcHasLiveRelevance(campaign, npc))
        return false;
    if (storyLinkedOnly && npcStoryReferenceCount(campaign, npc.metadata.id, "NPC") == 0)
        return false;
    if (combatLinkedOnly && npcEncounterReferenceCount(campaign, npc.metadata.id) == 0)
        return false;

    const std::string normalizedQuery = trimLowerCopy(query);
    if (normalizedQuery.empty())
        return true;

    return trimLowerCopy(npcDirectorySearchText(campaign, npc)).find(normalizedQuery) != std::string::npos;
}
}
