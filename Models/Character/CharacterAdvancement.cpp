#include "CharacterAdvancementSupport.hpp"

namespace CharacterAdvancement
{
std::map<std::string, int> classLevels(const CharacterModel &c)
{
    if (c.classLevels.empty())
        return c.selectedClass.empty() ? std::map<std::string, int>{}
                                       : std::map<std::string, int>{{canonical(c.selectedClass), c.level}};
    return c.classLevels;
}
std::vector<int> standardSlots(const CharacterModel &c)
{
    // SRD 5.2.1 pp.25-26: full casters plus each half caster rounded up.
    static constexpr int slots[21][9] = {{0},
                                         {2},
                                         {3},
                                         {4, 2},
                                         {4, 3},
                                         {4, 3, 2},
                                         {4, 3, 3},
                                         {4, 3, 3, 1},
                                         {4, 3, 3, 2},
                                         {4, 3, 3, 3, 1},
                                         {4, 3, 3, 3, 2},
                                         {4, 3, 3, 3, 2, 1},
                                         {4, 3, 3, 3, 2, 1},
                                         {4, 3, 3, 3, 2, 1, 1},
                                         {4, 3, 3, 3, 2, 1, 1},
                                         {4, 3, 3, 3, 2, 1, 1, 1},
                                         {4, 3, 3, 3, 2, 1, 1, 1},
                                         {4, 3, 3, 3, 2, 1, 1, 1, 1},
                                         {4, 3, 3, 3, 3, 1, 1, 1, 1},
                                         {4, 3, 3, 3, 3, 2, 1, 1, 1},
                                         {4, 3, 3, 3, 3, 2, 2, 1, 1}};
    int level = 0;
    for (const auto &[name, recordedLevel] : classLevels(c))
    {
        // UI helpers can run before preview rejects a malformed imported history.
        const int n = std::clamp(recordedLevel, 0, 20);
        if (name == "Bard" || name == "Cleric" || name == "Druid" || name == "Sorcerer" || name == "Wizard")
            level += n;
        if (name == "Paladin" || name == "Ranger")
            level += (n + 1) / 2;
    }
    std::vector<int> result(10, 0);
    for (int i = 1; i <= 9; ++i)
        result[i] = slots[std::clamp(level, 0, 20)][i - 1];
    return result;
}
int pactSlotLevel(const CharacterModel &c)
{
    int n = classLevel(c, "Warlock");
    return n > 0 ? std::min(5, (n + 1) / 2) : 0;
}
CharacterSpellcastingModel spellcastingForEntry(const CharacterModel &c, const IRuleSet &rules,
                                                const CharacterSpellEntryModel &entry)
{
    auto result = c.spellcasting;
    if (dynamic_cast<const DND5e *>(&rules) && classLevels(c).count(canonical(entry.source)))
    {
        auto ability = castingAbility(entry.source);
        if (!ability.empty())
            result.ability = ability;
    }
    return result;
}
std::string reviewKey(const Request &r)
{
    using namespace PersistenceSerialization;
    std::ostringstream s;
    s << r.targetLevel << ' ' << r.manualHitPoints << ' ' << r.hitPointGain << ' ' << r.restoreHealth << ' ' << r.guided
      << ' ' << r.automaticSlots << ' ' << r.automaticResources << ' ' << std::quoted(r.newFeatures)
      << std::quoted(r.advancingClass) << std::quoted(r.newFeats) << std::quoted(r.overrideReason)
      << std::quoted(serializeAttributes(r.abilityIncreases)) << std::quoted(serializeSpellSlots(r.slotMaximums))
      << std::quoted(serializeSpellbookEntries(r.spellsToAdd)) << std::quoted(serializeStringList(r.spellsToRemove))
      << std::quoted(serializeResourceTrackers(r.resourceChanges));
    return s.str();
}
bool apply(CharacterModel &c, const IRuleSet &rules, const Request &r, std::string &error,
           const std::vector<SpellModel> *catalog)
{
    auto result = preview(c, rules, r, catalog);
    if (!result.valid)
    {
        error = result.error;
        return false;
    }
    if (r.guided && r.reviewToken != reviewKey(r))
    {
        error = "Review the current choices before applying advancement.";
        return false;
    }
    c = std::move(result.proposed);
    c.metadata.touch();
    error.clear();
    return true;
}
} // namespace CharacterAdvancement
