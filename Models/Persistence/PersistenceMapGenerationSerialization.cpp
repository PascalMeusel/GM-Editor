#include "PersistenceSerialization.hpp"

#include <algorithm>
#include <charconv>
#include <exception>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace PersistenceSerialization
{
std::string serializeMapGenerationMetadata(const MapGenerationMetadata &generation)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char pointSeparator = ';';
    std::stringstream stream;
    // Generated feature points are part of the deterministic map contract.
    // max_digits10 guarantees an exact float round-trip instead of silently
    // changing feature-graph hashes after a save/load cycle.
    stream << std::setprecision(std::numeric_limits<float>::max_digits10);
    stream << "M" << fieldSeparator
           << (generation.enabled ? 1 : 0) << fieldSeparator
           << (generation.continuousRendering ? 1 : 0) << fieldSeparator
           << generation.recipe.generatorVersion << fieldSeparator
           << generation.recipe.recipeId << fieldSeparator
           << generation.recipe.styleId << fieldSeparator
           << static_cast<int>(generation.recipe.purpose) << fieldSeparator
           << static_cast<int>(generation.recipe.landscape) << fieldSeparator
           << generation.recipe.seed << fieldSeparator
           << generation.recipe.settlementCount << fieldSeparator
           << generation.recipe.landmarkCount << fieldSeparator
           << generation.recipe.edgeConnectionCount << fieldSeparator
           << generation.graph.schemaVersion << fieldSeparator
           << generation.recipe.styleVersion << fieldSeparator
           << static_cast<int>(generation.recipe.scale) << fieldSeparator
           << static_cast<int>(generation.recipe.quality) << fieldSeparator
           << generation.recipe.styleSnapshotId << fieldSeparator
           << static_cast<int>(generation.recipe.creationIntent) << fieldSeparator
           << static_cast<int>(generation.recipe.sizePreset) << fieldSeparator
           << generation.recipe.contentLanguage << fieldSeparator
           << generation.recipe.nameCulture << fieldSeparator
           << (generation.recipe.allowAdditionalNations ? 1 : 0) << fieldSeparator
           << generation.recipe.recipeVersion << fieldSeparator
           << mapLandformToken(generation.recipe.landform) << fieldSeparator
           << generation.recipe.waterLevel << fieldSeparator << generation.recipe.mountainDensity << fieldSeparator
           << generation.recipe.forestDensity << fieldSeparator << generation.recipe.settlementDensity << fieldSeparator
           << generation.recipe.routeDensity << fieldSeparator << generation.recipe.tacticalDensity << fieldSeparator
           << generation.recipe.travelScenePurpose << fieldSeparator << generation.recipe.travelSceneBiome << fieldSeparator
           << generation.recipe.travelSceneDangerLevel << fieldSeparator << (generation.recipe.travelSceneCombatReady ? 1 : 0) << fieldSeparator
           << generation.recipe.travelSceneSeed << "\n";

    const auto writeRecipeValues = [&](const char *recordType, const std::vector<std::string> &values) {
        for (const std::string &value : values)
            stream << recordType << fieldSeparator << value << "\n";
    };
    writeRecipeValues("K", generation.recipe.knownFacts);
    writeRecipeValues("R", generation.recipe.requiredElements);
    writeRecipeValues("O", generation.recipe.optionalElements);
    writeRecipeValues("X", generation.recipe.excludedElements);
    writeRecipeValues("L", generation.recipe.lockedFeatureIds);

    for (const MapGenerationConstraint &constraint : generation.recipe.constraints)
    {
        stream << "C" << fieldSeparator << constraint.id << fieldSeparator << constraint.label << fieldSeparator
               << static_cast<int>(constraint.featureKind) << fieldSeparator << constraint.x << fieldSeparator << constraint.y << fieldSeparator
               << (constraint.required ? 1 : 0) << fieldSeparator << (constraint.locked ? 1 : 0) << "\n";
    }
    for (const MapGenerationFact &fact : generation.recipe.facts)
    {
        stream << "Q" << fieldSeparator << fact.id << fieldSeparator << static_cast<int>(fact.type) << fieldSeparator
               << static_cast<int>(fact.strength) << fieldSeparator << fact.label << fieldSeparator << fact.value << fieldSeparator
               << fact.sourceText << fieldSeparator << fact.confidence << fieldSeparator << (fact.confirmed ? 1 : 0);
        for (const std::string &entityRef : fact.entityRefs)
            stream << fieldSeparator << entityRef;
        stream << "\n";
    }
    for (const MapGeneratedFeature &feature : generation.graph.features)
    {
        stream << "F" << fieldSeparator << feature.id << fieldSeparator << static_cast<int>(feature.kind) << fieldSeparator
               << feature.name << fieldSeparator << feature.materialId << fieldSeparator << feature.width << fieldSeparator
               << (feature.closed ? 1 : 0) << fieldSeparator << (feature.locked ? 1 : 0) << fieldSeparator;
        for (const MapFeaturePoint &point : feature.points)
            stream << point.x << "," << point.y << pointSeparator;
        stream << fieldSeparator << feature.lineage.sourceMapId << fieldSeparator << feature.lineage.sourceFeatureId
               << fieldSeparator << feature.lineage.sourceRevision << fieldSeparator << (feature.discovered ? 1 : 0)
               << fieldSeparator << (feature.gmOnly ? 1 : 0) << "\n";
    }
    return stream.str();
}

