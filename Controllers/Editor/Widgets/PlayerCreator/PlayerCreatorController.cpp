#include "PlayerCreatorController.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/CampaignEntityDraft.hpp"
#include "Models/Localization/LocalizationService.hpp"

#include <utility>
#include <algorithm>
#include "Models/Character/PortraitRecipe.hpp"
#include "Models/Character/CharacterHitPointEstimate.hpp"

PlayerCreatorController::PlayerCreatorController(Campaign &currentCampaign) : _currentCampaign(currentCampaign)
{
    _view = std::make_unique<PlayerCreatorView>(*this);
}

void PlayerCreatorController::configure()
{
    _draftCampaignId = _currentCampaign.metadata.id;
    _draftCommitted = false;
    _draftEdited = false;
    _saveError.clear();
    _sceneContext = {};
    _sceneCreated = {};
    if (!_currentCampaign.ruleSet)
        return;

    _creationStep = 0;
    _model = CharacterModel{};
    _model.spellcasting.spellAttackMode = SpellValueMode::Calculated;
    _model.spellcasting.spellSaveDcMode = SpellValueMode::Calculated;
    for (const auto &name : _currentCampaign.ruleSet->getAttributes())
        _model.attributes[name] = 10;

    const auto classes = _currentCampaign.ruleSet->getClasses();
    if (!classes.empty())
        _model.selectedClass = classes.front();

    const auto races = _currentCampaign.ruleSet->getRaces();
    if (!races.empty())
        _model.selectedRace = races.front();
    gm::portraits::Recipe portrait;
    portrait.species = gm::portraits::speciesForRace(_model.selectedRace);
    gm::portraits::write(_model.metadata, portrait);
}

void PlayerCreatorController::update(std::shared_ptr<GLFWwindow> window)
{
    if (!m_isOpen)
        return;

    _view->update();
    if (!_view->isVisible())
        m_isOpen = false;
}

void PlayerCreatorController::setOpenCharacterSheetsHandler(std::function<void()> handler)
{
    _openCharacterSheetsHandler = std::move(handler);
}

void PlayerCreatorController::setOpenPlayerOverviewHandler(std::function<void()> handler)
{
    _openPlayerOverviewHandler = std::move(handler);
}

void PlayerCreatorController::setSceneCreationContext(CampaignSceneDraftContext context, std::function<void()> created)
{
    // A resumed draft retains the scene where it was started. Merely visiting
    // another library/scene must not silently reassign that creation request.
    if (!_sceneContext.empty() && !_draftCommitted) return;
    _sceneContext = std::move(context);
    _sceneCreated = std::move(created);
}

bool PlayerCreatorController::savePlayer()
{
    markDraftEdited();

    _saveError.clear();
    if (std::string(_model.nameBuffer).find_first_not_of(" \t\r\n") == std::string::npos)
    {
        _saveError = gm::localization::trUi("Name is required.");
        return false;
    }
    for (auto &[name, value] : _model.attributes)
        value = std::clamp(value, 1, 30);
    if (_model.maxHitPoints <= 0)
    {
        _model.maxHitPoints = estimateNewCharacterMaxHitPoints(_model, _currentCampaign.ruleSet.get());
        if (_model.currentHitPoints <= 0)
            _model.currentHitPoints = _model.maxHitPoints;
    }
    normalizeCharacterHitPoints(_model);
    normalizeSpellcasting(_model.spellcasting);
    _model.metadata.touch();
    _model.inventory.ownerEntityId = _model.metadata.id;
    _model.inventory.name = std::string(_model.nameBuffer) + " – " + gm::localization::trUi("Inventory");
    if (persistCampaignEntityDraft(_currentCampaign, _model, &Campaign::characters,
        [this](Campaign &proposed) {
            if (!linkCampaignEntityDraftToScene(proposed, _sceneContext, _model.metadata.id, "Player", _model.nameBuffer))
            {
                _saveError = gm::localization::trUi("The original session scene is unavailable. Your character draft has been kept.");
                return false;
            }
            return ProjectSaveService::getInstance().saveCampaign(proposed, "create player");
        }))
    {
        _draftEdited = false;
        _draftCommitted = true;
        if (_sceneCreated) _sceneCreated();
        else openCharacterSheets();
        return true;
    }
    if (!_saveError.empty()) return false;
    _saveError = gm::localization::trUi("Not saved. Your draft is still here. Try Create again. ") +
        ProjectSaveService::getInstance().state().lastError;
    return false;
}

void PlayerCreatorController::openCharacterSheets()
{
    if (_openCharacterSheetsHandler)
        _openCharacterSheetsHandler();
}

void PlayerCreatorController::openPlayerOverview()
{
    if (_openPlayerOverviewHandler)
        _openPlayerOverviewHandler();
}

void PlayerCreatorController::onShow()
{
    _view->show();
    _view->requestDockOnNextUpdate();
}


void PlayerCreatorController::resumeDraft()
{
    if (_draftCampaignId != _currentCampaign.metadata.id || _draftCommitted) configure();
}

bool PlayerCreatorController::flushDraft()
{
    if (!_draftEdited)
        return true;

    // A campaign-level save must never turn a half-finished creation draft
    // into a real player. Keep the draft intact and let the user explicitly
    // Create Player or discard it in the creator.
    _saveError = gm::localization::trUi(
        "Finish creating this player or discard the draft before saving the campaign.");
    return false;
}

void PlayerCreatorController::discardDraft()
{
    configure();
}
