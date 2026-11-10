#pragma once

#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Models/Map/TacticalMapGenerator.hpp"
#include "Views/Editor/Widgets/Combat/EncounterBuilder/EncounterBuilderView.hpp"
#include <cstdint>
#include <memory>
#include <functional>
#include <future>
#include <atomic>
#include <vector>
#include <map>

class EncounterBuilderController : public IWidgetController
{
public:
    explicit EncounterBuilderController(Campaign &currentCampaign);
    ~EncounterBuilderController() override;

    void update(std::shared_ptr<GLFWwindow> window) override;
    void setStageHandler(std::function<bool(EntityId)> handler) { _stageHandler = std::move(handler); }
    bool beginNewEncounterDraft();
    void resetDrafts();
    void markDraftEdited() { _draftEdited = true; _saveSucceeded = false; }
    bool hasUnsavedDrafts() const;
    bool flushDrafts();
    void setMapAuthoringHandler(std::function<bool(EntityId, EntityId)> handler) { _mapAuthoringHandler = std::move(handler); }
    bool openSharedMapAuthoring(bool createNew);
    bool assignSavedMap(const EntityId &encounterId, const EntityId &mapId);
    bool selectEncounterById(const EntityId &encounterId);
    void setPreparationSaveHandlers(std::function<bool()> beforeSave, std::function<void(EntityId)> afterSave)
    { _beforeSaveHandler = std::move(beforeSave); _savedHandler = std::move(afterSave); }

private:
    friend class EncounterBuilderView;
    friend struct EncounterWorkflowProbe;
    friend struct BattleTrackerPreparationProbe;

    std::unique_ptr<EncounterBuilderView> _view;
    Campaign &_currentCampaign;
    EncounterModel _draftEncounter;
    int _selectedEncounterIndex = -1;
    int _selectedCharacterIndex = 0;
    int _selectedNPCIndex = 0;
    int _selectedMonsterIndex = 0;
    int _selectedParticipantIndex = -1;
    int _selectedAtlasMapIndex = 0;
    int _tacticalTemplateIndex = static_cast<int>(TacticalMapTemplate::OpenGround);
    int _tacticalWidth = 40;
    int _tacticalHeight = 30;
    int _tacticalSizePresetIndex = 1;
    int _tacticalCoverDensity = 24;
    int _tacticalObjectiveCount = 1;
    std::uint32_t _tacticalSeed = 240529u;
    bool _keepTacticalSeed = false;
    int _selectedTacticalCandidate = -1;
    bool _useAdvancedHexGrid = false;
    std::string _tacticalRequiredElements;
    std::string _tacticalStatus;
    std::vector<TacticalMapCandidate> _tacticalCandidates;
    std::future<std::vector<TacticalMapCandidate>> _tacticalGeneration;
    EntityId _tacticalGenerationEncounterId;
    std::shared_ptr<std::atomic_bool> _tacticalCancellation;
    void cancelTacticalGeneration();
    void resetTacticalDraftContext();
    void pollTacticalGeneration();

    void startNewEncounter();
    void loadSelectedEncounter();
    bool saveEncounter();
    bool _saveFailed = false, _saveSucceeded = false;
    bool _explicitNewEncounterDraft = false;
    bool _saveBlockedByCombat = false;
    EntityId _persistedDraftId;
    bool _draftEdited = false;
    struct RetainedEncounterDraft { EncounterModel encounter; EntityId persistedId; bool dirty = false; };
    std::map<EntityId, RetainedEncounterDraft> _retainedEncounterDrafts;
    void retainCurrentEncounterDraft();
    bool restoreRetainedEncounterDraft(const EntityId &id);
    std::function<bool(EntityId)> _stageHandler;
    std::function<bool(EntityId, EntityId)> _mapAuthoringHandler;
    std::function<bool()> _beforeSaveHandler;
    std::function<void(EntityId)> _savedHandler;
    void addSelectedPlayer();
    void addAllPlayers();
    void addSelectedNPC();
    void addSelectedMonster();
    void removeSelectedParticipant();
    void synchronizeParticipantsWithSources();
    bool useSelectedAtlasTacticalMap();
    void generateTacticalCandidates();
    bool useSelectedTacticalCandidate();
    std::vector<MonsterDefinitionModel> getMonsterCatalog() const;
    std::vector<ItemModel> getAvailableItems() const;
    int findOpenX() const;
    int findOpenY() const;
    bool saveCampaign();
    void onShow() override;
};
