#include "SpellLibraryControllerSupport.hpp"

bool SpellLibraryController::saveDraft()
{
    _draftEdited = true;
    if (trimCopy(_draftSpell.name).empty() || _draftSpell.level < 0 || _draftSpell.level > 9)
    {
        _statusMessage = uiText("A name and spell level from 0 to 9 are required.", "Ein Name und ein Zaubergrad von 0 bis 9 sind erforderlich.");
        return false;
    }
    if (!_selectedSpellId.empty() && !gm::ux::resolveNavigationEntity(_currentCampaign, _selectedSpellId).available)
    {
        _statusMessage = uiText("This spell is unavailable. Your draft is retained.", "Dieser Zauber ist nicht mehr verfügbar. Dein Entwurf bleibt erhalten.");
        return false;
    }
    _draftSpell.metadata.touch();
    if (!persistCampaignEntityDraft(_currentCampaign, _draftSpell, &Campaign::customSpells,
        [](Campaign &proposed) { return ProjectSaveService::getInstance().saveCampaign(proposed, "save spell draft"); }))
    {
        _statusMessage = uiText("Not saved. Your spell draft is retained; retry Save.", "Nicht gespeichert. Dein Zauberentwurf bleibt erhalten; versuche Speichern erneut.");
        return false;
    }
    _selectedSpellId = _draftSpell.metadata.id;
    getSelectedSpell();
    _draftEdited = false;
    _retainedDrafts.erase(_draftSpell.metadata.id);
    _statusMessage = uiText("Saved: ", "Gespeichert: ") + _draftSpell.name;
    return true;
}

void SpellLibraryController::deleteSelectedSpell()
{
    if (!getSelectedSpell()) return;
    const EntityId removedSpellId = _selectedSpellId;
    Campaign proposed = _currentCampaign;
    proposed.customSpells.erase(std::remove_if(proposed.customSpells.begin(), proposed.customSpells.end(),
        [&](const SpellModel &spell) { return spell.metadata.id == removedSpellId; }), proposed.customSpells.end());
    const auto removeReference = [&](auto &owner) {
        auto &spells = owner.spellcasting.spells;
        spells.erase(std::remove_if(spells.begin(), spells.end(), [&](const CharacterSpellEntryModel &entry) {
            return entry.spellId == removedSpellId;
        }), spells.end());
    };
    for (auto &character : proposed.characters) removeReference(character);
    for (auto &npc : proposed.npcs) removeReference(npc);
    for (auto &item : proposed.customItems) if (item.linkedSpellId == removedSpellId) item.linkedSpellId.clear();
    if (!ProjectSaveService::getInstance().saveCampaign(proposed, "delete spell"))
    {
        _statusMessage = uiText("Deletion was not saved. The spell and draft are retained; retry Delete.",
            "Die Löschung wurde nicht gespeichert. Zauber und Entwurf bleiben erhalten; versuche Löschen erneut.");
        return;
    }
    _currentCampaign = std::move(proposed);
    _draftEdited = false;
    _selectedSpellId.clear();
    _selectedSpellIndex = -1;
    _retainedDrafts.erase(removedSpellId);
    _draftSpell = SpellModel{};
    _statusMessage = uiText("Spell deleted.", "Zauber gelöscht.");
}

std::vector<SpellLibraryController::CatalogEntry> SpellLibraryController::getCatalogEntries() const
{
    std::vector<CatalogEntry> entries;
    if (_currentCampaign.ruleSet)
    {
        const std::vector<SpellModel> rulesetSpells = _currentCampaign.ruleSet->getSpells();
        for (const SpellModel &spell : rulesetSpells)
            entries.push_back({spell, _currentCampaign.ruleSet->getName(), false, -1});
    }

    for (int i = 0; i < static_cast<int>(_currentCampaign.customSpells.size()); ++i)
    {
        const SpellModel &spell = _currentCampaign.customSpells[static_cast<std::size_t>(i)];
        entries.push_back({spell, uiText("Campaign", "Kampagne"), true, i});
    }

    std::sort(entries.begin(), entries.end(), [](const CatalogEntry &left, const CatalogEntry &right) {
        if (left.spell.level != right.spell.level)
            return left.spell.level < right.spell.level;
        if (left.spell.name != right.spell.name)
            return left.spell.name < right.spell.name;
        return left.source < right.source;
    });
    return entries;
}

