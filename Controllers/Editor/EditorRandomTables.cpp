#include "Controllers/Editor/EditorWindowController.hpp"

#include "Models/Project/EntityRegistry.hpp"
#include "Models/Project/ProjectSaveService.hpp"

#include <algorithm>

bool EditorWindowController::saveRandomTable(const RandomTableModel &table, std::string &error)
{
    const std::vector<std::string> validationErrors = RandomTables::validate(table);
    if (!validationErrors.empty())
    {
        error = validationErrors.front();
        return false;
    }

    Campaign staged = _currentCampaign;
    auto found = std::find_if(staged.randomTables.begin(), staged.randomTables.end(), [&](const auto &candidate) {
        return candidate.metadata.id == table.metadata.id;
    });
    RandomTableModel saved = table;
    saved.metadata.touch();
    if (found == staged.randomTables.end())
        staged.randomTables.push_back(std::move(saved));
    else
        *found = std::move(saved);
    staged.metadata.touch();

    if (!ProjectSaveService::getInstance().saveCampaign(staged, "save random table"))
    {
        error = ProjectSaveService::getInstance().state().lastError;
        if (error.empty())
            error = "The random table could not be saved.";
        return false;
    }

    _currentCampaign = std::move(staged);
    EntityRegistry::getInstance().rebuild(_currentCampaign);
    markSearchIndexDirty();
    error.clear();
    return true;
}

RandomTableRollResult EditorWindowController::rollRandomTable(const EntityId &tableId)
{
    const auto found = std::find_if(_currentCampaign.randomTables.begin(), _currentCampaign.randomTables.end(),
                                    [&](const auto &table) { return table.metadata.id == tableId; });
    if (found == _currentCampaign.randomTables.end())
    {
        RandomTableRollResult result;
        result.error = "The random table is no longer available.";
        return result;
    }
    return RandomTables::roll(*found, _randomTableGenerator);
}

bool EditorWindowController::addRandomTableRollToSessionNotes(const RandomTableRollResult &roll, std::string &error)
{
    if (!roll.rolled)
    {
        error = roll.error.empty() ? "Roll the table before adding a note." : roll.error;
        return false;
    }
    Campaign staged = _currentCampaign;
    if (!RandomTables::appendToActiveSessionNotes(staged, roll, error))
        return false;

    if (!ProjectSaveService::getInstance().saveCampaign(staged, "add random table result to session notes"))
    {
        error = ProjectSaveService::getInstance().state().lastError;
        if (error.empty())
            error = "The session note could not be saved.";
        return false;
    }
    _currentCampaign = std::move(staged);
    EntityRegistry::getInstance().rebuild(_currentCampaign);
    markSearchIndexDirty();
    error.clear();
    return true;
}

bool EditorWindowController::createRandomTableRollHandout(const RandomTableRollResult &roll, std::string &error)
{
    if (!roll.rolled || !roll.playerSafe)
    {
        error = "Only a result marked player-safe can become a handout.";
        return false;
    }

    Campaign staged = _currentCampaign;
    if (!RandomTables::createPlayerHandout(staged, roll, error))
        return false;

    if (!ProjectSaveService::getInstance().saveCampaign(staged, "create player handout from random table result"))
    {
        error = ProjectSaveService::getInstance().state().lastError;
        if (error.empty())
            error = "The handout could not be saved.";
        return false;
    }
    _currentCampaign = std::move(staged);
    EntityRegistry::getInstance().rebuild(_currentCampaign);
    markSearchIndexDirty();
    error.clear();
    return true;
}
