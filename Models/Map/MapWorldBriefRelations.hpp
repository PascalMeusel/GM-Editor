#pragma once

#include "Models/Map/MapWorldBriefParsing.hpp"

namespace MapWorldBriefInterpreter
{
inline void readNaturalNames(const std::string &source, MapWorldBrief &result)
{
    static const std::regex nations(R"((?:nations|nationen)\s*(?::|are|sind)?\s+([^.!?\n]+))", std::regex::icase);
    std::smatch match;
    if (result.nationNames.empty() && std::regex_search(source, match, nations))
    {
        std::string list = match[1].str();
        const std::string lower = asciiLower(list);
        for (const auto *stop : {" share ", " border ", " lie ", " teilen ", " grenzen ", " liegen ", ";"})
        {
            const auto pos = lower.find(stop);
            if (pos != std::string::npos) list.resize(std::min(list.size(), pos));
        }
        auto names = splitNames(list);
        if (!names.empty() && std::all_of(names.begin(), names.end(), plausibleProperName) &&
            (result.statedNationCount == 0 || names.size() == static_cast<std::size_t>(result.statedNationCount)))
            result.nationNames = std::move(names);
    }
    static const std::regex subjectCapital(R"((?:^|[.!?\n]\s*)([^.!?,;\n]+?)\s+(?:is|ist)\s+(?:(?:the|a|die|eine)\s+)?(?:(?:coastal|northern|southern|eastern|western|küstennah[e]?|nördliche|südliche)\s+)?(?:capital|hauptstadt)\b)", std::regex::icase);
    static const std::regex objectCapital(R"((?:capital|hauptstadt)\s*(?::|is called|is named|is|ist|heißt|heisst)?\s+([^.!?,;\n]+))", std::regex::icase);
    const auto cleanName = [](std::string value) {
        const auto lower = asciiLower(value);
        for (const auto *stop : {" on ", " at ", " in ", " near ", " im ", " am ", " an ", " liegt ", " is ", " ist ", " sits "})
        {
            const auto pos = lower.find(stop);
            if (pos != std::string::npos) value.resize(std::min(value.size(), pos));
        }
        return trim(value);
    };
    if (result.capitalName.empty())
    {
        if (std::regex_search(source, match, subjectCapital))
        {
            const auto name = cleanName(match[1].str());
            if (plausibleProperName(name)) result.capitalName = name;
        }
        else if (std::regex_search(source, match, objectCapital))
        {
            const auto name = cleanName(match[1].str());
            if (plausibleProperName(name)) result.capitalName = name;
        }
    }
    static const std::regex subjectTown(R"((?:^|[.!?\n]\s*)([^.!?,;\n]+?)\s+(?:is|ist)\s+(?:(?:the|a|an|die|eine)\s+)?(?:(?:coastal|northern|southern|eastern|western|small|kleine|östliche|westliche)\s+)?(?:town|city|stadt)\b)", std::regex::icase);
    static const std::regex objectTown(R"((?:town|city|stadt)\s+(?:named\s+|called\s+|namens\s+)?([^.!?,;\n]+))", std::regex::icase);
    for (const auto *pattern : {&subjectTown, &objectTown})
        for (auto it = std::sregex_iterator(source.begin(), source.end(), *pattern); it != std::sregex_iterator(); ++it)
        {
            const auto name = cleanName((*it)[1].str());
            if (plausibleProperName(name) && asciiLower(name) != asciiLower(result.capitalName) &&
                std::none_of(result.cityNames.begin(), result.cityNames.end(), [&](const auto &existing) { return asciiLower(existing) == asciiLower(name); }))
                result.cityNames.push_back(name);
        }
}

inline MapGenerationFactStrength strengthForClause(const std::string &lower)
{
    if (containsAnyTerm(lower, {"automatisch", "automatic", "auto"}))
        return MapGenerationFactStrength::Automatic;
    if (containsAnyTerm(lower, {
            "sollte", "sollten", "wunsch", "wünsche", "wuensche", "hätte gern", "haette gern",
            "wäre schön", "waere schoen", "möglichst", "moeglichst", "vorzugsweise",
            "should", "prefer", "preferred", "would like", "would be nice", "ideally", "maybe", "perhaps"}))
        return MapGenerationFactStrength::Prefer;
    // Unqualified declarative statements are requirements in the world brief:
    // the user can explicitly downgrade them in the Facts step.
    return MapGenerationFactStrength::Must;
}

inline bool clauseIsNegative(const std::string &lower)
{
    return containsAnyTerm(lower, {
        "kein", "keine", "keinen", "keiner", "keines", "ohne", "ausschließen", "ausschliessen",
        "nicht enthalten", "darf nicht", "dürfen nicht", "duerfen nicht",
        "no", "without", "exclude", "excluded", "must not", "may not", "do not include"});
}

inline bool clauseHasExplicitMust(const std::string &lower)
{
    return containsAnyTerm(lower, {
        "muss", "müssen", "muessen", "benötigt", "benoetigt", "zwingend", "unbedingt",
        "must", "needs", "required", "has to", "have to"});
}

inline bool clauseHasUnresolvedRelation(const std::string &lower)
{
    return containsAnyTerm(lower, {
        "nördlich", "noerdlich", "südlich", "suedlich", "östlich", "oestlich", "westlich",
        "norden", "süden", "sueden", "osten", "westen", "zwischen", "neben", "nahe", "grenzt an",
        "north of", "south of", "east of", "west of", "between", "beside", "near", "borders",
        "connects", "connect", "verbindet", "verbinden"});
}

inline void stripRelationPrefix(std::string &value, const std::vector<std::string> &prefixes)
{
    bool changed = true;
    while (changed && !value.empty())
    {
        changed = false;
        const std::string lower = asciiLower(value);
        for (const std::string &prefix : prefixes)
        {
            if (lower.rfind(prefix, 0) != 0)
                continue;
            value = trim(value.substr(prefix.size()));
            changed = true;
            break;
        }
    }
}

inline void stripRelationSuffix(std::string &value, const std::vector<std::string> &suffixes)
{
    bool changed = true;
    while (changed && !value.empty())
    {
        changed = false;
        const std::string lower = asciiLower(value);
        for (const std::string &suffix : suffixes)
        {
            if (lower.size() < suffix.size() ||
                lower.compare(lower.size() - suffix.size(), suffix.size(), suffix) != 0)
                continue;
            value = trim(value.substr(0, value.size() - suffix.size()));
            changed = true;
            break;
        }
    }
}

// Relation entity extraction intentionally accepts only a small, explicit
// grammar.  A loose noun-phrase guess would be worse here: it could turn
// flavour prose into a confirmed Must fact and let the application claim that
// it understood something which it did not.
inline std::string cleanRelationEntity(std::string value)
{
    value = trim(std::move(value));
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\'')))
        value = trim(value.substr(1, value.size() - 2));

    stripRelationPrefix(value, {
        "und ", "and ", "die nation ", "der nation ", "dem land ", "das land ",
        "der staat ", "das reich ", "die stadt ", "der stadt ", "die hauptstadt ",
        "the nation ", "the country ", "the realm ", "the city ", "the capital ",
        "nation ", "land ", "staat ", "reich ", "stadt ", "hauptstadt ", "city ", "capital "});
    stripRelationSuffix(value, {
        " befindet sich", " is located", " liegt", " liege", " lies", " is", " ist"});

    value = trim(std::move(value));
    if (value.empty() || value.size() > 80)
        return {};
    const std::string lower = asciiLower(value);
    if (containsAnyTerm(lower, {
            "muss", "must", "nördlich", "noerdlich", "südlich", "suedlich",
            "östlich", "oestlich", "westlich", "north of", "south of", "east of",
            "west of", "zwischen", "between", "grenzt an", "borders"}))
        return {};
    return value;
}

