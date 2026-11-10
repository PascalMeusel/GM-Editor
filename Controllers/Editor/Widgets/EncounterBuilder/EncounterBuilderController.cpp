#include "EncounterBuilderController.hpp"
#include "Models/Map/MapAtlasService.hpp"
#include "Models/Map/CurrentMapArtRecipe.hpp"
#include "Models/UX/CampaignNavigationResolver.hpp"
#include "Models/Map/TravelSceneGenerator.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Rulesets/CharacterRulesService.hpp"
#include "Models/Combat/EncounterMapEligibility.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <optional>
#include <random>
#include <sstream>

namespace
{
void refreshHitPointBounds(EncounterParticipantModel &participant, int newMaxHitPoints)
{
    const bool hadNoStoredMaximum = participant.maxHitPoints <= 0;
    participant.maxHitPoints = std::max(1, newMaxHitPoints);
    if (hadNoStoredMaximum)
        participant.currentHitPoints = participant.maxHitPoints;
    else
        participant.currentHitPoints = std::clamp(participant.currentHitPoints, 0, participant.maxHitPoints);
    participant.defeated = participant.currentHitPoints == 0;
}

std::vector<MonsterActionModel> getInventoryPowerActions(const InventoryModel &inventory, const std::vector<ItemModel> &items)
{
    std::vector<MonsterActionModel> actions;
    for (const InventoryEntryModel &entry : inventory.entries)
    {
        if (!entry.equipped && !entry.attuned)
            continue;
        const ItemModel *item = findItemById(items, entry.itemId);
        if (!item)
            continue;
        for (MonsterActionModel action : item->powers)
        {
            if (action.name.empty())
                action.name = item->name.empty() ? "Item Power" : item->name;
            if (action.actionKind.empty() || action.actionKind == "Attack")
                action.actionKind = "Item Power";
            action.sourceLabel = item->name;
            action.sourceEntityId = item->metadata.id;
            actions.push_back(action);
        }
    }
    return actions;
}

std::vector<MonsterActionModel> getCharacterFeatureActions(const CharacterModel &character)
{
    std::vector<MonsterActionModel> actions = character.featureActions;
    for (MonsterActionModel &action : actions)
    {
        if (action.actionKind.empty() || action.actionKind == "Attack")
            action.actionKind = "Feature";
        action.sourceLabel = std::string(character.nameBuffer);
        action.sourceEntityId = character.metadata.id;
    }
    return actions;
}

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string trimCopy(const std::string &value)
{
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])))
        ++start;
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
        --end;
    return value.substr(start, end - start);
}

std::vector<std::string> splitDamageTraits(const std::string &traits)
{
    std::vector<std::string> values;
    std::stringstream stream(traits);
    std::string value;
    while (std::getline(stream, value, ','))
    {
        value = lowerCopy(trimCopy(value));
        if (!value.empty())
            values.push_back(value);
    }
    return values;
}
}

EncounterBuilderController::EncounterBuilderController(Campaign &currentCampaign) : _currentCampaign(currentCampaign)
{
    _view = std::make_unique<EncounterBuilderView>(*this);
    startNewEncounter();
    _explicitNewEncounterDraft = false;
}

EncounterBuilderController::~EncounterBuilderController()
{
    cancelTacticalGeneration();
}

void EncounterBuilderController::update(std::shared_ptr<GLFWwindow> window)
{
    (void)window;
    pollTacticalGeneration();
    if (!m_isOpen)
    {
        cancelTacticalGeneration();
        return;
    }

    synchronizeParticipantsWithSources();
    _view->update();
    if (!_view->isVisible())
    {
        m_isOpen = false;
        cancelTacticalGeneration();
    }
}

bool EncounterBuilderController::selectEncounterById(const EntityId &encounterId)
{
    if (_saveFailed && encounterId != _draftEncounter.metadata.id) return false;
    if (!gm::ux::resolveNavigationEntity(_currentCampaign, encounterId).available) return false;
    for (int i = 0; i < static_cast<int>(_currentCampaign.encounters.size()); ++i)
    {
        if (_currentCampaign.encounters[static_cast<std::size_t>(i)].metadata.id == encounterId)
        {
            _selectedEncounterIndex = i;
            loadSelectedEncounter();
            return true;
        }
    }
    return false;
}

