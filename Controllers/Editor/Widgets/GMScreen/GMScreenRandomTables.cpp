#include "Controllers/Editor/Widgets/GMScreen/GMScreenController.hpp"

#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Random/RandomTableService.hpp"

#include <algorithm>
#include <utility>

void GMScreenController::rollSelectedRandomTable()
{
    if (_currentCampaign.randomTables.empty())
    {
        _randomTableStatus = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
                                 ? "Erstelle zuerst eine Zufallstabelle im Kompendium."
                                 : "Create a random table in Compendium first.";
        return;
    }
    _selectedRandomTableIndex = std::clamp(_selectedRandomTableIndex, 0,
                                           static_cast<int>(_currentCampaign.randomTables.size()) - 1);
    RandomTableRollResult result = RandomTables::roll(
        _currentCampaign.randomTables[static_cast<std::size_t>(_selectedRandomTableIndex)], _randomGenerator);
    if (!result.rolled)
    {
        _randomTableStatus = result.error;
        return;
    }
    _randomTableRollHistory.insert(_randomTableRollHistory.begin(), result);
    if (_randomTableRollHistory.size() > 10)
        _randomTableRollHistory.resize(10);
    _randomTableStatus = RandomTables::formatRoll(result);
}

bool GMScreenController::saveLatestRandomTableRollToNotes()
{
    if (_randomTableRollHistory.empty())
        return false;
    Campaign staged = _currentCampaign;
    if (!RandomTables::appendToActiveSessionNotes(staged, _randomTableRollHistory.front(), _randomTableStatus))
        return false;
    if (!ProjectSaveService::getInstance().saveCampaign(staged, "add GM screen random table result to session notes"))
    {
        _randomTableStatus = ProjectSaveService::getInstance().state().lastError;
        return false;
    }
    _currentCampaign = std::move(staged);
    _randomTableStatus = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
                             ? "Ergebnis zu den Sitzungsnotizen hinzugefügt."
                             : "Result added to session notes.";
    return true;
}

bool GMScreenController::createLatestRandomTableHandout()
{
    if (_randomTableRollHistory.empty())
        return false;
    Campaign staged = _currentCampaign;
    if (!RandomTables::createPlayerHandout(staged, _randomTableRollHistory.front(), _randomTableStatus))
        return false;
    if (!ProjectSaveService::getInstance().saveCampaign(staged, "create GM screen random table handout"))
    {
        _randomTableStatus = ProjectSaveService::getInstance().state().lastError;
        return false;
    }
    _currentCampaign = std::move(staged);
    _randomTableStatus = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
                             ? "Spieler-Handout erstellt; prüfe es vor der Freigabe."
                             : "Player handout created; review it before revealing.";
    return true;
}
