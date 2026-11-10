#include "Models/Character/CharacterDraftGenerator.hpp"
#include "NPCCreatorController.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/CampaignEntityDraft.hpp"

#include <algorithm>
#include <random>
#include <utility>

namespace
{
int randomInt(int minValue, int maxValue)
{
    static std::mt19937 generator{std::random_device{}()};
    std::uniform_int_distribution<int> distribution(minValue, maxValue);
    return distribution(generator);
}

const std::string &randomChoice(const std::vector<std::string> &values)
{
    return values[static_cast<std::size_t>(randomInt(0, static_cast<int>(values.size()) - 1))];
}

const std::vector<std::string> &monsterRoleTags()
{
    static const std::vector<std::string> values{"minion", "boss", "controller", "artillery", "support", "hazard", "social"};
    return values;
}
}

NPCCreatorController::NPCCreatorController(Campaign &currentCampaign) : _currentCampaign(currentCampaign)
{
    _view = std::make_unique<NPCCreatorView>(*this);
}

void NPCCreatorController::configure()
{
    _draftCampaignId = _currentCampaign.metadata.id;
    _draftCommitted = false;
    _draftEdited[0] = _draftEdited[1] = false;
    _saveError.clear();
    _sceneContext = {};
    _sceneCreated = {};
    _selectedEntryType = 0;
    _selectedNpcCategory = 0;
    _generationSpecies.clear();
    _model = NPCModel{};
    _model.spellcasting.spellAttackMode = SpellValueMode::Calculated;
    _model.spellcasting.spellSaveDcMode = SpellValueMode::Calculated;
    gm::portraits::write(_model.metadata, gm::portraits::Recipe{});
    resetMonsterDraft();

    if (_currentCampaign.ruleSet)
    {
        for (const auto &name : _currentCampaign.ruleSet->getAttributes())
            _model.attributes[name] = 10;
    }
}

void NPCCreatorController::update(std::shared_ptr<GLFWwindow> window)
{
    if (!m_isOpen)
        return;

    _view->update();
    if (!_view->isVisible())
        m_isOpen = false;
}

void NPCCreatorController::setOpenNPCOverviewHandler(std::function<void(const EntityId &)> handler)
{
    _openNPCOverviewHandler = std::move(handler);
}

void NPCCreatorController::setSceneCreationContext(CampaignSceneDraftContext context, std::function<void()> created)
{
    // A resumed draft retains the scene where it was started. Merely visiting
    // another library/scene must not silently reassign that creation request.
    if (!_sceneContext.empty() && hasUnsavedDraft()) return;
    _sceneContext = std::move(context);
    _sceneCreated = std::move(created);
}

bool NPCCreatorController::saveNPC()
{
    _draftEdited[0] = true;

    _saveError.clear();
    if (_model.name.find_first_not_of(" \t\r\n") == std::string::npos)
    {
        _saveError = gm::localization::trUi("Name is required.");
        return false;
    }
    _model.category = static_cast<NPCCategory>(_selectedNpcCategory);
    normalizeSpellcasting(_model.spellcasting);
    _model.metadata.touch();
    _model.inventory.ownerEntityId = _model.metadata.id;
    _model.inventory.name = _model.name + " – " + gm::localization::trUi("Inventory");
    if (persistCampaignEntityDraft(_currentCampaign, _model, &Campaign::npcs,
        [this](Campaign &proposed) {
            if (!linkCampaignEntityDraftToScene(proposed, _sceneContext, _model.metadata.id, "NPC", _model.name))
            {
                _saveError = gm::localization::trUi("The original session scene is unavailable. Your character draft has been kept.");
                return false;
            }
            return ProjectSaveService::getInstance().saveCampaign(proposed, "create NPC");
        }))
    {
        _draftEdited[0] = false;
        _draftCommitted = true;
        if (_sceneCreated) _sceneCreated();
        else openNPCOverview(_model.metadata.id);
        return true;
    }
    if (!_saveError.empty()) return false;
    _saveError = gm::localization::trUi("Not saved. Your draft is still here. Try Create again. ") +
        ProjectSaveService::getInstance().state().lastError;
    return false;
}

void NPCCreatorController::openNPCOverview(const EntityId &npcId)
{
    if (_openNPCOverviewHandler)
        _openNPCOverviewHandler(npcId);
}

