#pragma once



#include "Models/Rulesets/DND/DND5e.hpp"



inline const char *DND5e::getName() const
{
        return "D&D assistance (mixed SRD 5.1 / 5.2.1)";
    }

inline const char *DND5e::getId() const
{
        return "dnd-2024";
    }

inline const char *DND5e::getVersionId() const
{
        return "dnd-2024-5.5e";
    }

inline const std::vector<std::string> DND5e::getLegacyNames() const
{
        return {"Dungeons & Dragons 2024 (5.5e)", "Dungeons & Dragons 5th Edition", "dnd5e", "dnd2024"};
    }

inline const bool DND5e::levelsExist() const
{ return true; }

inline const std::vector<std::string> DND5e::getAttributes() const
{
        return {"Strength", "Dexterity", "Constitution", "Intelligence", "Wisdom", "Charisma"};
    }

inline const std::vector<std::string> DND5e::getClasses() const
{
        return {"Barbarian", "Bard", "Cleric", "Druid", "Fighter", "Monk", "Paladin", "Ranger", "Rogue", "Sorcerer", "Warlock", "Wizard"};
    }

inline RuleAdvancementProfile DND5e::advancementProfile(const std::string &className) const
{
        // SRD 5.2.1 p.23, Fixed Hit Points by Class (existing SRD attribution).
        const auto name = lowerCopy(className);
        int fixed = 0;
        if (name == "barbarian") fixed = 7;
        else if (name == "fighter" || name == "paladin" || name == "ranger") fixed = 6;
        else if (name == "bard" || name == "cleric" || name == "druid" || name == "monk" || name == "rogue" || name == "warlock") fixed = 5;
        else if (name == "sorcerer" || name == "wizard") fixed = 4;
        // SRD 5.2.1 p.23 lists the class hit dice: first level uses the die
        // maximum, while later levels use the fixed HP progression above.
        int starting = 0;
        if (name == "barbarian") starting = 12;
        else if (name == "fighter" || name == "paladin" || name == "ranger") starting = 10;
        else if (name == "bard" || name == "cleric" || name == "druid" || name == "monk" || name == "rogue" || name == "warlock") starting = 8;
        else if (name == "sorcerer" || name == "wizard") starting = 6;
        return {fixed, starting, "Constitution", 20};
    }

inline const std::vector<std::string> DND5e::getRaces() const
{
        // The bundled default uses the nine SRD 5.2.1 species. Existing campaign
        // strings and user-authored species remain readable and editable.
        return {"Dragonborn", "Dwarf", "Elf", "Gnome", "Goliath", "Halfling", "Human", "Orc", "Tiefling"};
    }

inline std::vector<RuleContentAttribution> DND5e::getContentAttributions() const
{
        return {
            {"dnd-srd-5.2.1-rules-engine",
             "ruleset",
             "System Reference Document (SRD)",
             "5.2.1",
             "https://www.dndbeyond.com/srd",
             "Creative Commons Attribution 4.0 International",
             "https://creativecommons.org/licenses/by/4.0/legalcode",
             "This work includes material from the System Reference Document 5.2.1 (\"SRD 5.2.1\") by Wizards of the Coast LLC, available at https://www.dndbeyond.com/srd. The SRD 5.2.1 is licensed under the Creative Commons Attribution 4.0 International License, available at https://creativecommons.org/licenses/by/4.0/legalcode.",
             "Selected rules adapted into calculations, data and abbreviated interface text; incomplete implementation.",
             true},
            {"dnd-srd-5.1-legacy-catalog",
             "ruleset",
             "System Reference Document (SRD)",
             "5.1",
             "https://www.dndbeyond.com/srd",
             "Creative Commons Attribution 4.0 International",
             "https://creativecommons.org/licenses/by/4.0/legalcode",
             "This work includes material taken from the System Reference Document 5.1 (\"SRD 5.1\") by Wizards of the Coast LLC and available at https://dnd.wizards.com/resources/systems-reference-document. The SRD 5.1 is licensed under the Creative Commons Attribution 4.0 International License available at https://creativecommons.org/licenses/by/4.0/legalcode.",
             "Selected equipment, monster statistics and legacy rules adapted into data structures and abbreviated summaries; not a complete edition implementation.",
             true}};
    }

