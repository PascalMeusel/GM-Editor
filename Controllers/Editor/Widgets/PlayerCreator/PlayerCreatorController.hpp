#pragma once

#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Models/Character/CharacterModel.hpp"
#include "Views/Editor/Widgets/Player/PlayerCreator/PlayerCreatorView.hpp"
#include <functional>
#include "Models/Project/CampaignEntityDraft.hpp"

class PlayerCreatorController : public IWidgetController
{
public:
    explicit PlayerCreatorController(Campaign &currentCampaign);

    void update(std::shared_ptr<GLFWwindow> window) override;
    void configure();
    void resumeDraft();
    void markDraftEdited() { _draftEdited = true; _draftCommitted = false; }
    bool hasUnsavedDraft() const { return _draftEdited; }
    bool flushDraft();
    void discardDraft();

    CharacterModel &getModel() { return _model; }
    void setOpenCharacterSheetsHandler(std::function<void()> handler);
    void setOpenPlayerOverviewHandler(std::function<void()> handler);
    void setSceneCreationContext(CampaignSceneDraftContext context, std::function<void()> created);

private:
    EntityId _draftCampaignId;
    bool _draftCommitted = false;
    bool _draftEdited = false;
    friend class PlayerCreatorView;

    CharacterModel _model;
    std::unique_ptr<PlayerCreatorView> _view;
    Campaign &_currentCampaign;
    std::function<void()> _openCharacterSheetsHandler;
    std::function<void()> _openPlayerOverviewHandler;
    int _creationStep = 0;
    std::string _saveError;
    CampaignSceneDraftContext _sceneContext;
    std::function<void()> _sceneCreated;

    bool savePlayer();
    void openCharacterSheets();
    void openPlayerOverview();
    void onShow() override;
};
