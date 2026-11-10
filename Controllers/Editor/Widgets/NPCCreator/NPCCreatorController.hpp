#pragma once

#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Models/NPC/NPCModel.hpp"
#include "Views/Editor/Widgets/Npc/NPCCreator/NPCCreatorView.hpp"
#include <functional>
#include "Models/Project/CampaignEntityDraft.hpp"

class NPCCreatorController : public IWidgetController
{
public:
    explicit NPCCreatorController(Campaign &currentCampaign);

    void update(std::shared_ptr<GLFWwindow> window) override;
    void configure();
    void resumeDraft();
    void markDraftEdited() { _draftEdited[_selectedEntryType == 1 ? 1 : 0] = true; _draftCommitted = false; }
    bool hasUnsavedDraft() const { return _draftEdited[0] || _draftEdited[1]; }
    bool flushDraft();

    NPCModel &getModel() { return _model; }
    MonsterDefinitionModel &getMonsterModel() { return _monsterModel; }
    MonsterActionModel &getMonsterAction() { return _monsterAction; }
    void setOpenNPCOverviewHandler(std::function<void(const EntityId &)> handler);
    void setSceneCreationContext(CampaignSceneDraftContext context, std::function<void()> created);

private:
    EntityId _draftCampaignId;
    bool _draftCommitted = false;
    bool _draftEdited[2] = {false, false};
    bool _monsterActionEdited = false;
    friend class NPCCreatorView;
    friend struct ActorDraftWorkflowProbe;

    NPCModel _model;
    MonsterDefinitionModel _monsterModel;
    MonsterActionModel _monsterAction;
    std::unique_ptr<NPCCreatorView> _view;
    Campaign &_currentCampaign;
    std::function<void(const EntityId &)> _openNPCOverviewHandler;
    int _selectedEntryType = 0;
    int _selectedNpcCategory = 0;
    std::string _generationSpecies;
    int _selectedNpcSpellIndex = 0;
    std::string _saveError;
    CampaignSceneDraftContext _sceneContext;
    std::function<void()> _sceneCreated;

    bool saveNPC();
    bool saveMonster();
    void openNPCOverview(const EntityId &npcId = {});
    void generateNpcDraft();
    void generateMonsterDraft();
    void rerollNpcName();
    void rerollNpcRole();
    void rerollNpcHooks();
    void rerollNpcSecret();
    void rerollMonsterRoleTags();
    void rerollMonsterHooks();
    void rerollMonsterNumbers();
    void addNpcSpell();
    void addMonsterAction();
    void resetMonsterDraft();
    void onShow() override;
};
