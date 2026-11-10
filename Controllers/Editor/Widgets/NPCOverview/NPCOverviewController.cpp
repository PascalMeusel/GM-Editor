#include "NPCOverviewController.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include <algorithm>
#include <cctype>
#include <utility>

namespace
{
std::string uiText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? german : english;
}

bool hasTag(const EntityMetadata &metadata, const std::string &tag)
{
    return std::find(metadata.tags.begin(), metadata.tags.end(), tag) != metadata.tags.end();
}

void addTag(EntityMetadata &metadata, const std::string &tag)
{
    if (!hasTag(metadata, tag))
        metadata.tags.push_back(tag);
}

std::string trimCopy(const std::string &value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    return first >= last ? std::string{} : std::string(first, last);
}

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

int findFactionIndexByName(const std::vector<FactionModel> &factions, const std::string &name)
{
    const std::string needle = lowerCopy(trimCopy(name));
    if (needle.empty())
        return -1;

    for (int i = 0; i < static_cast<int>(factions.size()); ++i)
    {
        if (lowerCopy(trimCopy(factions[static_cast<std::size_t>(i)].name)) == needle)
            return i;
    }
    return -1;
}

int findDeityIndexByName(const std::vector<DeityModel> &deities, const std::string &name)
{
    const std::string needle = lowerCopy(trimCopy(name));
    if (needle.empty())
        return -1;

    for (int i = 0; i < static_cast<int>(deities.size()); ++i)
    {
        if (lowerCopy(trimCopy(deities[static_cast<std::size_t>(i)].name)) == needle)
            return i;
    }
    return -1;
}
}

NPCOverviewController::NPCOverviewController(Campaign &currentCampaign) : _currentCampaign(currentCampaign)
{
    _monsterActionDraft.name = gm::localization::trUi("Attack");
    _view = std::make_unique<NPCOverviewView>(*this);
}

void NPCOverviewController::update(std::shared_ptr<GLFWwindow> window)
{
    if (!m_isOpen)
        return;

    refreshSaveContext();
    _view->update();
    if (!_view->isVisible())
        m_isOpen = false;
}

bool NPCOverviewController::selectEntityById(const EntityId &entityId, const std::string &entityType)
{
    if (entityType == "NPC")
    {
        for (int index = 0; index < static_cast<int>(_context.campaign.npcs.size()); ++index)
            if (_context.campaign.npcs[static_cast<std::size_t>(index)].metadata.id == entityId)
            {
                _selectedNpcIndex = index;
                return true;
            }
    }
    else if (entityType == "Monster")
    {
        for (int index = 0; index < static_cast<int>(_context.campaign.customMonsters.size()); ++index)
            if (_context.campaign.customMonsters[static_cast<std::size_t>(index)].metadata.id == entityId)
            {
                _selectedMonsterIndex = index;
                return true;
            }
    }
    else if (entityType == "Faction")
    {
        for (int index = 0; index < static_cast<int>(_context.campaign.factions.size()); ++index)
            if (_context.campaign.factions[static_cast<std::size_t>(index)].metadata.id == entityId)
            {
                _selectedFactionIndex = index;
                return true;
            }
    }
    else if (entityType == "Deity")
    {
        for (int index = 0; index < static_cast<int>(_context.campaign.deities.size()); ++index)
            if (_context.campaign.deities[static_cast<std::size_t>(index)].metadata.id == entityId)
            {
                _selectedDeityIndex = index;
                return true;
            }
    }
    return false;
}

void NPCOverviewController::setOpenStoryThreadHandler(std::function<void(const EntityId &)> handler)
{
    _context.openStoryThreadHandler = std::move(handler);
}

void NPCOverviewController::openStoryThread(const EntityId &threadId)
{
    if (_context.openStoryThreadHandler)
        _context.openStoryThreadHandler(threadId);
}