inline RuleContentGateResult DND5e::validateContentAttribution(const RuleContentAttribution &attribution, bool publicBundle) const
{
        RuleContentGateResult result = IRuleSet::validateContentAttribution(attribution, publicBundle);
        const std::string source = lowerCopy(attribution.sourceName);
        const std::string license = lowerCopy(attribution.licenseName);
        const std::string licenseUrl = lowerCopy(attribution.licenseUrl);
        const std::string attributionText = lowerCopy(attribution.attributionText);

        const bool looksLikeSrd = source.find("systems reference document") != std::string::npos || source.find("srd") != std::string::npos;
        const bool looksCreativeCommons = license.find("creative commons") != std::string::npos || license.find("cc-by") != std::string::npos || license.find("cc by") != std::string::npos;
        const bool hasLicenseUrl = licenseUrl.find("creativecommons.org/licenses/by/4.0") != std::string::npos;
        const bool hasAttributionText = attributionText.find("systems reference document") != std::string::npos || attributionText.find("srd") != std::string::npos;
        const bool allowedSrd = looksLikeSrd && looksCreativeCommons && hasLicenseUrl && !attribution.sourceVersion.empty() && hasAttributionText;

        const std::vector<std::string> disallowedOfficialSources{
            "player's handbook",
            "players handbook",
            "monster manual",
            "dungeon master's guide",
            "dungeon masters guide",
            "unearthed arcana",
            "forgotten realms",
            "eberron",
            "dragonlance",
            "spelljammer",
            "planescape",
            "ravenloft",
            "strixhaven",
            "xanathar",
            "tasha",
            "mordenkainen",
            "volo",
            "fizban"};

        bool blockedOfficialSource = false;
        for (const std::string &blocked : disallowedOfficialSources)
        {
            if (source.find(blocked) != std::string::npos)
            {
                blockedOfficialSource = true;
                break;
            }
        }

        const bool officialDndSource = source.find("d&d") != std::string::npos || source.find("dungeons & dragons") != std::string::npos || source.find("wizards of the coast") != std::string::npos;
        if (publicBundle && (blockedOfficialSource || officialDndSource) && !allowedSrd)
            result.errors.push_back("Public D&D bundles may include SRD/CC content only; official non-SRD source labels are blocked.");
        if (publicBundle && source.find("srd") != std::string::npos && !allowedSrd)
            result.errors.push_back("SRD-labeled content requires source version, CC-BY-4.0 license URL, and attribution text.");
        if (!publicBundle && !allowedSrd)
            result.warnings.push_back("Private table content can be tracked locally, but it should not be redistributed as bundled D&D content.");

        result.allowed = result.errors.empty();
        if (result.allowed && allowedSrd)
            result.warnings.push_back("Content attribution passed the SRD/CC-BY public bundle gate.");
        return result;
    }

inline const std::vector<RuleSkillDefinition> DND5e::getSkills() const
{
        return {
            {"acrobatics", "Acrobatics", "Dexterity"},
            {"animal-handling", "Animal Handling", "Wisdom"},
            {"arcana", "Arcana", "Intelligence"},
            {"athletics", "Athletics", "Strength"},
            {"deception", "Deception", "Charisma"},
            {"history", "History", "Intelligence"},
            {"insight", "Insight", "Wisdom"},
            {"intimidation", "Intimidation", "Charisma"},
            {"investigation", "Investigation", "Intelligence"},
            {"medicine", "Medicine", "Wisdom"},
            {"nature", "Nature", "Intelligence"},
            {"perception", "Perception", "Wisdom"},
            {"performance", "Performance", "Charisma"},
            {"persuasion", "Persuasion", "Charisma"},
            {"religion", "Religion", "Intelligence"},
            {"sleight-of-hand", "Sleight of Hand", "Dexterity"},
            {"stealth", "Stealth", "Dexterity"},
            {"survival", "Survival", "Wisdom"}};
    }

inline const std::vector<RuleDifficultyClass> DND5e::getCommonDifficultyClasses() const
{
        return {{"Very Easy", 5}, {"Easy", 10}, {"Medium", 15}, {"Hard", 20}, {"Very Hard", 25}, {"Nearly Impossible", 30}};
    }

inline const std::vector<RuleCurrencyDefinition> DND5e::getCurrencies() const
{
        return {
            {"copper", "Copper", "cp", 1},
            {"silver", "Silver", "sp", 10},
            {"gold", "Gold", "gp", 100},
            {"platinum", "Platinum", "pp", 1000}};
    }

inline RuleCarryingCapacitySummary DND5e::evaluateCarryingCapacity(const std::map<std::string, int> &attributes, const InventoryModel &inventory, const std::vector<ItemModel> &items) const
{
        RuleCarryingCapacitySummary summary = IRuleSet::evaluateCarryingCapacity(attributes, inventory, items);
        if (summary.capacityKnown && summary.overCapacity)
            summary.warnings.push_back("D&D baseline carrying capacity is Strength x 15 lb before table-specific variant encumbrance.");
        return summary;
    }

inline const std::vector<RuleEquipmentSlotDefinition> DND5e::getEquipmentSlots() const
{
        return {
            {"body", "Body", 1},
            {"main-hand", "Main Hand", 1},
            {"off-hand", "Off Hand", 1},
            {"two-handed", "Two-Handed", 1},
            {"ranged", "Ranged", 1},
            {"head", "Head", 1},
            {"cloak", "Cloak", 1},
            {"hands", "Hands", 1},
            {"feet", "Feet", 1},
            {"ring", "Ring", 2},
            {"pack", "Pack", 99},
            {"pouch", "Pouch", 99}};
    }

