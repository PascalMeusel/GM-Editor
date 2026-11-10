#pragma once
#include "Controllers/Editor/EditorWindowController.hpp"
#include "Controllers/Editor/Widgets/LiveSessionDashboard/LiveSessionDashboardController.hpp"
#include "Controllers/Editor/Widgets/GMScreen/GMScreenController.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectCommand.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "imgui.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

// Production controllers and save service, without draw/update, windows, GL,
// input injection or audio playback. Only private action entry points are exposed.
struct TabletopHistoryProbe
{
    static Campaign &campaign(EditorWindowController &editor) { return editor._currentCampaign; }
    static TabletopToolkitController &toolkit(EditorWindowController &editor, bool gm = false)
    { return *(gm ? editor._children.gmScreen->_tabletopToolkit : editor._children.liveSessionDashboard->_tabletopToolkit); }
    static bool autosave(EditorWindowController &editor, ProjectSaveService &service)
    { return editor.updateCampaignAutosave(service); }
    static bool pending(EditorWindowController &editor) { return toolkit(editor)._history->pending; }
    static std::shared_ptr<TabletopToolkitHistoryState> shared(EditorWindowController &editor)
    { return toolkit(editor)._history; }
    template<bool Clues> static auto &records(EditorWindowController &editor)
    {
        if constexpr (Clues) return campaign(editor).storyClues;
        else return campaign(editor).storyObjectives;
    }
    template<bool Clues> static void track(EditorWindowController &editor, std::size_t index, bool gm = false)
    {
        auto &owner = toolkit(editor, gm);
        if constexpr (Clues) owner.trackClue(records<Clues>(editor).at(index));
        else owner.trackObjective(records<Clues>(editor).at(index));
    }
    template<bool Clues> static void edit(EditorWindowController &editor, std::size_t index, const std::string &title, bool gm = false)
    {
        track<Clues>(editor, index, gm); records<Clues>(editor)[index].title = title;
        toolkit(editor, gm).markCampaignEdited();
    }
    template<bool Clues> static void remove(EditorWindowController &editor, std::size_t index, bool gm = false)
    {
        track<Clues>(editor, index, gm);
        auto &values = records<Clues>(editor); values.erase(values.begin() + static_cast<std::ptrdiff_t>(index));
        toolkit(editor, gm).markCampaignEdited();
    }
    template<bool Clues> static EntityId append(EditorWindowController &editor, bool gm = false)
    {
        auto &owner = toolkit(editor, gm);
        if constexpr (Clues)
        {
            const auto id = owner._newClue.metadata.id;
            owner._newClue.title = "D"; owner._newClue.playerText = "New clue"; owner.addClue(); return id;
        }
        else
        {
            const auto id = owner._newObjective.metadata.id;
            owner._newObjective.title = "D"; owner.addObjective(); return id;
        }
    }
};

namespace TabletopHistoryTests
{
inline void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
inline void environment(const char *key, const std::string &value)
{
#ifdef _WIN32
    require(_putenv_s(key, value.c_str()) == 0, "Cannot isolate history fixture");
#else
    require(setenv(key, value.c_str(), 1) == 0, "Cannot isolate history fixture");
#endif
}
inline Campaign reload(const EntityId &id)
{
    for (auto campaign : CampaignRepository::loadAllCampaigns()) if (campaign.metadata.id == id) return campaign;
    throw std::runtime_error("Saved history fixture missing");
}
template<class T> std::vector<EntityId> ids(const std::vector<T> &values)
{
    std::vector<EntityId> result;
    for (const auto &value : values) result.push_back(value.metadata.id);
    return result;
}
struct Fixture
{
    EditorWindowController editor;
    EntityId id;
    Fixture()
    {
        Campaign value; value.ruleSet = std::make_shared<DND5e>();
        value.name = "Synthetic toolkit history " + value.metadata.id; id = value.metadata.id;
        for (const auto *title : {"A", "B", "C"})
        {
            StoryObjectiveModel objective; objective.title = title; value.storyObjectives.push_back(objective);
            StoryClueModel clue; clue.title = title; clue.playerText = "Public clue"; value.storyClues.push_back(clue);
        }
        CampaignRepository::saveCampaign(value); editor.setCurrentCampaign(value);
        ProjectSaveService::getInstance().discardUnsavedChanges("new isolated fixture");
    }
    ~Fixture() { ProjectCommandManager::getInstance().clear(); }
    bool save() { return editor.saveCurrentCampaign("keyboard shortcut"); }
    Campaign disk() const { return reload(id); }
    Campaign &live() { return TabletopHistoryProbe::campaign(editor); }
    template<bool Clues> auto &records() { return TabletopHistoryProbe::records<Clues>(editor); }
    template<bool Clues> std::vector<EntityId> diskIds() const
    {
        const auto saved = disk();
        if constexpr (Clues) return ids(saved.storyClues);
        else return ids(saved.storyObjectives);
    }
};
struct RejectWrites
{
    SQLite::Database database{ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE};
    explicit RejectWrites(bool clues)
    {
        database.exec(std::string("CREATE TRIGGER reject_history BEFORE INSERT ON ") + (clues ? "story_clues" : "story_objectives") +
            " BEGIN SELECT RAISE(ABORT,'injected history write failure'); END;");
    }
    ~RejectWrites() { try { database.exec("DROP TRIGGER reject_history;"); } catch (...) {} }
};
}
