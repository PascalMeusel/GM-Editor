#include "Views/Editor/SessionOutcomesPanelSupport.hpp"
#include <stdexcept>

bool SessionOutcomesPanel::recapNeedsCheckpoint(const RecapDraft &draft) const
{
    bool dirty = draft.title != draft.baseTitle || draft.intro != draft.baseIntro;
    bool same = draft.title == draft.checkpointTitle && draft.intro == draft.checkpointIntro &&
        draft.baseTitle == draft.checkpointBaseTitle && draft.baseIntro == draft.checkpointBaseIntro;
    std::size_t count = 0;
    for (const auto &id : draft.outcomeIds)
        if (const auto entry = _publicDrafts.find(id); entry != _publicDrafts.end() && entry->second.dirty())
        {
            dirty = true; ++count;
            const auto saved = draft.checkpointTexts.find(id);
            const auto &text = entry->second;
            same &= saved != draft.checkpointTexts.end() && text.text == saved->second.text && text.base == saved->second.base &&
                text.share == saved->second.share && text.baseShare == saved->second.baseShare;
        }
    return dirty ? draft.checkpoint.empty() || !same || count != draft.checkpointTexts.size() : !draft.checkpoint.empty();
}
void SessionOutcomesPanel::rememberRecapCheckpoint(RecapDraft &draft)
{
    draft.checkpointTitle = draft.title; draft.checkpointIntro = draft.intro;
    draft.checkpointBaseTitle = draft.baseTitle; draft.checkpointBaseIntro = draft.baseIntro;
    draft.checkpointTexts.clear();
    for (const auto &id : draft.outcomeIds)
        if (const auto entry = _publicDrafts.find(id); entry != _publicDrafts.end() && entry->second.dirty())
            draft.checkpointTexts.emplace(id, entry->second);
}

TextDraftRecovery::Fields SessionOutcomesPanel::recapFields(const EntityId &id, const RecapDraft &draft) const
{
        SessionOutcomes::Ledger before, after;
        before.publicTitle = draft.baseTitle; before.publicIntro = draft.baseIntro;
        after.publicTitle = draft.title; after.publicIntro = draft.intro;
        bool changed = before.publicTitle != after.publicTitle || before.publicIntro != after.publicIntro;
        for (const auto &outcomeId : draft.outcomeIds)
        {
            const auto found = _publicDrafts.find(outcomeId);
            if (found == _publicDrafts.end()) continue;
            const auto &text = found->second;
            if (text.text == text.base && text.share == text.baseShare) continue;
            SessionOutcomes::Outcome old, next;
            old.id = next.id = outcomeId;
            old.publicText = text.base; old.shareWithPlayers = text.baseShare;
            next.publicText = text.text; next.shareWithPlayers = text.share;
            // Keep deterministic codec fields so unchanged drafts don't rewrite.
            old.createdAtUtc = next.createdAtUtc = "recovery";
            before.outcomes.push_back(old); after.outcomes.push_back(next); changed = true;
        }
        if (!changed) return {};
        return {{"session", id}, {"before", SessionOutcomes::serialize(before)}, {"after", SessionOutcomes::serialize(after)}};
}

void SessionOutcomesPanel::checkpointRecap(Campaign &campaign, const EntityId &id, bool force)
{
    auto &draft = _recapDrafts.at(id);
    if (!draft.loaded || draft.pending || !SessionOutcomeDraftState::checkpointDue(ImGui::GetTime(), draft.lastCheckpoint, force)) return;
    draft.lastCheckpoint = ImGui::GetTime();
    if (!recapNeedsCheckpoint(draft)) { draft.error.clear(); return; }
    try
    {
        auto fields = recapFields(id, draft);
        if (fields.empty())
        {
            draft.error.clear();
            if (!draft.checkpoint.empty())
            {
                draft.error = TextDraftRecovery::remove(campaign, recapRecoveryKey(id));
                if (draft.error.empty()) { draft.checkpoint.clear(); rememberRecapCheckpoint(draft); }
            }
            return;
        }
        if (fields == draft.checkpoint) { draft.error.clear(); return; }
        draft.error = TextDraftRecovery::save(campaign, recapRecoveryKey(id), fields);
        if (draft.error.empty()) { draft.checkpoint = std::move(fields); rememberRecapCheckpoint(draft); }
    }
    catch (const std::exception &error) { draft.error = error.what(); }
}