void NPCCreatorController::generateNpcDraft()
{
    _selectedEntryType = 0;
    markDraftEdited();
    _selectedNpcCategory = std::clamp(_selectedNpcCategory, 0, 3);
    gm::creation::Generator generator(std::uint32_t(randomInt(1, 2147483647)));
    const auto races = !_generationSpecies.empty() ? std::vector<std::string>{_generationSpecies} :
        _currentCampaign.ruleSet ? _currentCampaign.ruleSet->getRaces() : std::vector<std::string>{};
    _model = generator.npc(races, static_cast<NPCCategory>(_selectedNpcCategory));
    _model.spellcasting.spellAttackMode = SpellValueMode::Calculated;
    _model.spellcasting.spellSaveDcMode = SpellValueMode::Calculated;
    std::vector<std::string> existingNames;
    for(const auto &npc : _currentCampaign.npcs) existingNames.push_back(npc.name);
    for(const auto &player : _currentCampaign.characters) existingNames.push_back(player.nameBuffer);
    _model.name = generator.uniqueName(_model.name, existingNames);
    _model.threatLevel = randomInt(0, 5);
    if (!_currentCampaign.factions.empty())
    {
        const FactionModel &faction = _currentCampaign.factions[static_cast<std::size_t>(randomInt(0, static_cast<int>(_currentCampaign.factions.size()) - 1))];
        _model.factionId = faction.metadata.id;
        _model.faction = faction.name;
    }
    if (!_currentCampaign.deities.empty() && _model.category == NPCCategory::Deity)
        _model.deityId = _currentCampaign.deities[static_cast<std::size_t>(randomInt(0, static_cast<int>(_currentCampaign.deities.size()) - 1))].metadata.id;
    if (_currentCampaign.ruleSet)
    {
        for (const auto &name : _currentCampaign.ruleSet->getAttributes())
            _model.attributes[name] = randomInt(8, 16);
    }
}

void NPCCreatorController::generateMonsterDraft()
{
    _selectedEntryType = 1;
    markDraftEdited();
    const std::vector<MonsterDefinitionModel> catalog = _currentCampaign.ruleSet ? _currentCampaign.ruleSet->getMonsters() : std::vector<MonsterDefinitionModel>{};
    if (!catalog.empty())
    {
        const MonsterDefinitionModel &base = catalog[static_cast<std::size_t>(randomInt(0, static_cast<int>(catalog.size()) - 1))];
        _monsterModel = base;
        _monsterModel.metadata = EntityMetadata{"monster"};
        _monsterModel.catalogSourceId.clear();
        _monsterModel.baseCatalogMonsterId = base.metadata.id.empty() ? base.name : base.metadata.id;
        _monsterModel.name = std::string(gm::localization::trUi("Generated")) + " " +
                             (base.name.empty() ? std::string(gm::localization::trUi("Monster")) : base.name);
        _monsterModel.source = gm::localization::trUi("Generated Draft");
    }
    else
    {
        resetMonsterDraft();
        _monsterModel.name = gm::localization::trUi("Generated Monster");
    }

    rerollMonsterRoleTags();
    rerollMonsterHooks();
    _monsterModel.challengeRating = _monsterModel.challengeRating.empty() ? "1" : _monsterModel.challengeRating;
    rerollMonsterNumbers();
}

void NPCCreatorController::rerollNpcName()
{
    markDraftEdited();

    gm::creation::Generator generator(std::uint32_t(randomInt(1, 2147483647)));
    std::vector<std::string> existingNames;
    for(const auto &npc : _currentCampaign.npcs) existingNames.push_back(npc.name);
    for(const auto &player : _currentCampaign.characters) existingNames.push_back(player.nameBuffer);
    _model.name = generator.uniqueName(generator.name(), existingNames);
}

void NPCCreatorController::rerollNpcRole()
{
    markDraftEdited();

    // Role, goal, resources and secret form one scenario; keep them coherent.
    gm::creation::Generator generator(std::uint32_t(randomInt(1, 2147483647)));
    const auto scenario = generator.npc({}, _model.category);
    _model.role = scenario.role;
    _model.goals = scenario.goals;
    _model.resources = scenario.resources;
    _model.threatLocation = scenario.threatLocation;
    _model.secrets = scenario.secrets;
}

void NPCCreatorController::rerollNpcHooks()
{
    rerollNpcRole();
}

void NPCCreatorController::rerollNpcSecret()
{
    markDraftEdited();
    gm::creation::Generator generator(std::uint32_t(randomInt(1, 2147483647)));
    static const char *const secrets[] = {
        "Their public name is an alias used to protect someone close to them.",
        "A creditor has demanded a favor they cannot openly discuss.",
        "They secretly support a relative on the other side of a local dispute.",
        "They plan to leave town after their current business is settled.",
        "They have hidden a letter that could repair a broken friendship.",
        "They once testified under pressure and want to set the record straight."
    };
    _model.secrets = generator.pick(secrets);
}
void NPCCreatorController::rerollMonsterRoleTags()
{
    markDraftEdited();

    _monsterModel.encounterRoleTags = {randomChoice(monsterRoleTags())};
}

