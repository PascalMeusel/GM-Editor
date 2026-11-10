#include "SpellLibraryControllerSupport.hpp"

void SpellLibraryController::previewImportFromText()
{
    _statusMessage.clear();
    _importPreviewMessage.clear();
    _importReceiptMessage.clear();
    _importPreviewRows.clear();
    _importPreviewReady = false;

    const std::vector<SpellModel> importedSpells = parseImportedSpells(_importText);
    if (importedSpells.empty())
    {
        _importPreviewMessage = uiText("No spells found. Paste SRD-compatible CSV or flat JSON spell objects.",
                                       "Keine Zauber gefunden. Füge SRD-kompatible CSV-Daten oder flache JSON-Zauberobjekte ein.");
        ProjectLogger::getInstance().warning("Spell Import", _importPreviewMessage);
        return;
    }

    int added = 0;
    int updated = 0;
    int warnings = 0;
    for (SpellModel spell : importedSpells)
    {
        ImportPreviewRow row;
        row.spell = spell;
        row.updatesExisting = std::any_of(_currentCampaign.customSpells.begin(), _currentCampaign.customSpells.end(), [&](const SpellModel &candidate) {
            return toLowerCopy(candidate.name) == toLowerCopy(spell.name) && candidate.level == spell.level;
        });
        row.action = importActionForSpell(_currentCampaign, spell);
        row.warnings = validateSpell(spell);
        warnings += static_cast<int>(row.warnings.size());
        if (row.updatesExisting)
            ++updated;
        else
            ++added;
        _importPreviewRows.push_back(row);
    }

    _importPreviewReady = !_importPreviewRows.empty();
    _importPreviewMessage = uiText("Preview ready: ", "Vorschau bereit: ") + std::to_string(added) +
                            uiText(" new, ", " neu, ") + std::to_string(updated) +
                            uiText(" update(s)", " Aktualisierung(en)");
    if (warnings > 0)
        _importPreviewMessage += ", " + std::to_string(warnings) + uiText(" warning(s)", " Warnung(en)");
    _importPreviewMessage += uiText(". Review before applying.", ". Vor dem Anwenden prüfen.");
}

