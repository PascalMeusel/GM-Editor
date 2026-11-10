#include "SpellLibraryControllerSupport.hpp"

SpellLibraryController::SpellLibraryController(Campaign &currentCampaign)
    : _currentCampaign(currentCampaign)
{
    _view = std::make_unique<SpellLibraryView>(*this);
    createNewDraft();
}

void SpellLibraryController::update(std::shared_ptr<GLFWwindow> window)
{
    (void)window;
    if (!m_isOpen)
        return;

    _view->update();
    if (!_view->isVisible())
        m_isOpen = false;
}

void SpellLibraryController::configure()
{
    if (_draftCampaignId != _currentCampaign.metadata.id) resetDrafts();
    getSelectedSpell();
}

bool SpellLibraryController::selectSpellById(const EntityId &spellId)
{
    configure();
    if (!gm::ux::resolveNavigationEntity(_currentCampaign, spellId).available) return false;
    for (int index = 0; index < static_cast<int>(_currentCampaign.customSpells.size()); ++index)
    {
        if (_currentCampaign.customSpells[static_cast<std::size_t>(index)].metadata.id != spellId)
            continue;
        selectSpell(index);
        _view->requestEditorTab();
        return true;
    }
    const auto entries = getCatalogEntries();
    for (int index = 0; index < static_cast<int>(entries.size()); ++index)
        if (entries[index].spell.metadata.id == spellId)
        {
            _catalogSearch.clear(); _selectedCatalogLevelIndex = 0; _selectedCatalogSourceIndex = 0;
            _selectedCatalogIndex = index; _view->requestCatalogTab(); return true;
        }
    return false;
}

SpellModel *SpellLibraryController::getSelectedSpell()
{
    _selectedSpellIndex = -1;
    for (int index = 0; index < static_cast<int>(_currentCampaign.customSpells.size()); ++index)
        if (_currentCampaign.customSpells[index].metadata.id == _selectedSpellId)
        { _selectedSpellIndex = index; return &_currentCampaign.customSpells[index]; }
    return nullptr;
}

void SpellLibraryController::createNewDraft()
{
    retainDraft();
    _draftSpell = SpellModel{};
    _draftSpell.name = uiText("New Spell", "Neuer Zauber");
    _selectedSpellId.clear();
    _selectedSpellIndex = -1;
    _draftEdited = false;
    _statusMessage.clear();
}

void SpellLibraryController::selectSpell(int spellIndex)
{
    if (spellIndex < 0 || spellIndex >= static_cast<int>(_currentCampaign.customSpells.size())) return;
    const EntityId id = _currentCampaign.customSpells[spellIndex].metadata.id;
    if (id == _selectedSpellId) { _selectedSpellIndex = spellIndex; return; }
    retainDraft();
    if (restoreDraft(id)) return;
    _selectedSpellIndex = spellIndex;
    _selectedSpellId = id;
    _draftSpell = _currentCampaign.customSpells[spellIndex];
    _draftEdited = false;
    _statusMessage.clear();
}

void SpellLibraryController::selectCatalogCustomSpell(const CatalogEntry &entry)
{
    if (!entry.campaignCustom)
        return;
    selectSpell(entry.customSpellIndex);
}

void SpellLibraryController::copyCatalogSpellToCampaign(const CatalogEntry &entry)
{
    if (entry.campaignCustom)
    {
        selectCatalogCustomSpell(entry);
        return;
    }

    SpellModel copy = entry.spell;
    copy.metadata = EntityMetadata{"spell"};
    copy.metadata.tags.push_back("copied-from-ruleset");
    copy.metadata.customProperties["source_spell_id"] = entry.spell.metadata.id;
    copy.metadata.customProperties["source_ruleset"] = entry.source;
    copy.metadata.touch();
    retainDraft();
    _selectedSpellId.clear();
    _selectedSpellIndex = -1;
    _draftSpell = copy;
    _draftEdited = true;
    _statusMessage = uiText("Copied ", "Kopiert: ") + copy.name +
                     uiText(" into campaign spells.", " wurde zu den Kampagnenzaubern hinzugefügt.");
    saveDraft();
}
