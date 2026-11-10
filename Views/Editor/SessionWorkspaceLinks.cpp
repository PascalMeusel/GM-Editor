#include "Views/Editor/SessionWorkspaceSupport.hpp"
#include "Views/Editor/SessionWorkspaceCommandPopup.hpp"

void SessionWorkspace::drawLinks(Campaign &campaign, const Actions &actions)
{
    const auto *session = SessionPlan::findSession(campaign, _sessionId);
    const bool readOnly = !session || !session->endedAtUtc.empty();
    ImGui::TextUnformatted(st("Linked material", "Verknüpfte Inhalte"));
    if (_sceneId.empty() || !SessionPlan::findScene(campaign, _sceneId))
    {
        ImGui::TextWrapped("%s", st("Select a scene to attach people, maps and encounters.", "Wähle eine Szene, um Figuren, Karten und Begegnungen zu verknüpfen."));
        return;
    }
    const auto links = _scene.links;
    for (std::size_t i = 0; i < links.size(); ++i)
    {
        const auto &link = links[i];
        const auto *entity = EntityRegistry::getInstance().find(link.targetEntityId);
        std::string name = entity ? entity->displayName : link.label;
        bool available = entity != nullptr;
        if (link.targetEntityType == "Handout")
            for (const auto &handout : campaign.handouts) if (handout.metadata.id == link.targetEntityId)
            { name = handout.title; available = true; break; }
        const char *kind = link.targetEntityType == "world_place" ? st("Place", "Ort") :
            link.targetEntityType == "Player" ? st("Character", "Figur") :
            link.targetEntityType == "NPC" ? st("NPC", "NSC") :
            link.targetEntityType == "Map" ? st("Map", "Karte") :
            link.targetEntityType == "Encounter" ? st("Encounter", "Begegnung") :
            gm::localization::trUi(link.targetEntityType.c_str());
        ImGui::PushID(link.metadata.id.c_str());
        ImGui::TextDisabled("%s", kind);
        ImGui::TextWrapped("%s", name.empty() ? st("Missing material", "Fehlender Inhalt") : name.c_str());
        if (!available) ImGui::TextWrapped("%s", st("This link is missing. Unlink it and attach the replacement.", "Dieses Ziel fehlt. Löse die Verknüpfung und füge einen Ersatz hinzu."));
        ImGui::BeginDisabled(!available);
        if (action("Open", "Öffnen") && flush(campaign, actions.persist) && actions.openEntity)
        {
            if (link.targetEntityType == "Handout") { _handoutId = link.targetEntityId; ImGui::OpenPopup("SessionHandout"); }
            else actions.openEntity(link.targetEntityType, link.targetEntityId);
        }
        ImGui::EndDisabled();
        if (ImGui::BeginPopup("SessionHandout"))
        {
            for (const auto &handout : campaign.handouts) if (handout.metadata.id == _handoutId)
            {
                ImGui::TextUnformatted(handout.title.c_str());
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 340);
                ImGui::TextUnformatted(handout.body.c_str());
                ImGui::Spacing();
                ImGui::TextDisabled("%s", st("GM notes", "SL-Notizen"));
                ImGui::TextUnformatted(handout.gmNotes.c_str());
                ImGui::PopTextWrapPos();
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(readOnly);
        const bool unlink = action("Unlink", "Lösen");
        ImGui::EndDisabled();
        if (unlink) { _scene.links.erase(_scene.links.begin() + static_cast<std::ptrdiff_t>(i)); changed(_sceneDirty); ImGui::PopID(); break; }
        ImGui::PopID();
        ImGui::Spacing();
    }
    ImGui::BeginDisabled(readOnly);
    if (action("+ Attach material", "+ Inhalt verknüpfen", true))
    {
        EntityRegistry::getInstance().rebuild(campaign);
        ImGui::OpenPopup("AttachSessionMaterial");
    }
    if (ImGui::BeginPopup("AttachSessionMaterial"))
    {
        ImGui::SetNextItemWidth(270);
        FormUi::InputTextWithHint("##material-filter", st("Find people, maps, encounters...", "Figuren, Karten, Begegnungen suchen..."), &_linkFilter);
        ImGui::BeginChild("MaterialResults", ImVec2(300, 280));
        auto materials = EntityRegistry::getInstance().entries();
        for (const auto &handout : campaign.handouts)
            materials.push_back({handout.metadata.id, "handout", handout.title, campaign.metadata.id, true});
        for (const auto &entity : materials)
        {
            const bool supported = entity.type == "character" || entity.type == "npc" || entity.type == "monster" || entity.type == "map" || entity.type == "encounter" || entity.type == "world_place" || entity.type == "audio" || entity.type == "item" || entity.type == "handout";
            if (!supported || !contains(entity.displayName + " " + entity.type, _linkFilter)) continue;
            const char *kind = entity.type == "world_place" ? st("Place", "Ort") :
                entity.type == "character" ? st("Character", "Figur") : entity.type == "npc" ? st("NPC", "NSC") :
                entity.type == "map" ? st("Map", "Karte") : entity.type == "encounter" ? st("Encounter", "Begegnung") :
                entity.type == "audio" ? st("Audio", "Audio") : entity.type == "item" ? st("Item", "Gegenstand") :
                entity.type == "monster" ? st("Monster", "Monster") : st("Handout", "Handout");
            if (ImGui::Selectable((entity.displayName + " · " + kind + "##" + entity.id).c_str()))
            {
                const bool exists = std::any_of(_scene.links.begin(), _scene.links.end(), [&](const StoryLinkModel &link) { return link.targetEntityId == entity.id; });
                if (!exists)
                {
                    StoryLinkModel link; link.label = entity.displayName; link.targetEntityId = entity.id;
                    link.targetEntityType = entity.type == "character" ? "Player" : entity.type == "npc" ? "NPC" :
                        entity.type == "map" ? "Map" : entity.type == "encounter" ? "Encounter" : entity.type == "audio" ? "Audio" :
                        entity.type == "item" ? "Item" : entity.type == "monster" ? "Monster" : entity.type == "handout" ? "Handout" : "world_place";
                    _scene.links.push_back(std::move(link)); changed(_sceneDirty);
                }
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
    ImGui::Spacing();
    if (action("Create new material…", "Neuen Inhalt erstellen…")) ImGui::OpenPopup("CreateSessionMaterial");
    prepareSessionCommandPopup("CreateSessionMaterial");
    if (ImGui::BeginPopup("CreateSessionMaterial"))
    {
    if (action("Create a map", "Karte erstellen") && flush(campaign, actions.persist) && actions.newMap)
    { ImGui::CloseCurrentPopup(); actions.newMap(); }
    if (action("Create an NPC", "NSC erstellen") && flush(campaign, actions.persist) && actions.newNpc)
    { ImGui::CloseCurrentPopup(); actions.newNpc(); }
    if (action("Create a player character", "Spielerfigur erstellen") && flush(campaign, actions.persist) && actions.newCharacter)
    { ImGui::CloseCurrentPopup(); actions.newCharacter(); }
    if (action("Prepare an encounter", "Begegnung vorbereiten") && flush(campaign, actions.persist) && actions.newEncounter)
    { ImGui::CloseCurrentPopup(); actions.newEncounter(); }
    if (_error || _lastStatus != SessionPlan::Status::Applied) ImGui::TextWrapped("%s", statusText());
    ImGui::EndPopup();
    }
    ImGui::EndDisabled();
    ImGui::Spacing();
    if (action("Presentation controls", "Spieleranzeige steuern") && flush(campaign, actions.persist) && actions.presentation) actions.presentation();
    if (actions.blackout && action("Blackout", "Schwarzbild")) actions.blackout();
}


bool SessionWorkspace::restoreSelection(Campaign &campaign, const EntityId &sessionId, const EntityId &entryId,
                                        bool unavailable, const SessionPlan::Persist &persist)
{
    if (_campaignId != campaign.metadata.id)
    {
        *this = SessionWorkspace{};
        _campaignId = campaign.metadata.id;
    }
    if (!sessionId.empty() && !gm::ux::resolveNavigationEntity(campaign, sessionId).available) unavailable = true;
    const auto *destinationSession = SessionPlan::findSession(campaign, sessionId);
    const auto *destinationEntry = destinationSession ? SessionPlan::findEntry(*destinationSession, entryId) : nullptr;
    if (!entryId.empty() && (!destinationEntry ||
        (!destinationEntry->storyBeatId.empty() && !gm::ux::resolveNavigationEntity(campaign, destinationEntry->storyBeatId).available))) unavailable = true;
    if (!unavailable && !_selectionUnavailable && _sessionId == sessionId && _entryId == entryId) return true;
    if (dirty() && !flush(campaign, persist)) return false;
    if (sessionId.empty() && entryId.empty() && !unavailable)
    {
        *this = SessionWorkspace{};
        _campaignId = campaign.metadata.id;
        return true;
    }
    const auto *session = SessionPlan::findSession(campaign, sessionId);
    if (unavailable || !session || (!entryId.empty() && !SessionPlan::findEntry(*session, entryId)))
    {
        _sessionId = sessionId;
        _entryId = entryId;
        _selectionUnavailable = true;
        return true;
    }
    load(campaign, sessionId, entryId);
    return true;
}
