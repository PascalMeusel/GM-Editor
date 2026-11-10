#include "Controllers/Editor/Widgets/CharacterSheet/CharacterSheetController.hpp"
#include "Models/Character/CharacterTransfer.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Rulesets/Clockwork/ClockworkBloodline.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "workflow_test_database.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <limits>
#include <stdexcept>

struct CharacterAdvancementProbe
{
    static void begin(CharacterSheetController &controller) { controller.requestLevelUpSelectedCharacter(); }
    static void apply(CharacterSheetController &controller) { controller.applyLevelUpToSelectedCharacter(); }
    static void choices(CharacterSheetController &controller)
    {
        controller._levelUpTargetLevel = 4;
        controller._levelUpFeatures = "New technique\nNew technique";
        controller._levelUpChoices.newFeats = "Generated feat";
        controller._levelUpChoices.reviewToken = CharacterAdvancement::reviewKey(controller.advancementRequest());
    }
    static void cast(CharacterSheetController &controller, bool pact) { controller.castSpellFromSelectedCharacter(0, 1, false, pact); }
    static bool failed(const CharacterSheetController &controller) { return !controller._errorMessage.empty(); }
};
namespace
{
void require(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
Campaign read(const EntityId &id)
{
    for (auto campaign : CampaignRepository::loadAllCampaigns())
        if (campaign.metadata.id == id)
            return campaign;
    throw std::runtime_error("Campaign missing");
}
} // namespace

#include "spell_override_storage_cases.hpp"

void guidedContracts()
{
    using namespace CharacterAdvancement;
    DND5e rules;
    std::string error;
    CharacterModel c;
    c.selectedClass = "Fighter";
    c.level = 3;
    c.maxHitPoints = 28;
    c.currentHitPoints = 9;
    c.attributes = {{"Strength", 16},     {"Dexterity", 14}, {"Constitution", 14},
                    {"Intelligence", 13}, {"Wisdom", 13},    {"Charisma", 14}};
    auto malformed = c;
    malformed.classLevels = {{"Wizard", std::numeric_limits<int>::max()}, {"Paladin", std::numeric_limits<int>::max()}, {"Warlock", std::numeric_limits<int>::max()}};
    require(standardSlots(malformed)[9] == 1 && pactSlotLevel(malformed) == 5, "Large imported class levels overflowed slot helpers");
    malformed.classLevels = {{"Wizard", std::numeric_limits<int>::min()}, {"Paladin", std::numeric_limits<int>::min()}, {"Warlock", std::numeric_limits<int>::min()}};
    require(standardSlots(malformed)[1] == 0 && pactSlotLevel(malformed) == 0, "Negative imported class levels produced slots");
    Request r;
    r.guided = true;
    r.advancingClass = "Fighter";
    r.targetLevel = 4;
    r.restoreHealth = false;
    require(!preview(c, rules, r).valid, "Unresolved ASI accepted");
    r.abilityIncreases["Constitution"] = 2;
    auto p = preview(c, rules, r);
    require(p.valid && p.maximumHitPoints == 40 && p.proposed.currentHitPoints == 9,
            "Retroactive Constitution or wounded HP incorrect");
    auto changed = c;
    require(!apply(changed, rules, r, error) && changed.level == 3, "Unreviewed choices applied");
    r.reviewToken = reviewKey(r);
    require(apply(changed, rules, r, error) && changed.classLevels.at("Fighter") == 4,
            "Reviewed class history missing");
    r.newFeatures = "Additional choice";
    require(!apply(c, rules, r, error) && c.level == 3, "Changed choices retained old confirmation");
    r.newFeatures.clear();
    r.abilityIncreases.clear();
    r.newFeats = "Existing";
    c.feats = {"Existing"};
    require(!preview(c, rules, r).valid, "Duplicate feat accepted");
    r.newFeats = "Generated feat";
    require(preview(c, rules, r).valid, "Valid feat substitution rejected");
    r.abilityIncreases["Strength"] = 2;
    require(!preview(c, rules, r).valid, "Feat plus two ASI points accepted");
    r.abilityIncreases.clear();
    r.newFeats.clear();
    c.level = 5;
    r.targetLevel = 6;
    require(preview(c, rules, r).featChoices == 1, "Fighter extra milestone missing");
    c.selectedClass = "Rogue";
    c.level = 9;
    r.advancingClass = "Rogue";
    r.targetLevel = 10;
    require(preview(c, rules, r).featChoices == 1, "Rogue extra milestone missing");
    c.selectedClass = "Fighter";
    c.level = 3;
    r.advancingClass = "Wizard";
    r.targetLevel = 4;
    c.attributes["Intelligence"] = 12;
    require(!preview(c, rules, r).valid, "Multiclass prerequisite ignored");
    c.attributes["Intelligence"] = 13;
    p = preview(c, rules, r);
    require(p.valid && p.hitPointGain == 6 && p.proposed.classLevels.at("Wizard") == 1 &&
                p.proposed.spellcasting.slotMaximums[1] == 2,
            "Multiclass received first-level HP or wrong slots");
    c.classLevels = {{"Fighter", 2}};
    require(!preview(c, rules, r).valid, "Invalid class sum accepted");
    c.classLevels.clear();
    c.selectedClass = "Wizard";
    c.level = 3;
    c.classLevels = {{"Wizard", 3}};
    r.advancingClass = "Cleric";
    SpellModel spell;
    spell.name = "Generated second-level cleric spell";
    spell.level = 2;
    spell.classes = "Cleric";
    std::vector<SpellModel> catalog{spell};
    CharacterSpellEntryModel entry;
    entry.spellId = spell.metadata.id;
    r.spellsToAdd = {entry};
    require(!preview(c, rules, r, &catalog).valid, "Combined slots incorrectly grant higher class spells");
    catalog[0].level = 1;
    p = preview(c, rules, r, &catalog);
    require(p.valid && p.proposed.spellcasting.slotMaximums[2] == 3, "Combined caster slots incorrect");
    auto cast = spellcastingForEntry(p.proposed, rules, p.proposed.spellcasting.spells.back());
    require(cast.ability == "Wisdom", "Multiclass spell uses primary ability");
    catalog.clear();
    require(!preview(c, rules, r, &catalog).valid, "Deleted catalog spell accepted");
    r.spellsToAdd.clear();
    r.automaticSlots = false;
    r.slotMaximums.assign(10, 0);
    c.spellcasting.slotUsed[1] = 1;
    require(!preview(c, rules, r).valid, "Spent slot discarded");
    r.slotMaximums[1] = 2;
    require(preview(c, rules, r).valid, "Manual slot allocation rejected");
    c.spellcasting.slotUsed[1] = 0;
    c.selectedClass = "Warlock";
    c.classLevels.clear();
    c.level = 1;
    r.advancingClass = "Warlock";
    r.targetLevel = 2;
    r.automaticSlots = true;
    p = preview(c, rules, r);
    require(p.valid && p.proposed.resourceTrackers.size() == 1 && p.proposed.resourceTrackers[0].maximum == 2 &&
                p.proposed.spellcasting.slotMaximums[1] == 0,
            "Pact slots mixed with regular slots");
    c = p.proposed;
    c.resourceTrackers[0].used = 1;
    auto id = c.resourceTrackers[0].metadata.id;
    r.targetLevel = 3;
    p = preview(c, rules, r);
    require(p.valid && pactSlotLevel(p.proposed) == 2 && p.proposed.resourceTrackers[0].used == 1 &&
                p.proposed.resourceTrackers[0].metadata.id == id,
            "Pact advancement reset resources or identity");
    r.resourceChanges = {c.resourceTrackers[0]};
    r.resourceChanges[0].maximum = 0;
    require(!preview(c, rules, r).valid, "Spent resource discarded");
    r.resourceChanges.clear();
    r.overrideReason = "Generated homebrew exception";
    p = preview(c, rules, r);
    require(p.valid && p.proposed.gmNotes.find(r.overrideReason) != std::string::npos, "Override provenance lost");
    c = p.proposed;
    c.temporaryHitPoints = 4;
    CharacterModel imported;
    auto transfer = CharacterTransfer::serializeCharacterForExport(c);
    require(CharacterTransfer::deserializeCharacterForImport(transfer, imported, error) &&
                imported.classLevels == c.classLevels &&
                imported.resourceTrackers.size() == c.resourceTrackers.size() && imported.gmNotes == c.gmNotes &&
                imported.temporaryHitPoints == c.temporaryHitPoints,
            "Character transfer lost advancement or temporary-HP data");
    require(!CharacterTransfer::deserializeCharacterForImport(transfer.substr(0, transfer.size() - 4), imported, error),
            "Truncated character V3 accepted");
    std::vector<std::string> legacyFields(35);
    legacyFields[0] = "TTRPG_CHARACTER_V1";
    legacyFields[1] = "Generated legacy";
    legacyFields[2] = "1";
    legacyFields[3] = "Fighter";
    for (int index : {17, 18, 19, 22, 23, 24, 25, 27, 29, 30})
        legacyFields[index] = "0";
    legacyFields[34] = " ";
    std::string legacy;
    for (const auto &field : legacyFields)
    {
        if (!legacy.empty())
            legacy += '\x1f';
        legacy += field;
    }
    require(CharacterTransfer::deserializeCharacterForImport(legacy, imported, error) && imported.classLevels.empty(),
            "Legacy character export rejected");
    c.level = 2;
    c.classLevels = {{"Warlock", 2}};
    c.spellcasting.slotMaximums.clear();
    c.spellcasting.slotUsed.clear();
    r.guided = false;
    p = preview(c, rules, r);
    require(p.valid && p.proposed.classLevels.at("Warlock") == 3, "Legacy wrapper corrupted class history");
}

void castingCommitContracts()
{
    Campaign campaign; campaign.name = "Generated casting commit fixture"; campaign.ruleSet = std::make_shared<DND5e>();
    CharacterModel c; c.selectedClass = "Warlock"; c.level = 2; c.classLevels = {{"Warlock", 2}};
    c.maxHitPoints = 15; c.currentHitPoints = 9;
    SpellModel spell; spell.name = "Generated concentration spell"; spell.level = 1; spell.concentration = true;
    campaign.customSpells = {spell}; CharacterSpellEntryModel entry; entry.spellId = spell.metadata.id; entry.prepared = true;
    c.spellcasting.spells = {entry}; c.spellcasting.slotMaximums[1] = 2;
    ResourceTrackerModel pact; pact.definitionId = "dnd-2024.pact-slots"; pact.name = "Pact Magic"; pact.maximum = 2; pact.rechargeRule = "Short Rest";
    c.resourceTrackers = {pact}; campaign.characters = {c}; CampaignRepository::saveCampaign(campaign);
    CharacterSheetController controller(campaign); require(controller.selectCharacterById(c.metadata.id), "Cast target missing");
    SQLite::Database control(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
    control.exec("CREATE TRIGGER reject_cast BEFORE INSERT ON characters BEGIN SELECT RAISE(ABORT, 'injected casting failure'); END;");
    for (bool usePact : {false, true}) {
        CharacterAdvancementProbe::cast(controller, usePact);
        require(CharacterAdvancementProbe::failed(controller) && campaign.characters[0].spellcasting.slotUsed[1] == 0 && campaign.characters[0].resourceTrackers[0].used == 0 && campaign.characters[0].conditions.empty(), "Failed cast spent resources or started concentration");
    }
    control.exec("DROP TRIGGER reject_cast;"); CharacterAdvancementProbe::cast(controller, true);
    auto disk = read(campaign.metadata.id).characters[0];
    require(!CharacterAdvancementProbe::failed(controller) && disk.resourceTrackers[0].used == 1 && disk.spellcasting.slotUsed[1] == 0 && !disk.conditions.empty(), "Pact retry failed or spent ordinary slots");
    CharacterAdvancementProbe::cast(controller, false); disk = read(campaign.metadata.id).characters[0];
    require(disk.resourceTrackers[0].used == 1 && disk.spellcasting.slotUsed[1] == 1, "Normal cast modified Pact usage");
}

int main()
{
    try
    {
        guidedContracts();
        DND5e rules;
        for (const auto &entry : std::vector<std::pair<std::string, int>>{{"Barbarian", 7},
                                                                          {"Fighter", 6},
                                                                          {"Paladin", 6},
                                                                          {"Ranger", 6},
                                                                          {"Bard", 5},
                                                                          {"Cleric", 5},
                                                                          {"Druid", 5},
                                                                          {"Monk", 5},
                                                                          {"Rogue", 5},
                                                                          {"Warlock", 5},
                                                                          {"Sorcerer", 4},
                                                                          {"Wizard", 4}})
        {
            CharacterModel c;
            c.selectedClass = entry.first;
            c.level = 1;
            c.maxHitPoints = 12;
            c.currentHitPoints = 7;
            c.attributes["Constitution"] = 14;
            for (int level : {2, 5, 20})
            {
                CharacterAdvancement::Request request;
                request.targetLevel = level;
                request.restoreHealth = false;
                auto result = CharacterAdvancement::preview(c, rules, request);
                require(result.valid && result.hitPointGain == (entry.second + 2) * (level - 1),
                        "Class or multi-level HP calculation wrong");
                auto changed = c;
                std::string error;
                require(CharacterAdvancement::apply(changed, rules, request, error) && changed.currentHitPoints == 7,
                        "Advancement silently healed character");
            }
        }
        CharacterModel c;
        c.level = 1;
        c.maxHitPoints = 10;
        c.selectedClass = "Wizard";
        c.attributes["Constitution"] = 1;
        CharacterAdvancement::Request request;
        request.targetLevel = 4;
        require(CharacterAdvancement::preview(c, rules, request).hitPointGain == 3,
                "Minimum gain not applied per level");
        request.targetLevel = 21;
        require(!CharacterAdvancement::preview(c, rules, request).valid, "Exceeded ruleset cap");
        request.targetLevel = 1;
        require(!CharacterAdvancement::preview(c, rules, request).valid, "Repeated level accepted");
        request.targetLevel = 2;
        c.selectedClass = "Unknown";
        require(!CharacterAdvancement::preview(c, rules, request).valid, "Unknown class received invented rules");
        ClockworkBloodlineRuleset custom;
        require(!CharacterAdvancement::preview(c, custom, request).valid,
                "Custom rules received Constitution fallback");
        request.manualHitPoints = true;
        request.hitPointGain = 9;
        require(CharacterAdvancement::preview(c, custom, request).hitPointGain == 9,
                "Manual total override not respected");
        request.hitPointGain = -1;
        require(!CharacterAdvancement::preview(c, custom, request).valid, "Negative HP accepted");
        request.hitPointGain = std::numeric_limits<int>::max();
        require(!CharacterAdvancement::preview(c, custom, request).valid, "HP overflow accepted");
        isolateWorkflowDatabase();
        auto &save = ProjectSaveService::getInstance();
        save.setBackupSnapshotsEnabled(false);
        save.setRecoverySnapshotsEnabled(false);
        Campaign campaign;
        campaign.name = "Generated advancement fixture";
        campaign.ruleSet = std::make_shared<DND5e>();
        c.selectedClass = "Fighter";
        c.attributes["Constitution"] = 14;
        c.currentHitPoints = 7;
        c.traitsAndFeatures = {"Existing feature"};
        c.spellcasting.slotMaximums[1] = 2;
        c.spellcasting.slotUsed[1] = 1;
        CharacterModel other;
        campaign.characters = {c, other};
        CampaignRepository::saveCampaign(campaign);
        CharacterSheetController controller(campaign);
        require(controller.selectCharacterById(c.metadata.id), "Character unavailable");
        CharacterAdvancementProbe::begin(controller);
        CharacterAdvancementProbe::choices(controller);
        require(!controller.selectCharacterById(other.metadata.id) && !controller.flushDraft(),
                "Unconfirmed advancement silently switched or committed");
        SQLite::Database control(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
        control.exec("CREATE TRIGGER reject_advance BEFORE INSERT ON characters BEGIN SELECT RAISE(ABORT, 'injected "
                     "advancement failure'); END;");
        CharacterAdvancementProbe::apply(controller);
        require(CharacterAdvancementProbe::failed(controller) && controller.hasUnsavedDraft() &&
                    campaign.characters[0].level == 1 && read(campaign.metadata.id).characters[0].level == 1,
                "Failed advancement modified character or lost draft");
        control.exec("DROP TRIGGER reject_advance;");
        std::reverse(campaign.characters.begin(), campaign.characters.end());
        CharacterAdvancementProbe::apply(controller);
        auto disk = read(campaign.metadata.id);
        auto found = std::find_if(disk.characters.begin(), disk.characters.end(),
                                  [&](const auto &item) { return item.metadata.id == c.metadata.id; });
        require(!CharacterAdvancementProbe::failed(controller) && !controller.hasUnsavedDraft() &&
                    found != disk.characters.end() && found->level == 4 && found->maxHitPoints == 34,
                "Retry changed wrong identity or applied wrong gain");
        require(found->classLevels.at("Fighter") == 4 && found->feats.size() == 1 &&
                    found->traitsAndFeatures.size() == 2 && found->spellcasting.slotMaximums[1] == 2 &&
                    found->spellcasting.slotUsed[1] == 1,
                "Feature deduplication or preserved resources failed");
        CharacterAdvancementProbe::apply(controller);
        for (const auto &item : read(campaign.metadata.id).characters)
            if (item.metadata.id == c.metadata.id)
                require(item.level == 4 && item.maxHitPoints == 34, "Repeated apply advanced twice");
        require(controller.selectCharacterById(c.metadata.id), "Character reselection failed");
        CharacterAdvancementProbe::begin(controller);
        for (auto &item : campaign.characters)
            if (item.metadata.id == c.metadata.id)
                item.level = 5;
        CharacterAdvancementProbe::apply(controller);
        require(CharacterAdvancementProbe::failed(controller) && controller.hasUnsavedDraft(),
                "Stale advancement overwrote external change");
        controller.resetDrafts();
        require(controller.selectCharacterById(c.metadata.id), "Reselect for rule conflict failed");
        CharacterAdvancementProbe::begin(controller);
        campaign.ruleSet = std::make_shared<ClockworkBloodlineRuleset>();
        CharacterAdvancementProbe::apply(controller);
        require(CharacterAdvancementProbe::failed(controller) && controller.hasUnsavedDraft(),
                "Changed ruleset applied an old preview");
        campaign.ruleSet = std::make_shared<DND5e>();
        for (auto &item : campaign.characters)
            if (item.metadata.id == c.metadata.id)
                item.metadata.tags.push_back("archived");
        CharacterAdvancementProbe::apply(controller);
        require(CharacterAdvancementProbe::failed(controller), "Archived target advanced");
        campaign.characters.erase(std::remove_if(campaign.characters.begin(), campaign.characters.end(),
                                                 [&](const auto &item) { return item.metadata.id == c.metadata.id; }),
                                  campaign.characters.end());
        CharacterAdvancementProbe::apply(controller);
        require(CharacterAdvancementProbe::failed(controller) && controller.hasUnsavedDraft(),
                "Deleted target lost the draft");
        castingCommitContracts();
        spellOverrideStorage();
        std::cout << "Class rules, manual overrides, multi-level advancement and failure/reorder/retry passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
