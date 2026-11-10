#pragma once

#include "Models/Map/MapWorldBriefRelations.hpp"

namespace MapWorldBriefInterpreter
{
inline void addOrUpgradeFact(std::vector<MapGenerationFact> &facts, MapGenerationFact fact)
{
    const auto existing = std::find_if(facts.begin(), facts.end(), [&](const MapGenerationFact &entry) {
        return entry.id == fact.id;
    });
    if (existing == facts.end())
    {
        facts.push_back(std::move(fact));
        return;
    }
    const auto priority = [](MapGenerationFactStrength strength) {
        return strength == MapGenerationFactStrength::Must ? 3 :
               (strength == MapGenerationFactStrength::Prefer ? 2 : 1);
    };
    if (priority(fact.strength) > priority(existing->strength))
        existing->strength = fact.strength;
    existing->confidence = std::max(existing->confidence, fact.confidence);
    existing->confirmed = existing->confirmed || fact.confirmed;
    if (!fact.sourceText.empty() && existing->sourceText.find(fact.sourceText) == std::string::npos)
        existing->sourceText += (existing->sourceText.empty() ? "" : "\n") + fact.sourceText;
}

inline MapGenerationFact semanticFact(const std::string &key,
                                      MapGenerationFactType type,
                                      MapGenerationFactStrength strength,
                                      const std::string &sourceText,
                                      bool excluded)
{
    MapGenerationFact fact;
    fact.id = std::string(excluded ? "brief_exclude_" : "brief_feature_") + key;
    fact.type = excluded ? MapGenerationFactType::ExcludedElement : type;
    fact.strength = excluded ? MapGenerationFactStrength::Must : strength;
    fact.label = key;
    fact.value = key;
    fact.sourceText = sourceText;
    fact.confidence = excluded ? 0.98f :
                      (strength == MapGenerationFactStrength::Automatic ? 0.76f :
                       (strength == MapGenerationFactStrength::Prefer ? 0.90f : 0.96f));
    fact.confirmed = true;
    return fact;
}

inline void addUnrecognizedNote(MapWorldBrief &result, const std::string &clause, bool explicitMust)
{
    const std::string cleaned = trim(clause);
    if (cleaned.empty())
        return;
    if (std::find(result.unrecognizedText.begin(), result.unrecognizedText.end(), cleaned) == result.unrecognizedText.end())
        result.unrecognizedText.push_back(cleaned);
    MapGenerationFact fact;
    fact.id = "brief_note_" + std::to_string(stableTextHash(asciiLower(cleaned)));
    fact.type = MapGenerationFactType::Note;
    // Explicitly required prose that cannot be executed must fail closed.
    // Ordinary flavour text stays visible without blocking generation.
    fact.strength = explicitMust ? MapGenerationFactStrength::Must : MapGenerationFactStrength::Automatic;
    fact.label = cleaned;
    fact.value = cleaned;
    fact.sourceText = cleaned;
    fact.confidence = 0.0f;
    fact.confirmed = false;
    addOrUpgradeFact(result.facts, std::move(fact));
}

} // namespace MapWorldBriefInterpreter