bool SessionOutcomesPanel::prepareRecapDraft(Campaign &campaign, const EntityId &id,
                                           const SessionOutcomes::Ledger &ledger)
{
    auto &draft = _recapDrafts[id];
    if (!draft.loaded)
    {
        draft.loaded = true; draft.title = draft.baseTitle = ledger.publicTitle; draft.intro = draft.baseIntro = ledger.publicIntro;
        const auto saved = TextDraftRecovery::read(campaign, recapRecoveryKey(id));
        draft.pending = saved.found || !saved.success; draft.error = saved.error; draft.recovered = saved.fields;
    }
    if (!draft.pending && draft.title == draft.baseTitle && draft.intro == draft.baseIntro)
    { draft.title = draft.baseTitle = ledger.publicTitle; draft.intro = draft.baseIntro = ledger.publicIntro; }
    if (!draft.error.empty()) ImGui::TextWrapped("%s", draft.error.c_str());
    if (!draft.pending) return false;
    ImGui::TextWrapped("%s", ot("Unfinished player recap text was found. Restore it for review or discard the checkpoint.",
        "Unfertiger Text für den Spieler-Rückblick wurde gefunden. Stelle ihn zur Prüfung wieder her oder verwerfe die Sicherung."));
    const auto decode = [&] {
        if (draft.recovered.at("session") != id) throw std::runtime_error("Recovered recap belongs to another session.");
        const auto before = SessionOutcomes::deserialize(draft.recovered.at("before"));
        const auto after = SessionOutcomes::deserialize(draft.recovered.at("after"));
        if (!before.success || !after.success || before.ledger.outcomes.size() != after.ledger.outcomes.size())
            throw std::runtime_error("Recovered recap text could not be decoded. Its checkpoint was retained.");
        return std::pair{before.ledger, after.ledger};
    };
    ImGui::BeginDisabled(draft.recovered.empty());
    if (ob("Restore recap draft", "Rückblick-Entwurf wiederherstellen"))
    {
        try
        {
            const auto [before, after] = decode();
            std::map<EntityId, PublicDraft> restored;
            for (std::size_t i = 0; i < before.outcomes.size(); ++i)
            {
                const auto &old = before.outcomes[i], &next = after.outcomes[i];
                if (old.id.empty() || old.id != next.id || restored.count(old.id))
                    throw std::runtime_error("Recovered recap entry identities do not match. Its checkpoint was retained.");
                // Rejected, deleted, or changed entries remain private retained
                // drafts. The recap editor classifies them against current data;
                // restoring never mutates the saved player text.
                restored[old.id] = {next.publicText, old.publicText, next.shareWithPlayers, old.shareWithPlayers};
            }
            draft.title = after.publicTitle; draft.intro = after.publicIntro;
            draft.baseTitle = before.publicTitle; draft.baseIntro = before.publicIntro;
            for (auto &[outcome, text] : restored)
            { _publicDrafts[outcome] = std::move(text); draft.outcomeIds.insert(outcome); }
            draft.pending = false; draft.error.clear(); draft.checkpoint = draft.recovered; _recapPackage.reset();
            rememberRecapCheckpoint(draft);
        }
        catch (const std::exception &error) { draft.error = error.what(); }
    }
    if (ob("Copy recovered recap text", "Wiederhergestellten Rückblick kopieren"))
    {
        try
        {
            const auto [before, after] = decode();
            std::string text = after.publicTitle + "\n\n" + after.publicIntro;
            for (const auto &outcome : after.outcomes) text += "\n\n" + outcome.publicText;
            ImGui::SetClipboardText(text.c_str());
        }
        catch (const std::exception &error) { draft.error = error.what(); }
    }
    ImGui::EndDisabled();
    if (ob("Discard recovered recap", "Wiederhergestellten Rückblick verwerfen"))
    {
        draft.error = TextDraftRecovery::remove(campaign, recapRecoveryKey(id));
        if (draft.error.empty()) { draft.pending = false; draft.recovered.clear(); draft.checkpoint.clear(); }
    }
    return true;
}
