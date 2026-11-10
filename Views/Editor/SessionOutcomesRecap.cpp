#include "Views/Editor/SessionOutcomesPanelSupport.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Views/Editor/Widgets/ImageTextureCache.hpp"
#include "Views/Editor/Widgets/FrozenImageUi.hpp"

void SessionOutcomesPanel::recap(Campaign &campaign, const SessionModel &session,
                                const SessionOutcomes::Ledger &ledger, const SessionPlan::Persist &persist)
{
    using namespace SessionOutcomes;
    if (prepareRecapDraft(campaign, session.metadata.id, ledger)) return;
    auto &recapDraft = _recapDrafts[session.metadata.id];
    auto &_publicTitle = recapDraft.title;
    auto &_publicIntro = recapDraft.intro;
    const bool headerConflict = SessionOutcomeDraftState::headerConflict(ledger, _publicTitle, _publicIntro,
        recapDraft.baseTitle, recapDraft.baseIntro);
    if (_recapSession != session.metadata.id)
    {
        _recapSession = session.metadata.id;
        _recapPackage.reset(); _handouts.clear();
    }
    ImGui::TextWrapped("%s", ot("Only separate player text and deliberately selected public handouts are included. Review the exact text and images below.",
        "Nur eigener Spielertext und bewusst gewählte öffentliche Handouts werden aufgenommen. Prüfe unten den genauen Text und die Bilder."));
    ol("Player-facing title", "Titel für Spieler"); FormUi::InputText("##recap-title", &_publicTitle);
    ol("Introduction for players", "Einleitung für Spieler");
    FormUi::InputTextMultiline("##recap-intro", &_publicIntro, ImVec2(-1, 85), ImGuiInputTextFlags_WordWrap);
    if (headerConflict)
    {
        ImGui::TextWrapped("%s", ot("Saved title or introduction changed. This draft is retained privately; review uses the saved text.",
            "Gespeicherter Titel oder Einleitung geändert. Dieser Entwurf bleibt privat erhalten; die Prüfung verwendet den gespeicherten Text."));
        if (ob("Copy retained title and introduction", "Zurückbehaltenen Titel und Einleitung kopieren"))
            ImGui::SetClipboardText((_publicTitle + "\n\n" + _publicIntro).c_str());
        if (ob("Discard retained title and introduction", "Zurückbehaltenen Titel und Einleitung verwerfen"))
        {
            const auto previous = recapDraft;
            _publicTitle = recapDraft.baseTitle = ledger.publicTitle; _publicIntro = recapDraft.baseIntro = ledger.publicIntro;
            checkpointRecap(campaign, session.metadata.id, true);
            if (!recapDraft.error.empty()) { const auto error = recapDraft.error; recapDraft = previous; recapDraft.error = error; }
        }
    }
    ImGui::BeginDisabled(headerConflict);
    if (ob("Save player text", "Spielertext speichern"))
    {
        if (recapDraft.baseTitle != ledger.publicTitle || recapDraft.baseIntro != ledger.publicIntro)
            _message = ot("Saved player text changed. Copy your draft before reloading this session.", "Gespeicherter Spielertext wurde geändert. Kopiere deinen Entwurf vor dem Neuladen dieser Sitzung.");
        else if (report(setPublicRecap(campaign, session.metadata.id, _publicTitle, _publicIntro, persist)))
        { recapDraft.baseTitle = _publicTitle; recapDraft.baseIntro = _publicIntro; _recapPackage.reset(); }
    }
    ImGui::EndDisabled();
    for (const auto &outcome : ledger.outcomes)
    {
        if (outcome.state == State::Rejected) continue;
        recapDraft.outcomeIds.insert(outcome.id);
        ImGui::PushID(outcome.id.c_str());
        auto [entry, inserted] = _publicDrafts.try_emplace(outcome.id, PublicDraft{outcome.publicText, outcome.publicText, outcome.shareWithPlayers, outcome.shareWithPlayers});
        auto &draft = entry->second;
        if (!inserted && draft.text == draft.base && draft.share == draft.baseShare)
            draft = {outcome.publicText, outcome.publicText, outcome.shareWithPlayers, outcome.shareWithPlayers};
        if (SessionOutcomeDraftState::classify(ledger, outcome.id, draft) != SessionOutcomeDraftState::Disposition::Editable)
        { ImGui::PopID(); continue; }
        FormUi::Checkbox(ot("Include player text", "Spielertext aufnehmen"), &draft.share);
        FormUi::InputTextMultiline("##player-outcome", &draft.text, ImVec2(-1, 62), ImGuiInputTextFlags_WordWrap);
        if ((draft.text != draft.base || draft.share != draft.baseShare) && ob("Save this player text", "Diesen Spielertext speichern"))
        {
            if (report(setPlayerText(campaign, session.metadata.id, outcome.id, draft.base, draft.baseShare, draft.text, draft.share, persist)))
            { draft.base = draft.text; draft.baseShare = draft.share; _recapPackage.reset(); }
        }
        ImGui::PopID();
    }
    retainedRecapDrafts(campaign, session.metadata.id, ledger);
    if (ImGui::CollapsingHeader(ot("Choose public handouts (optional)", "Öffentliche Handouts wählen (optional)")))
    {
        ImGui::TextWrapped("%s", ot("Only handouts already allowed for players are listed. Managed image attachments are supported; other file types stay excluded.",
            "Nur bereits für Spieler freigegebene Handouts werden angezeigt. Verwaltete Bildanhänge werden unterstützt, andere Dateitypen bleiben ausgeschlossen."));
        for (const auto &handout : buildPlayerSafeHandouts(campaign))
        {
            bool chosen = _handouts.count(handout.assetId) != 0;
            if (FormUi::Checkbox((handout.title + "##" + handout.assetId).c_str(), &chosen))
            { if (chosen) _handouts.insert(handout.assetId); else _handouts.erase(handout.assetId); _recapPackage.reset(); }
        }
    }
    bool savedDraft = headerConflict || (_publicTitle == ledger.publicTitle && _publicIntro == ledger.publicIntro);
    for (const auto &o : ledger.outcomes)
        if (const auto it = _publicDrafts.find(o.id); it != _publicDrafts.end())
            savedDraft &= !SessionOutcomeDraftState::blocksReview(ledger, o.id, it->second);
    if (!savedDraft) ImGui::TextWrapped("%s", ot("Save the edited player text above before reviewing.", "Speichere den bearbeiteten Spielertext oben vor der Prüfung."));
    ImGui::BeginDisabled(!savedDraft);
    if (ob("Review player recap", "Spieler-Rückblick prüfen", true))
    {
        _recapPackage = std::make_shared<PlayerRecapPackage>();
        if (!_recapPackage->review(campaign, session.metadata.id, {_handouts.begin(), _handouts.end()}))
        { _message = _recapPackage->error(); _recapPackage.reset(); }
    }
    ImGui::EndDisabled();
    if (!_recapPackage) return;
    ImGui::SeparatorText(ot("Exactly what players will receive", "Genau dieser Text wird ausgegeben"));
    ImGui::TextWrapped("%s", _recapPackage->document().title.c_str());
    ImGui::TextWrapped("%s", _recapPackage->document().intro.c_str());
    for (const auto &item : _recapPackage->document().items) ImGui::TextWrapped("%s", item.text.c_str());
    bool imagesReviewed = true;
    for (const auto &handout : _recapPackage->attachments())
    {
        if (handout.projectRelativePath.empty()) continue;
        const auto asset=_recapPackage->image(handout);
        const auto &texture=findCachedImageTexture(asset);
        imagesReviewed &= texture.loaded&&texture.textureId&&texture.width>0&&texture.height>0;
        FrozenImageUi::slot(asset,300.f,ot("Loading recap image…","Rückblickbild wird geladen…"),
            ot("Image preview unavailable. Remove this handout or repair its image before exporting.",
               "Bildvorschau nicht verfügbar. Entferne dieses Handout oder repariere das Bild vor dem Export."),FrozenImageUi::lookup,false);
    }
    const bool current = savedDraft && _recapPackage->current(campaign);
    if(!imagesReviewed)ImGui::TextWrapped("%s",ot("View every selected image and wait for it to load before exporting.",
        "Sieh dir vor dem Export jedes gewählte Bild an und warte, bis es geladen ist."));
    if (!current) ImGui::TextWrapped("%s", ot("Content changed. Review it again before exporting.", "Inhalt geändert. Vor dem Export erneut prüfen."));
    ImGui::BeginDisabled(!current || !imagesReviewed);
    if (ob("Export reviewed Markdown and HTML", "Geprüftes Markdown und HTML exportieren", true))
    {
        if (_recapPackage->exportTo(campaign, ProjectPackageLayout::layoutForCampaign(campaign).exportsDirectory / "PlayerRecaps"))
            _message = std::string(ot("Exported to: ", "Exportiert nach: ")) + _recapPackage->outputDirectory().u8string();
        else _message = _recapPackage->error();
    }
    ImGui::EndDisabled();
}