inline std::vector<std::pair<std::size_t, std::string>> knownRelationEntities(
    const std::string &source,
    const std::vector<std::string> &knownNames,
    std::size_t begin,
    std::size_t end)
{
    const std::string lower = asciiLower(source);
    std::vector<std::pair<std::size_t, std::string>> matches;
    for (const std::string &name : knownNames)
    {
        if (name.empty())
            continue;
        const std::string normalizedName = asciiLower(name);
        std::size_t position = begin;
        while ((position = lower.find(normalizedName, position)) != std::string::npos && position < end)
        {
            const std::size_t nameEnd = position + normalizedName.size();
            const bool leftBoundary = position == 0 || !isWordByte(static_cast<unsigned char>(lower[position - 1]));
            const bool rightBoundary = nameEnd >= lower.size() || !isWordByte(static_cast<unsigned char>(lower[nameEnd]));
            if (leftBoundary && rightBoundary && nameEnd <= end)
                matches.emplace_back(position, name);
            position = std::max(position + 1, nameEnd);
        }
    }
    std::stable_sort(matches.begin(), matches.end(), [](const auto &left, const auto &right) {
        if (left.first != right.first)
            return left.first < right.first;
        // Prefer a complete known name over a shorter name that starts at the
        // same byte (for example "North Reach" over "North").
        return left.second.size() > right.second.size();
    });
    matches.erase(std::unique(matches.begin(), matches.end(), [](const auto &left, const auto &right) {
        return left.first == right.first || asciiLower(left.second) == asciiLower(right.second);
    }), matches.end());
    return matches;
}

struct RelationParseResult
{
    bool markerPresent = false;
    bool parsed = false;
};

inline void addOrUpgradeFact(std::vector<MapGenerationFact> &facts, MapGenerationFact fact);