std::vector<std::string> SpellLibraryController::getCatalogLevels(const std::vector<CatalogEntry> &entries) const
{
    std::vector<std::string> levels{uiText("All", "Alle")};
    for (const CatalogEntry &entry : entries)
    {
        const std::string label = spellLevelLabel(entry.spell.level);
        if (std::find(levels.begin(), levels.end(), label) == levels.end())
            levels.push_back(label);
    }
    return levels;
}

std::vector<SpellLibraryController::CatalogEntry> SpellLibraryController::getVisibleCatalogEntries(const std::vector<CatalogEntry> &entries, const std::vector<std::string> &levels) const
{
    std::vector<CatalogEntry> visibleEntries;
    const std::string selectedLevel =
        (_selectedCatalogLevelIndex > 0 && _selectedCatalogLevelIndex < static_cast<int>(levels.size())) ? levels[_selectedCatalogLevelIndex] : uiText("All", "Alle");

    for (const CatalogEntry &entry : entries)
    {
        if (_selectedCatalogSourceIndex == 1 && entry.campaignCustom)
            continue;
        if (_selectedCatalogSourceIndex == 2 && !entry.campaignCustom)
            continue;

        const bool levelMatches = selectedLevel == uiText("All", "Alle") || spellLevelLabel(entry.spell.level) == selectedLevel;
        const bool searchMatches = containsText(entry.spell.name, _catalogSearch) ||
                                   containsText(spellLevelLabel(entry.spell.level), _catalogSearch) ||
                                   containsText(spellSchoolLabel(entry.spell.school), _catalogSearch) ||
                                   containsText(spellResolutionLabel(entry.spell.resolution), _catalogSearch) ||
                                   containsText(entry.spell.classes, _catalogSearch) ||
                                   containsText(entry.spell.effectSummary, _catalogSearch) ||
                                   containsText(entry.spell.description, _catalogSearch) ||
                                   containsText(entry.source, _catalogSearch);
        if (levelMatches && searchMatches)
            visibleEntries.push_back(entry);
    }

    return visibleEntries;
}

void SpellLibraryController::saveCampaign(const std::string &reason)
{
    _currentCampaign.metadata.touch();
    ProjectSaveService::getInstance().saveCampaign(_currentCampaign, reason);
}

void SpellLibraryController::onShow()
{
    configure();
    _view->show();
    _view->requestDockOnNextUpdate();
}


void SpellLibraryController::retainDraft()
{
    if (!_draftSpell.metadata.id.empty()) _retainedDrafts[_draftSpell.metadata.id] = {_draftSpell, _selectedSpellId, _draftEdited};
}
bool SpellLibraryController::restoreDraft(const EntityId &id)
{
    const auto found = _retainedDrafts.find(id);
    if (found == _retainedDrafts.end()) return false;
    const auto draft = found->second;
    _draftSpell = draft.spell; _selectedSpellId = draft.sourceId; _draftEdited = draft.dirty;
    getSelectedSpell(); _statusMessage.clear(); return true;
}
void SpellLibraryController::resetDrafts()
{
    _draftCampaignId = _currentCampaign.metadata.id;
    createNewDraft(); _retainedDrafts.clear();
    _importText.clear(); _importPreviewRows.clear(); _importPreviewReady = false;
}
bool SpellLibraryController::hasUnsavedDrafts() const
{
    if (_draftEdited || !_importText.empty()) return true;
    for (const auto &entry : _retainedDrafts) if (entry.second.dirty) return true;
    return false;
}
bool SpellLibraryController::flushDrafts()
{
    if (!_importText.empty())
    {
        _view->requestImportTab();
        _statusMessage = uiText("Review or discard the pending spell import before continuing.", "Prüfe oder verwerfe den ausstehenden Zauberimport, bevor du fortfährst.");
        return false;
    }
    if (!hasUnsavedDrafts()) return true;
    const EntityId original = _draftSpell.metadata.id;
    if (_draftEdited && !saveDraft()) return false;
    retainDraft();
    std::vector<EntityId> pending;
    for (const auto &entry : _retainedDrafts) if (entry.second.dirty) pending.push_back(entry.first);
    for (const auto &id : pending) if (!restoreDraft(id) || !saveDraft()) return false;
    restoreDraft(original); _retainedDrafts.erase(original); return true;
}
