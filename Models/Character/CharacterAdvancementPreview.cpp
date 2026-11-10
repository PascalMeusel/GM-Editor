#include "CharacterAdvancementSupport.hpp"

namespace CharacterAdvancement
{
Preview preview(const CharacterModel &c, const IRuleSet &rules, const Request &r,
                const std::vector<SpellModel> *catalog)
{
    Preview out;
    out.proposed = c;
    auto &next = out.proposed;
    const bool dnd = dynamic_cast<const DND5e *>(&rules) != nullptr, override = !lines(r.overrideReason).empty();
    auto fail = [&](const char *message)
    {
        out.error = message;
        return out;
    };
    const std::string advancing = r.guided ? canonical(r.advancingClass) : c.selectedClass;
    const auto profile = rules.advancementProfile(advancing);
    if (!rules.levelsExist() || c.level < 1 || r.targetLevel <= c.level || r.targetLevel > profile.maximumLevel)
        return fail("Choose a higher level within the ruleset limit.");
    out.gainedLevels = r.targetLevel - c.level;
    if (r.guided)
    {
        auto levels = classLevels(c);
        long long total = 0;
        std::set<std::string> unique;
        for (const auto &[name, n] : levels)
        {
            if (n < 1 || !unique.insert(canonical(name)).second)
                return fail("Class history is invalid. Correct it before advancing.");
            total += n;
        }
        if (advancing.empty() || total != c.level || !levels.count(canonical(c.selectedClass)))
            return fail("Class history must match the current character level.");
        if (dnd && !override)
        {
            if (profile.fixedHitPointsPerLevel == 0)
                return fail("Custom progression requires a documented override.");
            if (!levels.count(advancing))
            {
                if (!qualifies(c, advancing))
                    return fail("Multiclass ability prerequisites are not met.");
                for (const auto &[name, n] : levels)
                    if (!qualifies(c, name))
                        return fail("Multiclass ability prerequisites are not met.");
            }
        }
        const int previous = levels.count(advancing) ? levels[advancing] : 0;
        levels[advancing] = previous + out.gainedLevels;
        next.classLevels = levels;
        if (dnd)
            for (int level = previous + 1; level <= levels[advancing]; ++level)
                if (level == 4 || level == 8 || level == 12 || level == 16 || level == 19 ||
                    (advancing == "Fighter" && (level == 6 || level == 14)) || (advancing == "Rogue" && level == 10))
                    ++out.featChoices;
        long long points = 0;
        const auto attributes = rules.getAttributes();
        for (const auto &[name, delta] : r.abilityIncreases)
        {
            if (!delta)
                continue;
            if (std::find(attributes.begin(), attributes.end(), name) == attributes.end())
                return fail("Unknown ability in advancement choices.");
            const long long value = static_cast<long long>(score(c, name)) + delta;
            if (value < 1 || value > std::numeric_limits<int>::max() ||
                (!override && (delta < 0 || (dnd && value > 20))))
                return fail("Ability changes exceed the supported limits.");
            next.attributes[name] = static_cast<int>(value);
            points += delta;
            if (const auto *defined = dynamic_cast<const DefinitionBackedRuleSet *>(&rules))
                for (const auto &ability : defined->definition().abilityScores)
                    if (ability.name == name)
                        next.ruleValues[ability.id] = static_cast<int>(value);
        }
        const auto feats = lines(r.newFeats);
        for (const auto &feat : feats)
            if (std::find(c.feats.begin(), c.feats.end(), feat) != c.feats.end())
                return fail("A selected feat is already present.");
        if (dnd && !override)
        {
            const int count = static_cast<int>(feats.size());
            if (count > out.featChoices || points > 2 * (out.featChoices - count) + count ||
                points < 2 * (out.featChoices - count))
                return fail("Resolve every feat or ability improvement choice, or document an override.");
        }
        appendUnique(next.feats, feats);
    }
    out.calculatedHitPoints = !r.manualHitPoints && profile.fixedHitPointsPerLevel > 0;
    if (!r.manualHitPoints && !out.calculatedHitPoints)
        return fail("This class requires an explicit manual HP gain.");
    const int modifier =
        profile.hitPointAbility.empty() ? 0 : rules.getAbilityModifier(c.attributes, profile.hitPointAbility);
    long long gain =
        r.manualHitPoints
            ? r.hitPointGain
            : static_cast<long long>(std::max(1, profile.fixedHitPointsPerLevel + modifier)) * out.gainedLevels;
    if (r.hitPointGain < 0 && r.manualHitPoints)
        return fail("HP gain is outside the supported range.");
    // Constitution changes apply to all attained levels, independently of rolled/fixed gain.
    if (r.guided && dnd)
        gain += static_cast<long long>(rules.getAbilityModifier(next.attributes, "Constitution") -
                                       rules.getAbilityModifier(c.attributes, "Constitution")) *
                r.targetLevel;
    CharacterModel normalized = c;
    normalizeCharacterHitPoints(normalized);
    const long long maximum = normalized.maxHitPoints + gain;
    if (maximum < 1 || maximum > std::numeric_limits<int>::max() || gain < std::numeric_limits<int>::min() ||
        gain > std::numeric_limits<int>::max())
        return fail("HP gain is outside the supported range.");
    out.hitPointGain = static_cast<int>(gain);
    out.maximumHitPoints = static_cast<int>(maximum);
    out.proficiencyBefore = rules.getProficiencyBonus(c.level);
    out.proficiencyAfter = rules.getProficiencyBonus(r.targetLevel);
    next.level = r.targetLevel;
    next.maxHitPoints = out.maximumHitPoints;
    next.currentHitPoints =
        r.restoreHealth ? out.maximumHitPoints : std::clamp(c.currentHitPoints, 0, out.maximumHitPoints);
    appendUnique(next.traitsAndFeatures, lines(r.newFeatures));
    if (r.guided)
    {
        next.spellcasting.slotMaximums.resize(10, 0);
        next.spellcasting.slotUsed.resize(10, 0);
        if (r.automaticSlots && dnd)
            next.spellcasting.slotMaximums = standardSlots(next);
        else if (!r.slotMaximums.empty())
        {
            if (r.slotMaximums.size() != 10)
                return fail("Spell slot limits must include levels zero through nine.");
            next.spellcasting.slotMaximums = r.slotMaximums;
        }
        for (int level = 0; level < 10; ++level)
            if (next.spellcasting.slotUsed[level] < 0 ||
                next.spellcasting.slotMaximums[level] < next.spellcasting.slotUsed[level] ||
                next.spellcasting.slotMaximums[level] < 0 || next.spellcasting.slotMaximums[level] > 1000 ||
                (level == 0 && next.spellcasting.slotMaximums[level] != 0))
                return fail("Slot limits cannot discard spent slots or exceed supported bounds.");
        for (const auto &id : r.spellsToRemove)
        {
            auto i = std::find_if(next.spellcasting.spells.begin(), next.spellcasting.spells.end(),
                                  [&](const auto &spell) { return spell.spellId == id; });
            if (i == next.spellcasting.spells.end())
                return fail("A selected spell is no longer assigned.");
            next.spellcasting.spells.erase(i);
        }
        for (auto entry : r.spellsToAdd)
        {
            if (!catalog)
                return fail("The spell library is unavailable.");
            auto spell = std::find_if(catalog->begin(), catalog->end(),
                                      [&](const auto &s) { return s.metadata.id == entry.spellId; });
            if (spell == catalog->end())
                return fail("A selected spell no longer exists.");
            if (std::any_of(next.spellcasting.spells.begin(), next.spellcasting.spells.end(),
                            [&](const auto &s) { return s.spellId == entry.spellId; }))
                return fail("A selected spell is already assigned.");
            if (dnd && !override)
            {
                CharacterModel single = next;
                single.classLevels = {{advancing, next.classLevels.at(advancing)}};
                auto slots = standardSlots(single);
                int highest = pactSlotLevel(single);
                for (int level = 1; level <= 9; ++level)
                    if (slots[level])
                        highest = level;
                if (spell->level > highest || castingAbility(advancing).empty() ||
                    (!spell->classes.empty() && lower(spell->classes).find(lower(advancing)) == std::string::npos))
                    return fail("The selected spell is not available to this class level.");
            }
            entry.source = advancing;
            next.spellcasting.spells.push_back(entry);
            next.spellcasting.enabled = true;
        }
        if (dnd && r.automaticResources)
        {
            int n = classLevel(next, "Monk");
            if (n >= 2)
                resource(next, "dnd-2024.focus", "Focus Points", n, "Short Rest");
            n = classLevel(next, "Sorcerer");
            if (n >= 2)
                resource(next, "dnd-2024.sorcery", "Sorcery Points", n, "Long Rest");
            n = classLevel(next, "Warlock");
            if (n >= 1)
                resource(next, "dnd-2024.pact-slots", "Pact Magic", n == 1 ? 1 : (n < 11 ? 2 : (n < 17 ? 3 : 4)),
                         "Short Rest");
        }
        std::set<std::string> changedResources;
        for (const auto &change : r.resourceChanges)
        {
            if (change.metadata.id.empty() || !changedResources.insert(change.metadata.id).second ||
                lines(change.name).empty())
                return fail("Resource choices need unique identities and names.");
            auto i = std::find_if(next.resourceTrackers.begin(), next.resourceTrackers.end(),
                                  [&](const auto &item) { return item.metadata.id == change.metadata.id; });
            if (i == next.resourceTrackers.end())
            {
                if (change.used != 0)
                    return fail("New advancement resources must start unspent.");
                next.resourceTrackers.push_back(change);
            }
            else
            {
                i->name = change.name;
                i->maximum = change.maximum;
                i->rechargeRule = change.rechargeRule;
            }
        }
        for (const auto &tracker : next.resourceTrackers)
            if (tracker.used < 0 || tracker.maximum < tracker.used || tracker.maximum < 0 || tracker.maximum > 1000000)
                return fail("Resource limits cannot discard spent uses or exceed supported bounds.");
        if (std::any_of(next.spellcasting.slotMaximums.begin(), next.spellcasting.slotMaximums.end(),
                        [](int n) { return n > 0; }) ||
            pactSlotLevel(next) > 0)
            next.spellcasting.enabled = true;
        if (dnd && !c.spellcasting.enabled && !castingAbility(advancing).empty())
            next.spellcasting.ability = castingAbility(advancing);
    }
    normalizeSpellcasting(next.spellcasting);
    if (!r.guided && !next.classLevels.empty())
        next.classLevels[canonical(c.selectedClass)] += out.gainedLevels;
    auto add =
        [&](const std::string &label, const std::string &before, const std::string &after, bool calculated = false)
    {
        if (before != after)
            out.changes.push_back({label, before, after, calculated});
    };
    auto number = [&](const std::string &label, int before, int after, bool calculated = false)
    { add(label, std::to_string(before), std::to_string(after), calculated); };
    number("Level", c.level, next.level);
    add("Classes", describeClasses(classLevels(c)), describeClasses(classLevels(next)));
    number("Maximum HP", normalized.maxHitPoints, next.maxHitPoints, out.calculatedHitPoints);
    number("Current HP", c.currentHitPoints, next.currentHitPoints);
    number("Proficiency", out.proficiencyBefore, out.proficiencyAfter, true);
    for (const auto &[name, n] : next.attributes)
        number(name, score(c, name), n);
    for (const auto &feat : next.feats)
        if (std::find(c.feats.begin(), c.feats.end(), feat) == c.feats.end())
            add("Feat", "", feat);
    for (const auto &feature : next.traitsAndFeatures)
        if (std::find(c.traitsAndFeatures.begin(), c.traitsAndFeatures.end(), feature) == c.traitsAndFeatures.end())
            add("Feature", "", feature);
    for (int level = 1; level <= 9; ++level)
        number("Spell slots " + std::to_string(level),
               level < int(c.spellcasting.slotMaximums.size()) ? c.spellcasting.slotMaximums[level] : 0,
               next.spellcasting.slotMaximums[level], r.automaticSlots && dnd);
    for (const auto &id : r.spellsToRemove)
    {
        std::string name = id;
        if (catalog)
            for (const auto &s : *catalog)
                if (s.metadata.id == id)
                    name = s.name;
        add("Spell", name, "");
    }
    for (const auto &entry : r.spellsToAdd)
    {
        std::string name = entry.spellId;
        if (catalog)
            for (const auto &s : *catalog)
                if (s.metadata.id == entry.spellId)
                    name = s.name;
        add(entry.prepared ? "Prepared spell" : "Spell", "", name + " (" + advancing + ")");
    }
    for (const auto &tracker : next.resourceTrackers)
    {
        auto old = std::find_if(c.resourceTrackers.begin(), c.resourceTrackers.end(),
                                [&](const auto &item) { return item.metadata.id == tracker.metadata.id; });
        auto summary = [](const auto &item)
        {
            return item.name + ": " + std::to_string(item.maximum) + " / " + item.rechargeRule + " (" +
                   std::to_string(item.used) + " used)";
        };
        add("Resource", old == c.resourceTrackers.end() ? "" : summary(*old), summary(tracker),
            r.automaticResources && tracker.definitionId.find("dnd-2024.") == 0 &&
            std::none_of(r.resourceChanges.begin(), r.resourceChanges.end(), [&](const auto &change) { return change.metadata.id == tracker.metadata.id; }));
    }
    if (!lines(r.overrideReason).empty())
    {
        add("Override reason", "", r.overrideReason);
        if (!next.gmNotes.empty())
            next.gmNotes += "\n";
        next.gmNotes += "Advancement override (level " + std::to_string(next.level) + "): " + r.overrideReason;
    }
    out.valid = true;
    return out;
}
} // namespace CharacterAdvancement