MapGenerationMetadata deserializeMapGenerationMetadata(const std::string &serializedGeneration)
{
    constexpr char fieldSeparator = '\x1f';
    MapGenerationMetadata generation;
    if (serializedGeneration.empty())
        return generation;

    auto splitFields = [&](const std::string &line) {
        std::vector<std::string> fields;
        std::stringstream fieldStream(line);
        std::string field;
        while (std::getline(fieldStream, field, fieldSeparator))
            fields.push_back(field);
        return fields;
    };

    try
    {
        std::stringstream stream(serializedGeneration);
        std::string line;
        while (std::getline(stream, line))
        {
            const std::vector<std::string> fields = splitFields(line);
            if (fields.empty())
                continue;
            if (fields[0] == "M" && fields.size() >= 13)
            {
                generation.enabled = std::stoi(fields[1]) != 0;
                generation.continuousRendering = std::stoi(fields[2]) != 0;
                generation.recipe.generatorVersion = std::max(1, std::stoi(fields[3]));
                generation.recipe.recipeId = fields[4];
                generation.recipe.styleId = fields[5];
                generation.recipe.purpose = static_cast<MapGenerationPurpose>(std::clamp(std::stoi(fields[6]), 0, 3));
                generation.recipe.landscape = static_cast<MapLandscapeProfile>(std::clamp(std::stoi(fields[7]), 0, 4));
                generation.recipe.seed = static_cast<std::uint32_t>(std::stoul(fields[8]));
                generation.recipe.settlementCount = std::max(0, std::stoi(fields[9]));
                generation.recipe.landmarkCount = std::max(0, std::stoi(fields[10]));
                generation.recipe.edgeConnectionCount = std::max(0, std::stoi(fields[11]));
                generation.graph.schemaVersion = std::max(1, std::stoi(fields[12]));
                if (fields.size() > 13 && !fields[13].empty())
                    generation.recipe.styleVersion = std::max(1, std::stoi(fields[13]));
                if (fields.size() > 14 && !fields[14].empty())
                    generation.recipe.scale = static_cast<MapLevel>(std::clamp(std::stoi(fields[14]), 0, 3));
                if (fields.size() > 15 && !fields[15].empty())
                    generation.recipe.quality = static_cast<MapGenerationQualityLevel>(std::clamp(std::stoi(fields[15]), 0, 2));
                if (fields.size() > 16 && !fields[16].empty())
                    generation.recipe.styleSnapshotId = fields[16];
                if (fields.size() > 17 && !fields[17].empty())
                    generation.recipe.creationIntent = static_cast<MapCreationIntent>(std::clamp(std::stoi(fields[17]), 0, 7));
                if (fields.size() > 18 && !fields[18].empty())
                    generation.recipe.sizePreset = static_cast<MapSizePreset>(std::clamp(std::stoi(fields[18]), 0, 3));
                if (fields.size() > 19 && !fields[19].empty())
                    generation.recipe.contentLanguage = fields[19];
                if (fields.size() > 20 && !fields[20].empty())
                    generation.recipe.nameCulture = fields[20];
                if (fields.size() > 21 && !fields[21].empty())
                    generation.recipe.allowAdditionalNations = std::stoi(fields[21]) != 0;
                if (fields.size() > 22 && !fields[22].empty())
                    generation.recipe.recipeVersion = std::max(1, std::stoi(fields[22]));
                if (fields.size() > 23)
                    generation.recipe.landform = mapLandformFromToken(fields[23]);
                if (fields.size() > 29)
                {
                    generation.recipe.waterLevel = std::clamp(std::stoi(fields[24]), -1, 100);
                    generation.recipe.mountainDensity = std::clamp(std::stoi(fields[25]), -1, 100);
                    generation.recipe.forestDensity = std::clamp(std::stoi(fields[26]), -1, 100);
                    generation.recipe.settlementDensity = std::clamp(std::stoi(fields[27]), -1, 100);
                    generation.recipe.routeDensity = std::clamp(std::stoi(fields[28]), -1, 100);
                    generation.recipe.tacticalDensity = std::clamp(std::stoi(fields[29]), -1, 100);
                }
                if (fields.size() > 34)
                {
                    generation.recipe.travelScenePurpose = std::clamp(std::stoi(fields[30]), -1, 4);
                    generation.recipe.travelSceneBiome = fields[31];
                    generation.recipe.travelSceneDangerLevel = std::clamp(std::stoi(fields[32]), 0, 5);
                    generation.recipe.travelSceneCombatReady = std::stoi(fields[33]) != 0;
                    generation.recipe.travelSceneSeed = static_cast<std::uint32_t>(std::stoul(fields[34]));
                }
                if (fields.size() <= 17)
                {
                    const MapKind legacyKind = generation.recipe.scale == MapLevel::Tactical ? MapKind::Encounter :
                                               generation.recipe.purpose == MapGenerationPurpose::SettlementEnvirons ? MapKind::Settlement :
                                               MapKind::Geographic;
                    generation.recipe.creationIntent = mapCreationIntentFromLegacy(generation.recipe.scale, legacyKind, MapScope::World);
                }
            }
            else if ((fields[0] == "K" || fields[0] == "R" || fields[0] == "O" || fields[0] == "X" || fields[0] == "L") && fields.size() >= 2)
            {
                std::vector<std::string> *target = nullptr;
                if (fields[0] == "K") target = &generation.recipe.knownFacts;
                else if (fields[0] == "R") target = &generation.recipe.requiredElements;
                else if (fields[0] == "O") target = &generation.recipe.optionalElements;
                else if (fields[0] == "X") target = &generation.recipe.excludedElements;
                else target = &generation.recipe.lockedFeatureIds;
                if (!fields[1].empty())
                    target->push_back(fields[1]);
            }
            else if (fields[0] == "C" && fields.size() >= 8)
            {
                MapGenerationConstraint constraint;
                constraint.id = fields[1];
                constraint.label = fields[2];
                constraint.featureKind = static_cast<MapGeneratedFeatureKind>(std::clamp(std::stoi(fields[3]), 0, 14));
                constraint.x = std::stof(fields[4]);
                constraint.y = std::stof(fields[5]);
                constraint.required = std::stoi(fields[6]) != 0;
                constraint.locked = std::stoi(fields[7]) != 0;
                generation.recipe.constraints.push_back(std::move(constraint));
            }
            else if (fields[0] == "Q" && fields.size() >= 9)
            {
                MapGenerationFact fact;
                fact.id = fields[1];
                fact.type = static_cast<MapGenerationFactType>(std::clamp(std::stoi(fields[2]), 0, 16));
                fact.strength = static_cast<MapGenerationFactStrength>(std::clamp(std::stoi(fields[3]), 0, 2));
                fact.label = fields[4];
                fact.value = fields[5];
                fact.sourceText = fields[6];
                fact.confidence = std::clamp(std::stof(fields[7]), 0.0f, 1.0f);
                fact.confirmed = std::stoi(fields[8]) != 0;
                // Structured map references are positional: entity, optional
                // nation, generated feature. Dropping an empty nation slot
                // makes the feature ID look like a missing campaign nation.
                for (std::size_t index = 9; index < fields.size(); ++index)
                    fact.entityRefs.push_back(fields[index]);
                // getline omits the final empty token; preserve it as well so
                // the serializer round-trips every reference position exactly.
                if (!line.empty() && line.back() == fieldSeparator)
                    fact.entityRefs.emplace_back();
                if (!fact.id.empty())
                    generation.recipe.facts.push_back(std::move(fact));
            }
            else if (fields[0] == "F" && fields.size() >= 9)
            {
                MapGeneratedFeature feature;
                feature.id = fields[1];
                feature.kind = static_cast<MapGeneratedFeatureKind>(std::clamp(std::stoi(fields[2]), 0, 14));
                feature.name = fields[3];
                feature.materialId = fields[4];
                feature.width = std::max(0.1f, std::stof(fields[5]));
                feature.closed = std::stoi(fields[6]) != 0;
                feature.locked = std::stoi(fields[7]) != 0;
                std::stringstream points(fields[8]);
                std::string pointValue;
                while (std::getline(points, pointValue, ';'))
                {
                    const std::size_t comma = pointValue.find(',');
                    if (comma == std::string::npos)
                        continue;
                    feature.points.push_back({std::stof(pointValue.substr(0, comma)), std::stof(pointValue.substr(comma + 1))});
                }
                if (fields.size() > 9)
                    feature.lineage.sourceMapId = fields[9];
                if (fields.size() > 10)
                    feature.lineage.sourceFeatureId = fields[10];
                if (fields.size() > 11 && !fields[11].empty())
                    feature.lineage.sourceRevision = static_cast<std::uint64_t>(std::stoull(fields[11]));
                if (fields.size() > 12 && !fields[12].empty())
                    feature.discovered = std::stoi(fields[12]) != 0;
                if (fields.size() > 13 && !fields[13].empty())
                    feature.gmOnly = std::stoi(fields[13]) != 0;
                if (!feature.id.empty() && !feature.points.empty())
                    generation.graph.features.push_back(std::move(feature));
            }
        }
    }
    catch (const std::exception &)
    {
        return MapGenerationMetadata{};
    }
    return generation;
}
}