void EncounterBuilderController::loadSelectedEncounter()
{
    if (_selectedEncounterIndex < 0 || _selectedEncounterIndex >= static_cast<int>(_currentCampaign.encounters.size()))
        return;

    const EntityId targetId = _currentCampaign.encounters[_selectedEncounterIndex].metadata.id;
    if (targetId == _draftEncounter.metadata.id) return;
    if (_saveFailed)
    {
        const auto previous = std::find_if(_currentCampaign.encounters.begin(), _currentCampaign.encounters.end(),
            [&](const EncounterModel &encounter) { return encounter.metadata.id == _draftEncounter.metadata.id; });
        _selectedEncounterIndex = previous == _currentCampaign.encounters.end() ? -1 :
            static_cast<int>(std::distance(_currentCampaign.encounters.begin(), previous));
        return;
    }
    retainCurrentEncounterDraft();
    resetTacticalDraftContext();
    const auto retained = _retainedEncounterDrafts.find(targetId);
    if (retained != _retainedEncounterDrafts.end())
    {
        _draftEncounter = retained->second.encounter;
        _persistedDraftId = retained->second.persistedId;
        _draftEdited = retained->second.dirty;
    }
    else
    {
        _draftEncounter = _currentCampaign.encounters[_selectedEncounterIndex];
        _persistedDraftId = _draftEncounter.metadata.id;
        _draftEdited = false;
    }
    normalizeEncounter(_draftEncounter);
    synchronizeParticipantsWithSources();
    _selectedParticipantIndex = _draftEncounter.participants.empty() ? -1 : 0;
}



bool EncounterBuilderController::useSelectedAtlasTacticalMap()
{
    if (_currentCampaign.maps.empty())
        return false;
    _selectedAtlasMapIndex = std::clamp(_selectedAtlasMapIndex, 0, static_cast<int>(_currentCampaign.maps.size()) - 1);
    const MapModel &map = _currentCampaign.maps[static_cast<std::size_t>(_selectedAtlasMapIndex)];
    if (!isEncounterScaleMap(map))
        return false;
    if (!encounterMapGridSupported(map))
    {
        _tacticalStatus = "The map needs a complete grid between 4 and 512 cells per side.";
        return false;
    }
    _draftEncounter.mapId = map.metadata.id;
    markDraftEdited();
    _draftEncounter.tacticalMapId = map.metadata.id;
    _draftEncounter.sourceMapId = MapAtlasService::effectiveParentId(map);
    _draftEncounter.sourceMapRect = map.atlasPlacement.parentRect;
    _draftEncounter.sourceMapRevision = map.atlasPlacement.parentRevision;
    _draftEncounter.mapContextName = map.name;
    _draftEncounter.battlefieldWidth = map.width;
    _draftEncounter.battlefieldHeight = map.height;
    _draftEncounter.battlefieldCells = map.cells;
    _draftEncounter.useMapCells = true;
    _draftEncounter.theaterOfMind = false;
    normalizeEncounter(_draftEncounter);
    _tacticalStatus.clear();
    return true;
}

void EncounterBuilderController::generateTacticalCandidates()
{
    if (_tacticalGeneration.valid())
        return;
    if (!_keepTacticalSeed)
        _tacticalSeed = std::random_device{}();
    _tacticalCancellation = std::make_shared<std::atomic_bool>(false);
    TacticalMapRecipe recipe;
    recipe.cancellationRequested = [token = _tacticalCancellation]() { return token->load(); };
    recipe.generatorVersion = 12;
    recipe.illustrationRecipeVersion = kCurrentMapIllustrationRecipe;
    recipe.mapTemplate = static_cast<TacticalMapTemplate>(std::clamp(_tacticalTemplateIndex, 0, 7));
    recipe.width = std::clamp(_tacticalWidth, 12, 80);
    recipe.height = std::clamp(_tacticalHeight, 10, 80);
    recipe.gridType = _useAdvancedHexGrid ? GridType::Hex : GridType::Square;
    recipe.seed = _tacticalSeed;
    recipe.coverDensity = std::clamp(_tacticalCoverDensity, 0, 60);
    recipe.objectiveCount = std::clamp(_tacticalObjectiveCount, 1, 3);
    recipe.requiredElements = TravelSceneGenerator::parseElementList(_tacticalRequiredElements);

    const MapModel *source = nullptr;
    if (!_currentCampaign.maps.empty())
    {
        _selectedAtlasMapIndex = std::clamp(_selectedAtlasMapIndex, 0, static_cast<int>(_currentCampaign.maps.size()) - 1);
        const MapModel &selected = _currentCampaign.maps[static_cast<std::size_t>(_selectedAtlasMapIndex)];
        if (selected.atlasLevel == MapLevel::Region || selected.atlasLevel == MapLevel::Site)
            source = &selected;
    }
    // The worker owns immutable recipe/context copies. It never reads the live
    // campaign, publishes a map, or replaces the encounter being edited.
    std::optional<MapModel> context = source ? std::optional<MapModel>(*source) : std::nullopt;
    _tacticalGenerationEncounterId = _draftEncounter.metadata.id;
    _tacticalCandidates.clear();
    _selectedTacticalCandidate = -1;
    _tacticalStatus = gm::localization::trUi("Creating battlefield designs…");
    try
    {
        _tacticalGeneration = std::async(std::launch::async, [recipe, context = std::move(context)]() {
            return TacticalMapGenerator::generate(recipe, context ? &*context : nullptr, 3);
        });
    }
    catch (const std::exception &error)
    {
        _tacticalStatus = std::string(gm::localization::trUi("Could not create battlefield designs. ")) + error.what();
    }
}

