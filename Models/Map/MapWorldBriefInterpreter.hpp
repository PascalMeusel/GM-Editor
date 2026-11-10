#pragma once

#include "Models/Map/MapWorldBriefFacts.hpp"

namespace MapWorldBriefInterpreter
{
inline MapWorldBrief interpret(const std::string &source)
{
    MapWorldBrief result;
    const std::string lower = asciiLower(source);
    const bool mentionsRiver = lower.find("river") != std::string::npos ||
                               lower.find("fluss") != std::string::npos;
    const bool describesSharing = lower.find("share") != std::string::npos ||
                                  lower.find("teilen") != std::string::npos;
    const bool describesLargeRiver = lower.find("great river") != std::string::npos ||
                                     lower.find("large river") != std::string::npos ||
                                     lower.find("großen fluss") != std::string::npos ||
                                     lower.find("grossen fluss") != std::string::npos ||
                                     lower.find("großer fluss") != std::string::npos ||
                                     lower.find("grosser fluss") != std::string::npos;
    result.requiresSharedGreatRiver = mentionsRiver && describesSharing && describesLargeRiver;
    std::size_t nationPosition = lower.find("nationen");
    if (nationPosition == std::string::npos)
        nationPosition = lower.find("nations");
    if (nationPosition == std::string::npos)
        nationPosition = lower.find("nation");
    if (nationPosition != std::string::npos)
    {
        result.statedNationCount = countBefore(lower, nationPosition);
        std::size_t markerLength = 0;
        const std::size_t marker = firstMarkerAfter(lower, nationPosition,
            {" mit namen ", " mit name ", " namens ", " called ", " named ", " names "}, markerLength);
        if (marker != std::string::npos && marker < nationPosition + 100)
        {
            const std::size_t begin = marker + markerLength;
            std::size_t end = source.size();
            // Stop at the first predicate following the name list.  The three
            // built-in ideas deliberately use different predicates (share,
            // border and lie around); without these markers the predicate was
            // incorrectly stored as part of the final nation name.
            for (const std::string &stop : {
                     ". ", ".", " sowie ", " as well as ", " und es gibt ", " and there is ",
                     " außerdem ", " additionally ", " share ", " shares ", " border ", " borders ",
                     " lie ", " lies ", " surround ", " surrounds ", " teilen ", " teilt ",
                     " grenzen ", " grenzt ", " liegen ", " liegt ", " umgeben "})
            {
                const std::size_t found = lower.find(stop, begin);
                if (found != std::string::npos)
                    end = std::min(end, found);
            }
            result.nationNames = splitNames(source.substr(begin, end - begin));
        }
    }

    std::size_t capitalPosition = lower.find("hauptstadt");
    if (capitalPosition == std::string::npos)
        capitalPosition = lower.find("capital");
    if (capitalPosition != std::string::npos)
        result.capitalName = extractSingleName(source, lower, capitalPosition);

    std::size_t cityPosition = lower.find("städte");
    if (cityPosition == std::string::npos)
        cityPosition = lower.find("staedte");
    if (cityPosition == std::string::npos)
        cityPosition = lower.find("cities");
    if (cityPosition != std::string::npos)
    {
        std::size_t markerLength = 0;
        const std::size_t marker = firstMarkerAfter(lower, cityPosition,
            {" mit namen ", " mit name ", " namens ", " called ", " named ", " names "}, markerLength);
        if (marker != std::string::npos && marker < cityPosition + 100)
        {
            const std::size_t begin = marker + markerLength;
            std::size_t end = source.size();
            for (const std::string &stop : {". ", ".", " sowie ", " as well as ", " und es gibt ", " and there is ", " außerdem ", " additionally "})
            {
                const std::size_t found = lower.find(stop, begin);
                if (found != std::string::npos)
                    end = std::min(end, found);
            }
            result.cityNames = splitNames(source.substr(begin, end - begin));
            result.cityNames.erase(std::remove_if(result.cityNames.begin(), result.cityNames.end(), [&](const std::string &name) {
                return asciiLower(name) == asciiLower(result.capitalName);
            }), result.cityNames.end());
        }
    }

    readNaturalNames(source, result);

    if (result.statedNationCount > 0)
    {
        MapGenerationFact fact;
        fact.id = "brief_nation_count";
        fact.type = MapGenerationFactType::NationCount;
        fact.strength = MapGenerationFactStrength::Must;
        fact.label = std::to_string(result.statedNationCount);
        fact.value = fact.label;
        fact.sourceText = source;
        fact.confirmed = true;
        result.facts.push_back(std::move(fact));
    }
    for (std::size_t index = 0; index < result.nationNames.size(); ++index)
    {
        MapGenerationFact fact;
        fact.id = "brief_nation_" + std::to_string(index);
        fact.type = MapGenerationFactType::Nation;
        fact.strength = MapGenerationFactStrength::Must;
        fact.label = result.nationNames[index];
        fact.value = result.nationNames[index];
        fact.sourceText = source;
        fact.confirmed = true;
        result.facts.push_back(std::move(fact));
    }
    if (!result.capitalName.empty())
    {
        MapGenerationFact fact;
        fact.id = "brief_capital";
        fact.type = MapGenerationFactType::Capital;
        fact.strength = MapGenerationFactStrength::Must;
        fact.label = result.capitalName;
        fact.value = result.capitalName;
        fact.sourceText = source;
        fact.confirmed = true;
        result.facts.push_back(std::move(fact));
    }
    for (std::size_t index = 0; index < result.cityNames.size(); ++index)
    {
        MapGenerationFact fact;
        fact.id = "brief_city_" + std::to_string(index);
        fact.type = MapGenerationFactType::Settlement;
        fact.strength = MapGenerationFactStrength::Must;
        fact.label = result.cityNames[index];
        fact.value = result.cityNames[index];
        fact.sourceText = source;
        fact.confirmed = true;
        result.facts.push_back(std::move(fact));
    }
    if (result.requiresSharedGreatRiver)
    {
        MapGenerationFact fact;
        fact.id = "brief_shared_great_river";
        fact.type = MapGenerationFactType::SharedRiver;
        fact.strength = MapGenerationFactStrength::Must;
        fact.label = "shared_great_river";
        fact.value = "shared_great_river";
        fact.entityRefs = result.nationNames;
        fact.sourceText = source;
        fact.confirmed = true;
        result.facts.push_back(std::move(fact));
    }

    struct SemanticVocabulary
    {
        const char *key;
        MapGenerationFactType type;
        std::vector<std::string> terms;
    };
    // Iteration order is the canonical order.  Equivalent German and English
    // briefs consequently produce identical executable fact sequences even
    // though their word order differs.
    const std::vector<SemanticVocabulary> vocabulary = {
        {"coast", MapGenerationFactType::Coast,
         {"küste", "kueste", "küsten", "kuesten", "coast", "coasts", "coastline", "coastlines"}},
        {"island", MapGenerationFactType::Island,
         {"insel", "inseln", "island", "islands"}},
        {"sea", MapGenerationFactType::WaterBody,
         {"meer", "meere", "ozean", "ozeane", "sea", "seas", "ocean", "oceans"}},
        {"lake", MapGenerationFactType::WaterBody,
         {"see", "seen", "binnensee", "binnenseen", "lake", "lakes"}},
        {"river", MapGenerationFactType::River,
         {"fluss", "flüsse", "fluesse", "river", "rivers"}},
        {"mountain_range", MapGenerationFactType::MountainRange,
         {"gebirge", "bergkette", "bergketten", "berge", "mountain range", "mountain ranges", "mountains"}},
        {"forest", MapGenerationFactType::Forest,
         {"wald", "wälder", "waelder", "forst", "forest", "forests", "woodland", "woodlands", "woods"}},
         {"road", MapGenerationFactType::Road,
         {"straße", "strasse", "straßen", "strassen", "fernstraße", "fernstrasse", "weg", "wege",
          "road", "roads", "highway", "highways"}}
    };

    std::vector<std::string> knownRelationNames = result.nationNames;
    knownRelationNames.insert(knownRelationNames.end(), result.cityNames.begin(), result.cityNames.end());
    if (!result.capitalName.empty())
        knownRelationNames.push_back(result.capitalName);

    for (const std::string &clause : splitClauses(source))
    {
        const std::string clauseLower = asciiLower(clause);
        const bool excluded = clauseIsNegative(clauseLower);
        const MapGenerationFactStrength strength = strengthForClause(clauseLower);
        bool recognized = false;
        bool semanticFeatureRecognized = false;
        for (const SemanticVocabulary &semantic : vocabulary)
        {
            if (!containsAnyTerm(clauseLower, semantic.terms))
                continue;
            recognized = true;
            semanticFeatureRecognized = true;
            // SharedRiver is the stronger executable promise, but retaining a
            // canonical River fact also records the user's explicit feature.
            addOrUpgradeFact(result.facts,
                             semanticFact(semantic.key, semantic.type, strength, clause, excluded));
        }

        const bool namesKnown =
            (containsAnyTerm(clauseLower, {"nation", "nationen", "nations"}) &&
             (result.statedNationCount > 0 || !result.nationNames.empty())) ||
            (containsAnyTerm(clauseLower, {"hauptstadt", "capital"}) && !result.capitalName.empty()) ||
            (containsAnyTerm(clauseLower, {"stadt", "städte", "staedte", "city", "cities"}) &&
             (!result.cityNames.empty() || !result.capitalName.empty()));
        recognized = recognized || namesKnown ||
                     (result.requiresSharedGreatRiver && containsAnyTerm(clauseLower, {"fluss", "river"}));

        const RelationParseResult relation = addRelationalFact(result, clause, knownRelationNames);
        recognized = recognized || relation.parsed;
        if (!recognized || (relation.markerPresent && !relation.parsed))
        {
            // A declarative relationship is a Must by default, just like the
            // other world-brief facts.  If the strict relation grammar cannot
            // resolve every participant, preserve it as a blocking note rather
            // than silently dropping the unknown part of the request.
            // Keep an unsupported landscape relationship visible, but do not
            // let ordinary generated prose block the executable semantic fact
            // that was understood (for example the required Forest).  An
            // explicit "must" remains fail-closed, as does a relationship for
            // which no usable semantic content was recognized.
            const bool failClosed = relation.markerPresent
                                        ? (clauseHasExplicitMust(clauseLower) ||
                                           (!semanticFeatureRecognized &&
                                            strengthForClause(clauseLower) == MapGenerationFactStrength::Must))
                                        : (!recognized && clauseHasExplicitMust(clauseLower));
            addUnrecognizedNote(result, clause, failClosed);
        }
    }

    return result;
}
} // namespace MapWorldBriefInterpreter