inline RelationParseResult addRelationalFact(MapWorldBrief &result,
                                             const std::string &clause,
                                             const std::vector<std::string> &knownNames)
{
    struct RelationGrammar
    {
        const char *canonicalValue;
        std::vector<std::string> markers;
        int referencedObjects;
    };
    const std::vector<RelationGrammar> grammar = {
        {"north_of", {"nördlich von", "noerdlich von", "im norden von", "north of"}, 1},
        {"south_of", {"südlich von", "suedlich von", "im süden von", "im sueden von", "south of"}, 1},
        {"east_of", {"östlich von", "oestlich von", "im osten von", "east of"}, 1},
        {"west_of", {"westlich von", "im westen von", "west of"}, 1},
        {"borders", {"grenzt an", "grenzen an", "borders", "border"}, 1},
        {"between", {"zwischen", "between"}, 2}};

    const std::string lower = asciiLower(clause);
    const RelationGrammar *selected = nullptr;
    std::size_t selectedPosition = std::string::npos;
    std::size_t selectedLength = 0;
    int relationKindsPresent = 0;
    for (const RelationGrammar &entry : grammar)
    {
        std::size_t entryPosition = std::string::npos;
        std::size_t entryLength = 0;
        for (const std::string &marker : entry.markers)
        {
            std::size_t position = lower.find(marker);
            while (position != std::string::npos)
            {
                const std::size_t markerEnd = position + marker.size();
                const bool leftBoundary = position == 0 || !isWordByte(static_cast<unsigned char>(lower[position - 1]));
                const bool rightBoundary = markerEnd >= lower.size() || !isWordByte(static_cast<unsigned char>(lower[markerEnd]));
                if (leftBoundary && rightBoundary)
                    break;
                position = lower.find(marker, markerEnd);
            }
            if (position != std::string::npos &&
                (entryPosition == std::string::npos || position < entryPosition))
            {
                entryPosition = position;
                entryLength = marker.size();
            }
        }
        if (entryPosition == std::string::npos)
            continue;
        ++relationKindsPresent;
        if (selectedPosition == std::string::npos || entryPosition < selectedPosition)
        {
            selected = &entry;
            selectedPosition = entryPosition;
            selectedLength = entryLength;
        }
    }

    RelationParseResult parse;
    parse.markerPresent = relationKindsPresent > 0 || clauseHasUnresolvedRelation(lower);
    // Multiple different relationships in one clause are deliberately sent to
    // review instead of partially accepting only the first one.
    if (!selected || relationKindsPresent != 1)
        return parse;

    std::string subject;
    const auto knownSubjects = knownRelationEntities(clause, knownNames, 0, selectedPosition);
    // Exactly one declared subject is required.  Choosing the last match from
    // a plural subject silently discarded information ("Arken and Liora ..."
    // became just "Liora ...").  Multi-subject relations remain reviewable
    // prose until the model has an explicit representation for them.
    if (knownSubjects.size() != 1)
        return parse;
    subject = knownSubjects.front().second;

    const std::size_t objectBegin = selectedPosition + selectedLength;
    std::vector<std::string> objects;
    const auto knownObjects = knownRelationEntities(clause, knownNames, objectBegin, clause.size());
    if (knownObjects.size() != static_cast<std::size_t>(selected->referencedObjects))
        return parse;
    for (const auto &[position, name] : knownObjects)
    {
        (void)position;
        if (std::none_of(objects.begin(), objects.end(), [&](const std::string &existing) {
                return asciiLower(existing) == asciiLower(name);
            }))
            objects.push_back(name);
    }
    if (subject.empty() || objects.size() != static_cast<std::size_t>(selected->referencedObjects) ||
        std::any_of(objects.begin(), objects.end(), [](const std::string &object) { return object.empty(); }))
        return parse;
    if (std::any_of(objects.begin(), objects.end(), [&](const std::string &object) {
            return asciiLower(object) == asciiLower(subject);
        }))
        return parse;

    // A relative-position fact may only reference entities that the brief has
    // actually declared.  Previously the fallback noun-phrase parser turned
    // "Arken and Liora border a vast forest" into the invalid pair
    // {Liora, "a vast forest"}.  The forest is already represented by its
    // semantic Forest fact; inventing a point/nation relationship for that
    // phrase makes the application's own idea suggestion impossible to run.
    const auto isKnownEntity = [&](const std::string &entity) {
        return std::any_of(knownNames.begin(), knownNames.end(), [&](const std::string &known) {
            return asciiLower(known) == asciiLower(entity);
        });
    };
    if (!isKnownEntity(subject) ||
        std::any_of(objects.begin(), objects.end(), [&](const std::string &object) {
            return !isKnownEntity(object);
        }))
        return parse;

    MapGenerationFact fact;
    fact.type = MapGenerationFactType::RelativePosition;
    fact.strength = strengthForClause(lower);
    fact.value = selected->canonicalValue;
    fact.entityRefs.push_back(subject);
    fact.entityRefs.insert(fact.entityRefs.end(), objects.begin(), objects.end());
    std::string signature = fact.value;
    fact.label = subject;
    for (const std::string &entity : objects)
    {
        fact.label += " | " + entity;
        signature += "|" + asciiLower(entity);
    }
    signature += "|" + asciiLower(subject);
    fact.id = "brief_relation_" + fact.value + "_" + std::to_string(stableTextHash(signature));
    fact.sourceText = clause;
    fact.confidence = fact.strength == MapGenerationFactStrength::Automatic ? 0.76f : 0.94f;
    fact.confirmed = true;
    addOrUpgradeFact(result.facts, std::move(fact));
    parse.parsed = true;
    return parse;
}

} // namespace MapWorldBriefInterpreter