void NPCCreatorController::rerollMonsterHooks()
{
    markDraftEdited();

    _monsterModel.traits = gm::localization::trUi("Generated hook: give this creature one table-specific behavior.");
    _monsterModel.rechargeActions = gm::localization::trUi("Optional recharge hook based on the encounter's terrain or objective.");
}

void NPCCreatorController::rerollMonsterNumbers()
{
    markDraftEdited();

    _monsterModel.armorClass = std::max(1, _monsterModel.armorClass + randomInt(-1, 1));
    _monsterModel.hitPoints = std::max(1, _monsterModel.hitPoints + randomInt(-5, 10));
}

void NPCCreatorController::addNpcSpell()
{
    markDraftEdited();

    if (_currentCampaign.customSpells.empty())
        return;
    if (_selectedNpcSpellIndex < 0 || _selectedNpcSpellIndex >= static_cast<int>(_currentCampaign.customSpells.size()))
        _selectedNpcSpellIndex = 0;

    const SpellModel &spell = _currentCampaign.customSpells[static_cast<std::size_t>(_selectedNpcSpellIndex)];
    const auto found = std::find_if(_model.spellcasting.spells.begin(), _model.spellcasting.spells.end(), [&](const CharacterSpellEntryModel &entry) {
        return entry.spellId == spell.metadata.id;
    });
    if (found != _model.spellcasting.spells.end())
        return;

    CharacterSpellEntryModel entry;
    entry.spellId = spell.metadata.id;
    entry.prepared = true;
    _model.spellcasting.enabled = true;
    _model.spellcasting.spells.push_back(entry);
}

void NPCCreatorController::resetMonsterDraft()
{
    _monsterModel = MonsterDefinitionModel{};
    _monsterModel.name = gm::localization::trUi("New Monster");
    _monsterModel.source = "Campaign";
    _monsterModel.size = "Medium";
    _monsterModel.type = "Creature";
    _monsterModel.challengeRating = "0";
    _monsterModel.armorClass = 10;
    _monsterModel.armorDamageReduction = 0;
    _monsterModel.hitPoints = 1;
    _monsterModel.speedFeet = 30;
    if (_currentCampaign.ruleSet)
    {
        for (const auto &name : _currentCampaign.ruleSet->getAttributes())
            _monsterModel.attributes[name] = 10;
    }

    _monsterActionEdited = false;
    _monsterAction = MonsterActionModel{};
    _monsterAction.name = gm::localization::trUi("Attack");
}

void NPCCreatorController::addMonsterAction()
{
    markDraftEdited();

    if (_monsterAction.name.empty())
        _monsterAction.name = gm::localization::trUi("Attack");
    _monsterModel.actions.push_back(_monsterAction);
    _monsterActionEdited = false;
    _monsterAction = MonsterActionModel{};
    _monsterAction.name = gm::localization::trUi("Attack");
}

bool NPCCreatorController::saveMonster()
{
    _draftEdited[1] = true;
    if (_monsterActionEdited) addMonsterAction();

    _saveError.clear();
    if (_monsterModel.name.empty())
        _monsterModel.name = gm::localization::trUi("Untitled Monster");
    _monsterModel.source = "Campaign";
    _monsterModel.metadata.touch();
    if (persistCampaignEntityDraft(_currentCampaign, _monsterModel, &Campaign::customMonsters,
        [this](Campaign &proposed) {
            if (!linkCampaignEntityDraftToScene(proposed, _sceneContext, _monsterModel.metadata.id, "Monster", _monsterModel.name))
            {
                _saveError = gm::localization::trUi("The original session scene is unavailable. Your character draft has been kept.");
                return false;
            }
            return ProjectSaveService::getInstance().saveCampaign(proposed, "create monster");
        }))
    {
        _draftEdited[1] = false;
        _draftCommitted = true;
        if (_sceneCreated) _sceneCreated();
        return true;
    }
    if (!_saveError.empty()) return false;
    _saveError = gm::localization::trUi("Not saved. Your draft is still here. Try Create again. ") +
        ProjectSaveService::getInstance().state().lastError;
    return false;
}

void NPCCreatorController::onShow()
{
    _view->show();
    _view->requestDockOnNextUpdate();
}


void NPCCreatorController::resumeDraft()
{
    if (_draftCampaignId != _currentCampaign.metadata.id || (_draftCommitted && !hasUnsavedDraft())) configure();
}

bool NPCCreatorController::flushDraft()
{
    const int original = _selectedEntryType;
    for (int type = 0; type < 2; ++type)
    {
        if (!_draftEdited[type]) continue;
        _selectedEntryType = type;
        if (!(type == 1 ? saveMonster() : saveNPC())) return false;
    }
    _selectedEntryType = original;
    return true;
}
