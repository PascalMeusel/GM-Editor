#pragma once

#include "Models/Map/MapGeneration.hpp"

#include <algorithm>
#include <string>

namespace MapFactsPresentation
{
inline const char *typeLabel(MapGenerationFactType type)
{
    switch (type)
    {
    case MapGenerationFactType::Note: return "Unrecognized note";
    case MapGenerationFactType::Nation: return "Nation";
    case MapGenerationFactType::NationCount: return "Number of nations";
    case MapGenerationFactType::Region: return "Region";
    case MapGenerationFactType::Settlement: return "City or settlement";
    case MapGenerationFactType::Capital: return "Capital";
    case MapGenerationFactType::WaterBody: return "Sea or lake";
    case MapGenerationFactType::River: return "River";
    case MapGenerationFactType::SharedRiver: return "Shared great river";
    case MapGenerationFactType::Coast: return "Coast";
    case MapGenerationFactType::Island: return "Island";
    case MapGenerationFactType::MountainRange: return "Mountain range";
    case MapGenerationFactType::Forest: return "Forest";
    case MapGenerationFactType::Road: return "Road";
    case MapGenerationFactType::RelativePosition: return "Relative position";
    case MapGenerationFactType::IncludedElement: return "Required element";
    case MapGenerationFactType::ExcludedElement: return "Excluded element";
    }
    return "Map fact";
}

inline const char *strengthLabel(MapGenerationFactStrength strength)
{
    switch (strength)
    {
    case MapGenerationFactStrength::Must: return "Must";
    case MapGenerationFactStrength::Prefer: return "Prefer";
    case MapGenerationFactStrength::Automatic: return "Automatic";
    }
    return "Automatic";
}

inline const char *recognitionLabel(const MapGenerationFact &fact)
{
    if (fact.type == MapGenerationFactType::Note || fact.confidence < 0.35f)
        return "Not understood";
    if (!fact.confirmed || fact.confidence < 0.80f)
        return "Check interpretation";
    return "Clearly recognized";
}

inline bool needsReview(const MapGenerationFact &fact)
{
    return !fact.confirmed || fact.confidence < 0.80f || fact.type == MapGenerationFactType::Note;
}

inline std::string displayValue(const MapGenerationFact &fact)
{
    if (fact.type == MapGenerationFactType::SharedRiver)
        return "One continuous great river for all named nations";
    if (!fact.label.empty())
    {
        if (fact.label == "mountain_range") return "Mountain range";
        if (fact.label == "shared_great_river") return "Shared great river";
        if (fact.label == "coast") return "Coast";
        if (fact.label == "island") return "Island";
        if (fact.label == "forest") return "Forest";
        if (fact.label == "river") return "River";
        if (fact.label == "road") return "Road";
        if (fact.label == "sea") return "Sea";
        if (fact.label == "lake") return "Lake";
        return fact.label;
    }
    return fact.value.empty() ? std::string("Unspecified") : fact.value;
}

inline bool displayValueIsUiLiteral(const MapGenerationFact &fact)
{
    if (fact.type == MapGenerationFactType::SharedRiver ||
        (fact.label.empty() && fact.value.empty()))
        return true;
    return fact.label == "mountain_range" || fact.label == "shared_great_river" ||
           fact.label == "coast" || fact.label == "island" || fact.label == "forest" ||
           fact.label == "river" || fact.label == "road" || fact.label == "sea" ||
           fact.label == "lake";
}

inline const MapGenerationFactValidation *validationForFact(
    const MapGenerationValidationReport &report,
    const MapGenerationFact &fact)
{
    const auto found = std::find_if(report.facts.begin(), report.facts.end(),
                                    [&](const MapGenerationFactValidation &validation) {
                                        return validation.factId == fact.id;
                                    });
    return found == report.facts.end() ? nullptr : &*found;
}

inline bool hasBlockingUnrecognizedNote(const MapGenerationValidationReport &report)
{
    return std::any_of(report.blockers.begin(), report.blockers.end(), [](const MapGenerationIssue &issue) {
        return issue.code == "request.must_note_untyped";
    });
}

inline int confidencePercent(const MapGenerationFact &fact)
{
    return std::clamp(static_cast<int>(fact.confidence * 100.0f + 0.5f), 0, 100);
}
}