void EncounterBuilderController::pollTacticalGeneration()
{
    if (!_tacticalGeneration.valid() ||
        _tacticalGeneration.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;
    const bool sameEncounter = !_tacticalGenerationEncounterId.empty() &&
        _draftEncounter.metadata.id == _tacticalGenerationEncounterId &&
        !_tacticalCancellation->load();
    try
    {
        auto candidates = _tacticalGeneration.get();
        if (!sameEncounter)
            return;
        _tacticalCandidates = std::move(candidates);
        _selectedTacticalCandidate = _tacticalCandidates.empty() ? -1 : 0;
        _tacticalStatus = gm::localization::trUi(_tacticalCandidates.empty()
            ? "No playable battlefield design was generated." : "Battlefield designs are ready to review.");
    }
    catch (const std::exception &error)
    {
        if (sameEncounter)
            _tacticalStatus = std::string(gm::localization::trUi("Could not create battlefield designs. ")) + error.what();
    }
}

bool EncounterBuilderController::useSelectedTacticalCandidate()
{
    if (_selectedTacticalCandidate < 0 || _selectedTacticalCandidate >= static_cast<int>(_tacticalCandidates.size()))
        return false;
    const TacticalMapCandidate &selectedCandidate = _tacticalCandidates[static_cast<std::size_t>(_selectedTacticalCandidate)];
    MapModel map = selectedCandidate.map;
    if (!_tacticalCandidates[static_cast<std::size_t>(_selectedTacticalCandidate)].readinessIssues.empty())
    {
        _tacticalStatus = gm::localization::trUi("Resolve battlefield readiness issues before using this design.");
        return false;
    }

    map.name = std::string(gm::localization::trUi(selectedCandidate.templateLabel.c_str())) + " #" + std::to_string(selectedCandidate.rank);
    const std::string baseId = map.metadata.id;
    int suffix = 2;
    auto idExists = [&](const EntityId &id) {
        return std::any_of(_currentCampaign.maps.begin(), _currentCampaign.maps.end(), [&](const MapModel &existing) { return existing.metadata.id == id; });
    };
    while (idExists(map.metadata.id))
        map.metadata.id = baseId + "::" + std::to_string(suffix++);
    const auto previousEncounter = _draftEncounter;
    const auto previousMetadata = _currentCampaign.metadata;
    const int previousAtlasSelection = _selectedAtlasMapIndex;
    _currentCampaign.maps.push_back(map);
    _selectedAtlasMapIndex = static_cast<int>(_currentCampaign.maps.size()) - 1;
    if (!useSelectedAtlasTacticalMap())
    {
        _currentCampaign.maps.pop_back();
        _draftEncounter = previousEncounter;
        _selectedAtlasMapIndex = previousAtlasSelection;
        _tacticalStatus = gm::localization::trUi("The selected battlefield could not be attached to the encounter.");
        return false;
    }
    const bool hasDefaultName = _draftEncounter.name == "New Encounter" || _draftEncounter.name == "Neue Begegnung";
    _draftEncounter.name = hasDefaultName ? map.name : _draftEncounter.name;
    if (!saveCampaign())
    {
        _currentCampaign.maps.pop_back();
        _currentCampaign.metadata = previousMetadata;
        _draftEncounter = previousEncounter;
        _selectedAtlasMapIndex = previousAtlasSelection;
        _tacticalStatus = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
            ? "Nicht gespeichert. Die Entwürfe bleiben hier. Wähle diesen Entwurf erneut, um es noch einmal zu versuchen."
            : "Not saved. Your designs remain here. Choose this design again to retry.";
        return false;
    }
    _tacticalCandidates.clear();
    _selectedTacticalCandidate = -1;
    _tacticalStatus = gm::localization::trUi("The battlefield is linked. Add participants and save encounter prep.");
    return true;
}