inline RuleEquipmentSlotValidation DND5e::validateEquipmentSlots(const InventoryModel &inventory, const std::vector<ItemModel> &items) const
{
        RuleEquipmentSlotValidation validation = IRuleSet::validateEquipmentSlots(inventory, items);
        bool hasTwoHanded = false;
        bool hasMainHand = false;
        bool hasOffHand = false;
        for (const InventoryEntryModel &entry : inventory.entries)
        {
            if (!entry.equipped)
                continue;
            const ItemModel *item = findItemById(items, entry.itemId);
            if (!item)
                continue;
            if (isArmorItem(*item) && item->name != "Shield" && entry.slot != "Body")
                validation.warnings.push_back(item->name + " should usually use the Body slot.");
            if (item->name == "Shield" && entry.slot != "Off Hand")
                validation.warnings.push_back("Shield should usually use the Off Hand slot.");
            if (entry.slot == "Two-Handed")
                hasTwoHanded = true;
            if (entry.slot == "Main Hand")
                hasMainHand = true;
            if (entry.slot == "Off Hand")
                hasOffHand = true;
        }
        if (hasTwoHanded && (hasMainHand || hasOffHand))
            validation.warnings.push_back("Two-Handed equipment conflicts with Main Hand or Off Hand equipment.");
        validation.valid = validation.warnings.empty();
        return validation;
    }

inline const std::vector<RuleConditionDefinition> DND5e::getConditionDefinitions() const
{
        return {
            {"blinded", "Blinded", "Cannot see; attacks against have advantage.", {"Fails checks requiring sight.", "Attacks have disadvantage.", "Attacks against the creature have advantage."}, "Varies by effect", "Effect ends or sight is restored."},
            {"charmed", "Charmed", "Cannot attack charmer; charmer has social advantage.", {"Cannot attack or target the charmer with harmful abilities.", "Charmer has advantage on social checks."}, "Varies by effect", "Effect ends or charm is broken."},
            {"frightened", "Frightened", "Disadvantage while source is visible; cannot move closer.", {"Disadvantage on ability checks and attacks while source is visible.", "Cannot willingly move closer to source."}, "Varies by effect", "Effect ends, source is gone, or save succeeds."},
            {"grappled", "Grappled", "Speed is 0 until escape.", {"Speed becomes 0.", "Ends if grappler is incapacitated or target is moved away."}, "Until escape", "Escape, forced movement, or grappler incapacitated."},
            {"incapacitated", "Incapacitated", "Cannot take actions or reactions.", {"Cannot take actions.", "Cannot take bonus actions.", "Cannot take reactions."}, "Varies by effect", "Effect ends."},
            {"invisible", "Invisible", "Unseen without special senses; attacks have advantage.", {"Attacks have advantage.", "Attacks against the creature have disadvantage unless located."}, "Varies by effect", "Effect ends or creature is revealed."},
            {"paralyzed", "Paralyzed", "Incapacitated; attacks from nearby can critically hit.", {"Incapacitated.", "Automatically fails Strength and Dexterity saves.", "Nearby hits can critically hit."}, "Varies by effect", "Effect ends or save succeeds."},
            {"poisoned", "Poisoned", "Disadvantage on attacks and ability checks.", {"Disadvantage on attack rolls.", "Disadvantage on ability checks."}, "Varies by effect", "Effect ends, antidote, or save succeeds."},
            {"prone", "Prone", "Crawl or stand; melee attacks nearby have advantage.", {"Disadvantage on attack rolls.", "Nearby attacks have advantage.", "Distant attacks have disadvantage."}, "Until standing", "Spend movement to stand."},
            {"restrained", "Restrained", "Speed 0; attacks against have advantage.", {"Speed becomes 0.", "Attacks have disadvantage.", "Dexterity saves have disadvantage.", "Attacks against have advantage."}, "Varies by effect", "Escape or effect ends."},
            {"stunned", "Stunned", "Incapacitated; fails Str/Dex saves.", {"Incapacitated.", "Automatically fails Strength and Dexterity saves.", "Attacks against have advantage."}, "Varies by effect", "Effect ends or save succeeds."},
            {"unconscious", "Unconscious", "Incapacitated, prone, unaware.", {"Incapacitated.", "Drops held items and falls prone.", "Automatically fails Strength and Dexterity saves.", "Nearby hits can critically hit."}, "Until awakened", "Healing, stabilization, or effect ends."}};
    }

inline int DND5e::getProficiencyBonus(int level) const
{
        return 2 + (std::max(1, level) - 1) / 4;
    }

inline int DND5e::getWeaponAbilityModifier(const std::map<std::string, int> &attributes, const ItemModel &weapon) const
{
        const RuleWeaponProfile profile = getWeaponProfile(weapon);
        if (profile.primaryAbility == "Dexterity")
            return getAbilityModifier(attributes, "Dexterity");
        if (profile.finesse || profile.primaryAbility == "Finesse")
            return std::max(getAbilityModifier(attributes, "Strength"), getAbilityModifier(attributes, "Dexterity"));
        return getAbilityModifier(attributes, "Strength");
    }
