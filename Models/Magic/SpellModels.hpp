#pragma once

#include "Models/Core/EntityMetadata.hpp"

#include <algorithm>
#include <string>
#include <vector>

enum class SpellSchool
{
    Abjuration = 0,
    Conjuration,
    Divination,
    Enchantment,
    Evocation,
    Illusion,
    Necromancy,
    Transmutation
};

enum class SpellResolutionType
{
    Utility = 0,
    SpellAttack,
    SavingThrow,
    Healing,
    SummonOrZone
};

struct SpellModel
{
    EntityMetadata metadata{"spell"};
    std::string name = "New Spell";
    int level = 0;
    SpellSchool school = SpellSchool::Evocation;
    SpellResolutionType resolution = SpellResolutionType::Utility;
    std::string castingTime = "1 Action";
    std::string range = "Self";
    std::string duration = "Instantaneous";
    bool concentration = false;
    bool ritual = false;
    bool verbal = true;
    bool somatic = true;
    bool material = false;
    std::string materialDescription;
    std::string classes;
    std::string attackBonus;
    std::string savingThrowAbility;
    std::string damage;
    std::string healing;
    std::string area;
    std::string effectSummary;
    std::string scaling;
    std::string description;
};

struct CharacterSpellEntryModel
{
    EntityId spellId;
    bool prepared = false;
    std::string source;
    std::string notes;
};

enum class SpellValueMode { Legacy = 0, Calculated = 1, Manual = 2 };

inline bool validSpellValueMode(int value) { return value >= 0 && value <= 2; }

struct CharacterSpellcastingModel
{
    bool enabled = false;
    std::string ability = "Intelligence";
    int spellAttackBonus = 0;
    int spellSaveDc = 10;
    SpellValueMode spellAttackMode = SpellValueMode::Legacy;
    SpellValueMode spellSaveDcMode = SpellValueMode::Legacy;
    std::vector<int> slotMaximums = std::vector<int>(10, 0);
    std::vector<int> slotUsed = std::vector<int>(10, 0);
    std::vector<CharacterSpellEntryModel> spells;
};

inline const char *spellSchoolLabel(SpellSchool school)
{
    switch (school)
    {
    case SpellSchool::Abjuration:
        return "Abjuration";
    case SpellSchool::Conjuration:
        return "Conjuration";
    case SpellSchool::Divination:
        return "Divination";
    case SpellSchool::Enchantment:
        return "Enchantment";
    case SpellSchool::Evocation:
        return "Evocation";
    case SpellSchool::Illusion:
        return "Illusion";
    case SpellSchool::Necromancy:
        return "Necromancy";
    case SpellSchool::Transmutation:
        return "Transmutation";
    }
    return "Evocation";
}

inline const char *spellResolutionLabel(SpellResolutionType resolution)
{
    switch (resolution)
    {
    case SpellResolutionType::Utility:
        return "Utility";
    case SpellResolutionType::SpellAttack:
        return "Spell Attack";
    case SpellResolutionType::SavingThrow:
        return "Saving Throw";
    case SpellResolutionType::Healing:
        return "Healing";
    case SpellResolutionType::SummonOrZone:
        return "Summon/Zone";
    }
    return "Utility";
}

inline std::string spellLevelLabel(int level)
{
    if (level <= 0)
        return "Cantrip";
    return "Level " + std::to_string(std::clamp(level, 1, 9));
}

inline const SpellModel *findSpellById(const std::vector<SpellModel> &spells, const EntityId &spellId)
{
    for (const SpellModel &spell : spells)
    {
        if (spell.metadata.id == spellId)
            return &spell;
    }
    return nullptr;
}

inline void normalizeSpellcasting(CharacterSpellcastingModel &spellcasting)
{
    spellcasting.slotMaximums.resize(10, 0);
    spellcasting.slotUsed.resize(10, 0);
    for (int level = 0; level <= 9; ++level)
    {
        spellcasting.slotMaximums[level] = std::max(0, spellcasting.slotMaximums[level]);
        spellcasting.slotUsed[level] = std::clamp(spellcasting.slotUsed[level], 0, spellcasting.slotMaximums[level]);
    }
    spellcasting.spellAttackBonus = std::max(-20, spellcasting.spellAttackBonus);
    spellcasting.spellSaveDc = std::max(1, spellcasting.spellSaveDc);
}