void NPCOverviewController::duplicateSelectedNpc()
{
    if (retryPendingSave())
        return;

    if (_selectedNpcIndex < 0 || _selectedNpcIndex >= static_cast<int>(_context.campaign.npcs.size()))
        return;

    const NPCModel &source = _context.campaign.npcs[_selectedNpcIndex];
    NPCModel copy = source;
    copy.metadata = EntityMetadata{"npc"};
    copy.metadata.tags = source.metadata.tags;
    copy.name = std::string(gm::localization::trUi("Copy of")) + " " +
                (source.name.empty() ? std::string(gm::localization::trUi("NPC")) : source.name);
    copy.inventory.metadata = EntityMetadata{"inventory"};
    copy.inventory.ownerEntityId = copy.metadata.id;
    copy.inventory.name = copy.name + " Inventory";
    normalizeSpellcasting(copy.spellcasting);

    _context.campaign.npcs.insert(_context.campaign.npcs.begin() + _selectedNpcIndex + 1, copy);
    ++_selectedNpcIndex;
    _context.campaign.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::duplicateSelectedMonster()
{
    if (retryPendingSave())
        return;

    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
        return;

    const MonsterDefinitionModel &source = _context.campaign.customMonsters[_selectedMonsterIndex];
    MonsterDefinitionModel copy = source;
    copy.metadata = EntityMetadata{"monster"};
    copy.metadata.tags = source.metadata.tags;
    copy.name = std::string(gm::localization::trUi("Copy of")) + " " +
                (source.name.empty() ? std::string(gm::localization::trUi("Monster")) : source.name);

    _context.campaign.customMonsters.insert(_context.campaign.customMonsters.begin() + _selectedMonsterIndex + 1, copy);
    ++_selectedMonsterIndex;
    clampSelectedMonsterActionIndex();
    _context.campaign.metadata.touch();
    saveCampaign();
}

void NPCOverviewController::clampSelectedMonsterActionIndex()
{
    if (_selectedMonsterIndex < 0 || _selectedMonsterIndex >= static_cast<int>(_context.campaign.customMonsters.size()))
    {
        _selectedMonsterActionIndex = -1;
        return;
    }

    const MonsterDefinitionModel &monster = _context.campaign.customMonsters[_selectedMonsterIndex];
    if (monster.actions.empty())
    {
        _selectedMonsterActionIndex = -1;
        return;
    }

    _selectedMonsterActionIndex = std::clamp(_selectedMonsterActionIndex, 0, static_cast<int>(monster.actions.size()) - 1);
}

int NPCOverviewController::importRulesetMonsterCatalog()
{
    if (retryPendingSave())
        return 0;

    if (!_context.campaign.ruleSet)
        return 0;

    const std::vector<MonsterDefinitionModel> catalog = _context.campaign.ruleSet->getMonsters();
    const std::vector<RuleContentAttribution> attributions = _context.campaign.ruleSet->getContentAttributions();
    const std::string attributionText = attributions.empty() ? std::string{} : attributions.front().attributionText;
    int importedCount = 0;

    for (const MonsterDefinitionModel &catalogMonster : catalog)
    {
        const EntityId sourceId = catalogMonster.metadata.id.empty() ? catalogMonster.name : catalogMonster.metadata.id;
        const auto existing = std::find_if(_context.campaign.customMonsters.begin(), _context.campaign.customMonsters.end(), [&](const MonsterDefinitionModel &monster) {
            return monster.catalogSourceId == sourceId;
        });
        if (existing != _context.campaign.customMonsters.end())
            continue;

        MonsterDefinitionModel imported = catalogMonster;
        imported.metadata = EntityMetadata{"monster"};
        imported.catalogSourceId = sourceId;
        imported.source = "Ruleset Monster Catalog";
        imported.sourceAttribution = attributionText;
        if (std::find(imported.metadata.tags.begin(), imported.metadata.tags.end(), "catalog-import") == imported.metadata.tags.end())
            imported.metadata.tags.push_back("catalog-import");
        _context.campaign.customMonsters.push_back(imported);
        ++importedCount;
    }

    if (importedCount > 0)
    {
        _selectedMonsterIndex = static_cast<int>(_context.campaign.customMonsters.size()) - 1;
        clampSelectedMonsterActionIndex();
        _context.campaign.metadata.touch();
        if (!saveCampaign())
            return 0;
    }
    return importedCount;
}

void NPCOverviewController::createVariantFromSelectedCatalogMonster()
{
    if (retryPendingSave())
        return;

    if (!_context.campaign.ruleSet)
        return;

    const std::vector<MonsterDefinitionModel> catalog = _context.campaign.ruleSet->getMonsters();
    if (catalog.empty())
        return;
    if (_selectedCatalogMonsterVariantIndex < 0 || _selectedCatalogMonsterVariantIndex >= static_cast<int>(catalog.size()))
        _selectedCatalogMonsterVariantIndex = 0;

    const MonsterDefinitionModel &base = catalog[static_cast<std::size_t>(_selectedCatalogMonsterVariantIndex)];
    const EntityId sourceId = base.metadata.id.empty() ? base.name : base.metadata.id;
    MonsterDefinitionModel variant = base;
    variant.metadata = EntityMetadata{"monster"};
    variant.catalogSourceId.clear();
    variant.baseCatalogMonsterId = sourceId;
    variant.name = std::string(gm::localization::trUi("Variant of")) + " " +
                   (base.name.empty() ? std::string(gm::localization::trUi("Monster")) : base.name);
    variant.source = "Campaign Variant";
    if (std::find(variant.metadata.tags.begin(), variant.metadata.tags.end(), "catalog-variant") == variant.metadata.tags.end())
        variant.metadata.tags.push_back("catalog-variant");

    _context.campaign.customMonsters.push_back(variant);
    _selectedMonsterIndex = static_cast<int>(_context.campaign.customMonsters.size()) - 1;
    clampSelectedMonsterActionIndex();
    _context.campaign.metadata.touch();
    saveCampaign();
}