void SessionOutcomesPanel::retainedRecapDrafts(Campaign &campaign, const EntityId &sessionId,
                                             const SessionOutcomes::Ledger &ledger)
{
    auto &recapDraft = _recapDrafts.at(sessionId);
    bool heading = false;
    for (const auto &id : recapDraft.outcomeIds)
    {
        const auto found = _publicDrafts.find(id);
        if (found == _publicDrafts.end() || !found->second.dirty()) continue;
        const auto disposition = SessionOutcomeDraftState::classify(ledger, id, found->second);
        using D = SessionOutcomeDraftState::Disposition;
        if (disposition == D::Editable) continue;
        if (!heading)
        {
            heading = true;
            ImGui::SeparatorText(ot("Retained drafts (GM only)", "Zurückbehaltene Entwürfe (nur SL)"));
            ImGui::TextWrapped("%s", ot("These drafts are excluded from player review and export. Copy them before explicitly discarding them if still needed.",
                "Diese Entwürfe sind von Spielerprüfung und Export ausgeschlossen. Kopiere sie vor dem ausdrücklichen Verwerfen, falls du sie noch brauchst."));
        }
        ImGui::PushID(id.c_str());
        ImGui::TextUnformatted(disposition == D::Rejected ? ot("Outcome rejected", "Ergebnis verworfen") :
            disposition == D::Missing ? ot("Outcome no longer exists", "Ergebnis nicht mehr vorhanden") :
            ot("Saved player text changed", "Gespeicherter Spielertext geändert"));
        FormUi::InputTextMultiline("##retained-player-outcome", &found->second.text, ImVec2(-1, 62),
            ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_WordWrap);
        if (ob("Copy retained draft", "Zurückbehaltenen Entwurf kopieren")) ImGui::SetClipboardText(found->second.text.c_str());
        if (ob("Discard retained draft", "Zurückbehaltenen Entwurf verwerfen"))
        {
            const auto previous = found->second;
            if (const auto *current = SessionOutcomeDraftState::find(ledger, id))
                found->second = {current->publicText, current->publicText, current->shareWithPlayers, current->shareWithPlayers};
            else _publicDrafts.erase(found);
            checkpointRecap(campaign, sessionId, true);
            if (!recapDraft.error.empty()) _publicDrafts[id] = previous;
        }
        ImGui::PopID();
    }
}
