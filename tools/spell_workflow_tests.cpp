#include "workflow_test_database.hpp"
#include "Controllers/Editor/Widgets/SpellLibrary/SpellLibraryController.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "Models/Rulesets/SpellcastingValues.hpp"
#include "Models/Rulesets/RulesetResolution.hpp"
#include "Models/Rulesets/RulesetVersioning.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <algorithm>
#include <iostream>
#include <map>
#include <stdexcept>

struct SpellWorkflowProbe
{
    static void edit(SpellLibraryController &editor, const std::string &name)
    { editor._draftSpell.name = name; editor.markDraftEdited(); }
    static std::string name(const SpellLibraryController &editor) { return editor._draftSpell.name; }
    static bool save(SpellLibraryController &editor) { return editor.saveDraft(); }
    static void remove(SpellLibraryController &editor) { editor.deleteSelectedSpell(); }
    static void preview(SpellLibraryController &editor, const std::string &text)
    { editor._importText = text; editor.previewImportFromText(); }
    static void apply(SpellLibraryController &editor) { editor.applyImportPreview(); }
    static bool pendingImport(const SpellLibraryController &editor)
    { return !editor._importText.empty() && editor._importPreviewReady && editor._importReceiptMessage.empty(); }
    static void copyCatalog(SpellLibraryController &editor, const SpellLibraryController::CatalogEntry &entry)
    { editor.copyCatalogSpellToCampaign(entry); }
    static SpellModel draft(const SpellLibraryController &editor) { return editor._draftSpell; }
    static std::vector<SpellLibraryController::CatalogEntry> catalogEntries(const SpellLibraryController &editor)
    { return editor.getCatalogEntries(); }
};
namespace
{
void require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
std::string spellName(const Campaign &campaign, const EntityId &id)
{
    for (const auto &spell : campaign.customSpells) if (spell.metadata.id == id) return spell.name;
    throw std::runtime_error("Missing spell identity");
}
Campaign read(const EntityId &id)
{
    for (auto campaign : CampaignRepository::loadAllCampaigns()) if (campaign.metadata.id == id) return campaign;
    throw std::runtime_error("Missing persisted fixture");
}
#include "spell_override_cases.hpp"
}
int main()
{
    try
    {
        isolateWorkflowDatabase();
        spellOverrideValues(); spellPinnedResolution();
        auto &save = ProjectSaveService::getInstance();
        save.setBackupSnapshotsEnabled(false); save.setRecoverySnapshotsEnabled(false);
        Campaign campaign; campaign.name = "Generated spell workflow"; campaign.ruleSet = std::make_shared<DND5e>();
        const DND5e dnd;
        const std::vector<SpellModel> dndSpells = dnd.getSpells();
        const auto spellById = [&](const char *id) {
            const auto found = std::find_if(dndSpells.begin(), dndSpells.end(), [&](const SpellModel &spell) {
                return spell.metadata.id == id;
            });
            require(found != dndSpells.end(), "Built-in healing spell missing");
            return *found;
        };
        CharacterSpellcastingModel wisdomCasting;
        wisdomCasting.enabled = true;
        wisdomCasting.ability = "Wisdom";
        const std::map<std::string, int> wisdom14{{"Wisdom", 14}};
        const SpellModel cureWounds = spellById("dnd.srd.spell.cure-wounds");
        const SpellModel healingWord = spellById("dnd.srd.spell.healing-word");
        const SpellModel prayerOfHealing = spellById("dnd.srd.spell.prayer-of-healing");
        require(dnd.summarizeSpellResolution(cureWounds, wisdomCasting, wisdom14, 3, 1).healing == "2d8+2" &&
                    dnd.summarizeSpellResolution(healingWord, wisdomCasting, wisdom14, 3, 1).healing == "2d4+2",
                "DND 2024 Cure Wounds and Healing Word must include the caster spellcasting modifier");
        require(dnd.summarizeSpellResolution(prayerOfHealing, wisdomCasting, wisdom14, 3, 2).healing == "2d8",
                "Prayer of Healing must retain its authored formula");
        const std::map<std::string, int> wisdom8{{"Wisdom", 8}};
        const std::map<std::string, int> wisdom10{{"Wisdom", 10}};
        require(dnd.summarizeSpellResolution(cureWounds, wisdomCasting, wisdom8, 3, 1).healing == "2d8-1" &&
                    dnd.summarizeSpellResolution(cureWounds, wisdomCasting, wisdom10, 3, 1).healing == "2d8",
                "Explicit healing formulas must resolve negative and zero spellcasting modifiers exactly once");
        require(cureWounds.healing == "2d8 + spellcasting modifier" &&
                    healingWord.healing == "2d4 + spellcasting modifier",
                "Catalog healing formulas must retain their explicit spellcasting-modifier rule when copied");
        SpellModel copiedCure = cureWounds;
        copiedCure.metadata.id = "campaign.spell.copied_cure_wounds";
        require(dnd.summarizeSpellResolution(copiedCure, wisdomCasting, wisdom14, 3, 1).healing == "2d8+2",
                "A catalog copy must retain Cure Wounds' explicit healing formula");
        {
            SpellLibraryController catalogEditor(campaign);
            const auto entries = SpellWorkflowProbe::catalogEntries(catalogEditor);
            const auto catalogCure = std::find_if(entries.begin(), entries.end(), [](const auto &entry) {
                return !entry.campaignCustom && entry.spell.metadata.id == "dnd.srd.spell.cure-wounds";
            });
            require(catalogCure != entries.end(), "Cure Wounds must be available to copy from the catalog");
            SpellWorkflowProbe::copyCatalog(catalogEditor, *catalogCure);
            copiedCure = SpellWorkflowProbe::draft(catalogEditor);
            require(copiedCure.metadata.id != cureWounds.metadata.id &&
                        copiedCure.healing == "2d8 + spellcasting modifier" &&
                        copiedCure.metadata.customProperties.at("source_spell_id") == cureWounds.metadata.id &&
                        dnd.summarizeSpellResolution(copiedCure, wisdomCasting, wisdom14, 3, 1).healing == "2d8+2",
                    "Copy To Campaign must retain Cure Wounds' explicit healing formula and attribution");
        }
        SpellModel arbitraryCustom = cureWounds;
        arbitraryCustom.metadata.id = "campaign.spell.custom_healing";
        arbitraryCustom.healing = "2d8";
        require(dnd.summarizeSpellResolution(arbitraryCustom, wisdomCasting, wisdom14, 3, 1).healing == "2d8",
                "A custom raw healing formula must not receive a name-based built-in bonus");
        arbitraryCustom.healing = "2d8 + spellcasting modifier + 1d4";
        require(dnd.summarizeSpellResolution(arbitraryCustom, wisdomCasting, wisdom14, 3, 1).healing == arbitraryCustom.healing,
                "Unsupported marker placement must remain authored instead of silently dropping formula content");
        SpellModel legacyCatalogCure = cureWounds;
        legacyCatalogCure.healing = "2d8";
        require(dnd.summarizeSpellResolution(legacyCatalogCure, wisdomCasting, wisdom14, 3, 1).healing == "2d8+2",
                "Older direct Cure Wounds catalog entries must retain their built-in compatibility bonus");
        wisdomCasting.spells.push_back({cureWounds.metadata.id, true, "", ""});
        const std::vector<MonsterActionModel> preparedHealing =
            dnd.getPreparedSpellActions(wisdomCasting, dndSpells, wisdom14, 3);
        require(preparedHealing.size() == 1 && preparedHealing.front().damage == "2d8+2",
                "Prepared Cure Wounds action must carry the resolved healing formula to combat");
        SpellModel first; first.name = "First spell"; first.level = 1;
        first.metadata.customProperties["existing"] = "preserve attribution";
        SpellModel second; second.name = "Second spell";
        campaign.customSpells = {first, second};
        CampaignRepository::saveCampaign(campaign);
        SpellLibraryController editor(campaign); editor.configure();
        require(editor.selectSpellById(first.metadata.id), "Select failed");
        SpellWorkflowProbe::edit(editor, "Retained spell");
        require(editor.selectSpellById(second.metadata.id) && editor.selectSpellById(first.metadata.id) &&
            SpellWorkflowProbe::name(editor) == "Retained spell" && editor.hasUnsavedDrafts(), "Library visit lost dirty draft");
        SQLite::Database control(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
        control.exec("CREATE TRIGGER reject_spell BEFORE INSERT ON custom_spells BEGIN SELECT RAISE(ABORT, 'injected failure'); END;");
        require(!SpellWorkflowProbe::save(editor) && editor.hasUnsavedDrafts() && campaign.customSpells[0].name == "First spell" &&
            spellName(read(campaign.metadata.id), first.metadata.id) == "First spell", "Failed commit changed memory/disk or dropped draft");
        control.exec("DROP TRIGGER reject_spell;");
        require(editor.flushDrafts() && !editor.hasUnsavedDrafts() && spellName(read(campaign.metadata.id), first.metadata.id) == "Retained spell",
            "Retry did not save retained draft");
        SpellWorkflowProbe::preview(editor, "name,level,description\nRetained spell,1,Imported notes\nThird spell,2,New content\n");
        require(SpellWorkflowProbe::pendingImport(editor), "No import preview");
        control.exec("CREATE TRIGGER reject_spell BEFORE INSERT ON custom_spells BEGIN SELECT RAISE(ABORT, 'injected import failure'); END;");
        SpellWorkflowProbe::apply(editor);
        require(SpellWorkflowProbe::pendingImport(editor) && campaign.customSpells.size() == 2 &&
            read(campaign.metadata.id).customSpells.size() == 2, "Failed import changed content or emitted success receipt");
        control.exec("DROP TRIGGER reject_spell;");
        SpellWorkflowProbe::apply(editor);
        require(!SpellWorkflowProbe::pendingImport(editor) && campaign.customSpells.size() == 3 &&
            campaign.customSpells[0].metadata.id == first.metadata.id &&
            campaign.customSpells[0].metadata.customProperties.at("existing") == "preserve attribution", "Import lost identity/metadata");
        const auto imported = read(campaign.metadata.id);
        const auto importedFirst = std::find_if(imported.customSpells.begin(), imported.customSpells.end(),
            [&](const SpellModel &spell) { return spell.metadata.id == first.metadata.id; });
        require(importedFirst != imported.customSpells.end() &&
            importedFirst->metadata.customProperties.at("existing") == "preserve attribution" &&
            importedFirst->metadata.customProperties.at("source") == "SRD-compatible import" &&
            importedFirst->metadata.createdAtUtc == first.metadata.createdAtUtc,
            "SQLite restart lost import attribution or original creation identity");
        require(editor.selectSpellById(first.metadata.id), "Reselect failed");
        control.exec("CREATE TRIGGER reject_spell_delete BEFORE DELETE ON custom_spells BEGIN SELECT RAISE(ABORT, 'injected deletion failure'); END;");
        SpellWorkflowProbe::remove(editor);
        require(campaign.customSpells.size() == 3 && read(campaign.metadata.id).customSpells.size() == 3, "Failed deletion changed content");
        control.exec("DROP TRIGGER reject_spell_delete;");
        SpellWorkflowProbe::remove(editor);
        require(campaign.customSpells.size() == 2 && read(campaign.metadata.id).customSpells.size() == 2 &&
            !editor.selectSpellById(first.metadata.id), "Delete retry did not remove selected identity");
        std::cout << "Spell drafts, commit retry, import retry/identity and deletion retry passed through production SQLite.\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