void SpellLibraryController::applyImportPreview()
{
    if (!_importPreviewReady || _importPreviewRows.empty())
        previewImportFromText();
    if (!_importPreviewReady || _importPreviewRows.empty())
        return;

    _statusMessage.clear();
    _importReceiptMessage.clear();
    Campaign proposed = _currentCampaign;
    int added = 0;
    int updated = 0;
    std::vector<std::string> addedNames;
    std::vector<std::string> updatedNames;
    for (const ImportPreviewRow &row : _importPreviewRows)
    {
        SpellModel spell = row.spell;
        if (trimCopy(spell.name).empty() || spell.level < 0 || spell.level > 9)
        {
            _statusMessage = uiText("Correct the required name and level fields before importing.",
                "Korrigiere die Pflichtfelder Name und Zaubergrad vor dem Importieren.");
            return;
        }
        spell.metadata = EntityMetadata{"spell"};
        spell.metadata.tags.push_back("imported");
        spell.metadata.tags.push_back("srd-compatible");
        if (!_importSource.empty())
            spell.metadata.customProperties["source"] = _importSource;
        if (!_importLicense.empty())
            spell.metadata.customProperties["license"] = _importLicense;
        spell.metadata.touch();

        const auto existing = std::find_if(proposed.customSpells.begin(), proposed.customSpells.end(), [&](const SpellModel &candidate) {
            return toLowerCopy(candidate.name) == toLowerCopy(spell.name) && candidate.level == spell.level;
        });
        if (existing != proposed.customSpells.end())
        {
            const auto retained = _retainedDrafts.find(existing->metadata.id);
            if ((_draftEdited && _selectedSpellId == existing->metadata.id) ||
                (retained != _retainedDrafts.end() && retained->second.dirty))
            {
                _statusMessage = uiText("Save the edited spell draft before importing an update to it.",
                    "Speichere den bearbeiteten Zauberentwurf, bevor du eine Aktualisierung importierst.");
                return;
            }
            const auto attribution = spell.metadata.customProperties;
            spell.metadata = existing->metadata;
            for (const auto &property : attribution) spell.metadata.customProperties[property.first] = property.second;
            spell.metadata.touch();
            *existing = spell;
            ++updated;
            updatedNames.push_back(spell.name.empty() ? uiText("<Unnamed Spell>", "<Unbenannter Zauber>") : spell.name);
        }
        else
        {
            proposed.customSpells.push_back(spell);
            ++added;
            addedNames.push_back(spell.name.empty() ? uiText("<Unnamed Spell>", "<Unbenannter Zauber>") : spell.name);
        }
    }

    if (!ProjectSaveService::getInstance().saveCampaign(proposed, "spell import"))
    {
        _statusMessage = uiText("Import was not saved. The preview and source text are retained for retry.",
            "Der Import wurde nicht gespeichert. Vorschau und Quelltext bleiben für einen erneuten Versuch erhalten.");
        return;
    }
    _currentCampaign = std::move(proposed);
    getSelectedSpell();
    if (!_draftEdited)
        if (const auto *selected = getSelectedSpell()) _draftSpell = *selected;
    for (auto it = _retainedDrafts.begin(); it != _retainedDrafts.end();)
        if (!it->second.dirty) it = _retainedDrafts.erase(it); else ++it;
    _statusMessage = uiText("Imported ", "Importiert: ") + std::to_string(added) +
                     uiText(" spell(s), updated ", " Zauber; aktualisiert: ") + std::to_string(updated) + ".";
    _importText.clear();
    _importPreviewRows.clear();
    _importPreviewReady = false;
    _importPreviewMessage.clear();

    const ProjectSaveState &saveState = ProjectSaveService::getInstance().state();
    std::ostringstream receipt;
    receipt << uiText("Import receipt: ", "Importbeleg: ") << added << uiText(" added, ", " hinzugefügt, ") << updated
            << uiText(" updated.", " aktualisiert.");
    receipt << uiText(" Source: ", " Quelle: ") << (_importSource.empty() ? uiText("Unspecified", "Nicht angegeben") : _importSource) << ".";
    receipt << uiText(" License: ", " Lizenz: ") << (_importLicense.empty() ? uiText("Unspecified", "Nicht angegeben") : _importLicense) << ".";
    if (!addedNames.empty())
        receipt << uiText(" Added: ", " Hinzugefügt: ") << joinLimitedNames(addedNames) << ".";
    if (!updatedNames.empty())
        receipt << uiText(" Updated: ", " Aktualisiert: ") << joinLimitedNames(updatedNames) << ".";
    if (!saveState.lastBackupPath.empty())
        receipt << uiText(" Rollback backup: ", " Rücksetz-Sicherung: ") << saveState.lastBackupPath << ".";
    if (!saveState.lastRecoverySnapshotPath.empty())
        receipt << uiText(" Recovery snapshot: ", " Wiederherstellungspunkt: ") << saveState.lastRecoverySnapshotPath << ".";
    if (!saveState.lastBackupError.empty())
        receipt << uiText(" Backup warning: ", " Sicherungswarnung: ") << saveState.lastBackupError << ".";
    if (!saveState.lastRecoveryError.empty())
        receipt << uiText(" Recovery warning: ", " Wiederherstellungswarnung: ") << saveState.lastRecoveryError << ".";
    _importReceiptMessage = receipt.str();

    ProjectEventBus::getInstance().publish(ProjectEvent{ProjectEventType::EntityCreated,
                                                        _currentCampaign.metadata.id,
                                                        {},
                                                        "spell",
                                                        _statusMessage});
}

std::string SpellLibraryController::importRollbackNotice() const
{
    const ProjectSaveConfig &config = ProjectSaveService::getInstance().config();
    std::string notice = uiText("Apply Import saves through the project save service.",
                                "Beim Anwenden wird der Import über den Projektspeicherdienst gesichert.");
    if (config.backupSnapshotsEnabled)
        notice += uiText(" A pre-save backup is written before imported spells are committed.",
                         " Vor dem Übernehmen der importierten Zauber wird eine Sicherung angelegt.");
    else
        notice += uiText(" Pre-save backups are disabled.", " Sicherungen vor dem Speichern sind deaktiviert.");
    if (config.recoverySnapshotsEnabled)
        notice += uiText(" A recovery snapshot is written after the save.",
                         " Nach dem Speichern wird ein Wiederherstellungspunkt angelegt.");
    else
        notice += uiText(" Recovery snapshots are disabled.", " Wiederherstellungspunkte sind deaktiviert.");
    notice += uiText(" Use the receipt paths after applying if you need to roll back the import.",
                     " Nutze bei Bedarf die Pfade im Importbeleg, um den Import zurückzusetzen.");
    return notice;
}
