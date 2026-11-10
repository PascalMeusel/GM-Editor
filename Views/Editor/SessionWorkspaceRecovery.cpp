#include "Views/Editor/SessionWorkspaceSupport.hpp"
#include <stdexcept>

namespace
{
constexpr const char *Key = "session-workspace";
using Fields = TextDraftRecovery::Fields;
void sceneFields(Fields &fields, const StoryBeatModel &scene, const std::string &prefix)
{
    fields[prefix + "title"] = scene.title; fields[prefix + "summary"] = scene.summary;
    fields[prefix + "clues"] = scene.clues; fields[prefix + "consequences"] = scene.consequences;
    fields[prefix + "read-aloud"] = scene.readAloudText; fields[prefix + "player-summary"] = scene.playerSummary;
    fields[prefix + "gm-notes"] = scene.gmNotes;
}
void readScene(const Fields &fields, StoryBeatModel &scene)
{
    scene.title = fields.at("scene-title"); scene.summary = fields.at("scene-summary");
    scene.clues = fields.at("scene-clues"); scene.consequences = fields.at("scene-consequences");
    scene.readAloudText = fields.at("scene-read-aloud"); scene.playerSummary = fields.at("scene-player-summary");
    scene.gmNotes = fields.at("scene-gm-notes");
}
}

void SessionWorkspace::refreshRecoveryBase(const Campaign &campaign)
{
    const auto *session = SessionPlan::findSession(campaign, _sessionId);
    if (!session) return;
    if (!_sessionDirty)
    {
        _sessionTextBase["title"] = session->title; _sessionTextBase["notes"] = session->roughNotes;
        _sessionTextBase["scheduled"] = session->scheduledAtUtc;
    }
    if (!_entryDirty)
        if (const auto *entry = SessionPlan::findEntry(*session, _entryId)) _sessionTextBase["entry-note"] = entry->prepNote;
}

void SessionWorkspace::checkpointRecovery(Campaign &campaign, bool force)
{
    if (!_recoveryRead || _recoveryPending || _campaignId != campaign.metadata.id) return;
    const auto now = ImGui::GetTime();
    if (!force && now - _lastRecoveryAt < 2.0) return;
    _lastRecoveryAt = now;
    if (!dirty() && _newSessionTitle.empty() && _newSceneTitle.empty())
    {
        if (!_lastRecoveryFields.empty())
        {
            _recoveryError = TextDraftRecovery::remove(campaign, Key);
            if (_recoveryError.empty()) _lastRecoveryFields.clear();
        }
        return;
    }
    Fields fields{{"session", _sessionId}, {"entry", _entryId}, {"scene", _sceneId},
        {"session-dirty", _sessionDirty ? "1" : "0"}, {"scene-dirty", _sceneDirty ? "1" : "0"},
        {"entry-dirty", _entryDirty ? "1" : "0"}, {"title", _title}, {"notes", _notes},
        {"scheduled", _scheduled}, {"entry-note", _entryNote},
        {"new-session", _newSessionTitle}, {"new-scene", _newSceneTitle}};
    for (const auto &[key, value] : _sessionTextBase) fields["base-" + key] = value;
    sceneFields(fields, _scene, "scene-"); sceneFields(fields, _sceneBase, "base-scene-");
    if (fields == _lastRecoveryFields) return;
    _recoveryError = TextDraftRecovery::save(campaign, Key, fields);
    if (_recoveryError.empty()) _lastRecoveryFields = std::move(fields);
}

