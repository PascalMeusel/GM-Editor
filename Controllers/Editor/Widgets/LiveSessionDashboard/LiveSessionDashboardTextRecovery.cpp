#include "Controllers/Editor/Widgets/LiveSessionDashboard/LiveSessionDashboardControllerSupport.hpp"
#include <stdexcept>
#include <imgui.h>

namespace
{
constexpr const char *Key = "live-dashboard";
using Fields = TextDraftRecovery::Fields;
Fields savedText(const SessionModel &session)
{
    std::string noteIds;
    for (const auto &note : session.notes) noteIds += std::to_string(note.metadata.id.size()) + ":" + note.metadata.id;
    return {{"session", session.metadata.id}, {"summary", session.summary}, {"recap", session.recap},
        {"unresolved", session.unresolvedThreads}, {"loot", session.lootAwarded}, {"xp", session.xpMilestones},
        {"npcs", session.npcsMet}, {"locations", session.locationsVisited}, {"hooks", session.openHooks},
        {"note", ""}, {"note-link", ""}, {"notes-revision", noteIds}};
}
}

void LiveSessionDashboardController::prepareTextRecovery()
{
    if (_textRecoveryCampaign != _currentCampaign.metadata.id)
    {
        _textRecoveryCampaign = _currentCampaign.metadata.id;
        _textRecoveryRead = _textRecoveryPending = false; _textRecoveryBase.clear();
        _textRecoveryFields.clear(); _textRecoveredFields.clear(); _textRecoveryError.clear();
    }
    if (!_textRecoveryRead)
    {
        const auto result = TextDraftRecovery::read(_currentCampaign, Key);
        _textRecoveryRead = true; _textRecoveryPending = result.found || !result.success;
        _textRecoveredFields = result.fields; _textRecoveryError = result.error;
    }
    if (const auto *session = activeSession(); session && _sessionSummaryDraftId == session->metadata.id &&
        (_textRecoveryBase.empty() || _textRecoveryBase["session"] != session->metadata.id))
        _textRecoveryBase = savedText(*session);
}

void LiveSessionDashboardController::checkpointTextRecovery(bool force)
{
    if (!_textRecoveryRead || _textRecoveryPending || _textRecoveryBase.empty() ||
        _textRecoveryCampaign != _currentCampaign.metadata.id || _sessionSummaryDraftId.empty()) return;
    const auto now = ImGui::GetTime();
    if (!force && now - _textRecoveryAt < 2.0) return;
    _textRecoveryAt = now;
    Fields fields{{"session", _sessionSummaryDraftId}, {"summary", _sessionSummaryDraft}, {"recap", _recapDraft},
        {"unresolved", _unresolvedThreadsDraft}, {"loot", _lootAwardedDraft}, {"xp", _xpMilestonesDraft},
        {"npcs", _npcsMetDraft}, {"locations", _locationsVisitedDraft}, {"hooks", _openHooksDraft},
        {"note", _sessionNoteDraft}, {"note-link", _selectedNoteEntityId}};
    fields["notes-revision"] = _textRecoveryBase["notes-revision"];
    if (fields == _textRecoveryBase)
    {
        if (!_textRecoveryFields.empty()) clearTextRecovery();
        return;
    }
    for (const auto &[key, value] : _textRecoveryBase) fields["base-" + key] = value;
    if (fields == _textRecoveryFields) return;
    _textRecoveryError = TextDraftRecovery::save(_currentCampaign, Key, fields);
    if (_textRecoveryError.empty()) _textRecoveryFields = std::move(fields);
}

bool LiveSessionDashboardController::restoreTextRecovery()
{
    try
    {
        const auto &fields = _textRecoveredFields;
        const auto *session = activeSession();
        if (!session || session->metadata.id != fields.at("session"))
            throw std::runtime_error("The recovered live session is no longer active. Copy its text before discarding it.");
        const auto current = savedText(*session);
        for (const auto &[key, value] : current)
            if (fields.at("base-" + key) != value)
                throw std::runtime_error("Saved session text changed. Copy the recovered text before discarding it.");
        for (const auto &[key, value] : current) (void)fields.at(key);
        _sessionSummaryDraftId = fields.at("session"); _sessionSummaryDraft = fields.at("summary");
        _recapDraft = fields.at("recap"); _unresolvedThreadsDraft = fields.at("unresolved");
        _lootAwardedDraft = fields.at("loot"); _xpMilestonesDraft = fields.at("xp");
        _npcsMetDraft = fields.at("npcs"); _locationsVisitedDraft = fields.at("locations");
        _openHooksDraft = fields.at("hooks"); _sessionNoteDraft = fields.at("note");
        _selectedNoteEntityId = fields.at("note-link"); _textRecoveryBase = current;
        _textRecoveryFields = fields; _textRecoveryPending = false; _textRecoveryError.clear();
        _sessionPlanMessage = "Recovered text is a draft. Review it, then save.";
        return true;
    }
    catch (const std::exception &error) { _textRecoveryError = error.what(); return false; }
}

void LiveSessionDashboardController::clearTextRecovery()
{
    _textRecoveryError = TextDraftRecovery::remove(_currentCampaign, Key);
    if (!_textRecoveryError.empty()) return;
    _textRecoveryPending = false; _textRecoveredFields.clear(); _textRecoveryFields.clear(); _textRecoveryBase.clear();
    if (const auto *session = activeSession()) _textRecoveryBase = savedText(*session);
}