bool SessionWorkspace::restoreRecovery(Campaign &campaign)
{
    try
    {
        const auto fields = _recoveredFields;
        const auto flag = [&](const char *key) {
            const auto &value = fields.at(key);
            if (value != "0" && value != "1") throw std::runtime_error("Invalid recovery flags.");
            return value == "1";
        };
        const bool sessionDirty = flag("session-dirty"), sceneDirty = flag("scene-dirty"), entryDirty = flag("entry-dirty");
        const auto *session = SessionPlan::findSession(campaign, fields.at("session"));
        const auto *entry = session ? SessionPlan::findEntry(*session, fields.at("entry")) : nullptr;
        const auto *scene = SessionPlan::findScene(campaign, fields.at("scene"));
        const auto require = [](bool value) { if (!value) throw std::runtime_error("Saved content changed or is unavailable. Copy the recovered text before discarding it."); };
        if (sessionDirty || sceneDirty || entryDirty) require(session && session->endedAtUtc.empty());
        if (sessionDirty) require(session->title == fields.at("base-title") && session->roughNotes == fields.at("base-notes") &&
                                  session->scheduledAtUtc == fields.at("base-scheduled"));
        if (entryDirty) require(entry && entry->prepNote == fields.at("base-entry-note"));
        if (sceneDirty)
        {
            require(scene && entry && entry->storyBeatId == scene->metadata.id);
            Fields canonical; sceneFields(canonical, *scene, "base-scene-");
            for (const auto &[key, value] : canonical) require(fields.at(key) == value);
        }
        // Validate every referenced field before changing the visible draft.
        StoryBeatModel restoredScene = scene ? *scene : StoryBeatModel{};
        if (sceneDirty) readScene(fields, restoredScene);
        const auto title = fields.at("title"), notes = fields.at("notes"), scheduled = fields.at("scheduled");
        const auto entryNote = fields.at("entry-note"), newSession = fields.at("new-session"), newScene = fields.at("new-scene");
        load(campaign, fields.at("session"), fields.at("entry"));
        if (sessionDirty) { _title = title; _notes = notes; _scheduled = scheduled; }
        if (sceneDirty) _scene = std::move(restoredScene);
        if (entryDirty) _entryNote = entryNote;
        _newSessionTitle = newSession; _newSceneTitle = newScene;
        _sessionDirty = sessionDirty; _sceneDirty = sceneDirty; _entryDirty = entryDirty;
        _recoveryReview = true; _recoveryPending = false; _recoveryError.clear();
        _lastRecoveryFields = fields; _lastEdit = ImGui::GetTime();
        return true;
    }
    catch (const std::exception &error) { _recoveryError = error.what(); return false; }
}

void SessionWorkspace::readRecovery(Campaign &campaign)
{
    if (!_recoveryRead)
    {
        const auto result = TextDraftRecovery::read(campaign, Key);
        _recoveryRead = true; _recoveryPending = result.found || !result.success;
        _recoveredFields = result.fields; _recoveryError = result.error;
    }
}

bool SessionWorkspace::drawRecovery(Campaign &campaign)
{
    readRecovery(campaign);
    if (!_recoveryError.empty()) ImGui::TextWrapped("%s", _recoveryError.c_str());
    if (!_recoveryPending)
    {
        if (_recoveryReview) ImGui::TextWrapped("%s", st("Recovered text is a draft. Review it, then Save.", "Wiederhergestellter Text ist ein Entwurf. Prüfe ihn und speichere danach."));
        return false;
    }
    ImGui::TextWrapped("%s", st("Unfinished session text was recovered. Restore it for review or discard the checkpoint.",
        "Unfertiger Sitzungstext wurde gefunden. Stelle ihn zur Prüfung wieder her oder verwerfe die Sicherung."));
    ImGui::BeginDisabled(_recoveredFields.empty());
    if (action("Restore text draft", "Textentwurf wiederherstellen")) restoreRecovery(campaign);
    if (action("Copy recovered text", "Wiederhergestellten Text kopieren"))
    {
        std::string text;
        for (const auto &[key, value] : _recoveredFields)
            if (key.rfind("base-", 0) != 0) text += key + ": " + value + "\n\n";
        ImGui::SetClipboardText(text.c_str());
    }
    ImGui::EndDisabled();
    if (action("Discard recovered text", "Wiederhergestellten Text verwerfen"))
    {
        _recoveryError = TextDraftRecovery::remove(campaign, Key);
        if (_recoveryError.empty()) { _recoveredFields.clear(); _recoveryPending = false; _lastRecoveryFields.clear(); }
    }
    return true;
}
