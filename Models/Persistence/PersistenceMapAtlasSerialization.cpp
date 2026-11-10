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
std::string serializeMapAtlasMetadata(const MapModel &map)
{
    constexpr char separator = '\x1f';
    std::stringstream stream;
    stream << "A" << separator << 4 << separator
           << static_cast<int>(map.atlasLevel) << separator
           << static_cast<int>(map.mapKind) << separator
           << static_cast<int>(map.atlasStatus) << separator
           << static_cast<int>(map.visibility) << separator
           << map.revision << separator
           << map.atlasPlacement.parentId << separator
           << map.atlasPlacement.parentRect.x << separator
           << map.atlasPlacement.parentRect.y << separator
           << map.atlasPlacement.parentRect.width << separator
           << map.atlasPlacement.parentRect.height << separator
           << map.atlasPlacement.parentRevision << separator
           << map.atlasPlacement.sourceRevision << separator
           << static_cast<int>(map.creationIntent) << separator
           << static_cast<int>(map.sizePreset) << separator
           << (map.spatialReference.calibrated ? 1 : 0) << separator
           << map.spatialReference.extentWidth << separator
           << map.spatialReference.extentHeight << separator
           << static_cast<int>(map.spatialReference.unit) << separator
           << map.spatialReference.originX << separator
           << map.spatialReference.originY << separator
           << map.spatialReference.rotationDegrees << separator
           << (map.importedReference.enabled ? 1 : 0) << separator
           << map.importedReference.assetId << separator
           << map.importedReference.projectRelativePath << separator
           << map.importedReference.sourceFileName << separator
           << map.importedReference.contentHash << separator
           << map.importedReference.sourceWidth << separator
           << map.importedReference.sourceHeight << separator
           << map.importedReference.crop.x << separator
           << map.importedReference.crop.y << separator
           << map.importedReference.crop.width << separator
           << map.importedReference.crop.height << separator
           << map.importedReference.rotationDegrees << separator
           << (map.importedReference.gmOnly ? 1 : 0) << separator
           << (map.importedReference.semanticallyTraced ? 1 : 0) << separator
           << map.importedReference.workingProxyAssetId << separator
           << map.importedReference.workingProxyProjectRelativePath << separator
           << map.importedReference.workingProxyContentHash << separator
           << map.importedReference.workingProxyWidth << separator
           << map.importedReference.workingProxyHeight << separator
           << map.importedReference.exifOrientation << separator
           << (map.importedReference.workingProxyGenerated ? 1 : 0) << separator
           << (map.playerRelease.politicalBorders ? 1 : 0) << separator
           << (map.playerRelease.waysAndRoutes ? 1 : 0) << separator
           << (map.playerRelease.placesAndMarkers ? 1 : 0) << separator
           << (map.playerRelease.labels ? 1 : 0) << separator
           << (map.playerRelease.tokensAndAnnotations ? 1 : 0);
    return stream.str();
}

bool deserializeMapAtlasMetadata(const std::string &serializedAtlas, MapModel &map)
{
    // Publication consent is the one additive field that must not survive a
    // missing, unknown, or corrupt persisted record on a reused model.
    map.playerRelease = {};
    if (serializedAtlas.empty())
        return false;
    constexpr char separator = '\x1f';
    std::vector<std::string> fields;
    std::stringstream stream(serializedAtlas);
    std::string field;
    while (std::getline(stream, field, separator))
        fields.push_back(field);
    try
    {
        if (fields.size() < 14 || fields[0] != "A")
            return false;

        const int atlasVersion = std::max(0, std::stoi(fields[1]));
        if (atlasVersion < 1 || atlasVersion > 4 ||
            (atlasVersion == 4 && fields.size() < 49))
        {
            map.playerRelease = {};
            return false;
        }
        MapLevel atlasLevel = static_cast<MapLevel>(std::clamp(std::stoi(fields[2]), 0, 3));
        MapKind mapKind = static_cast<MapKind>(std::clamp(std::stoi(fields[3]), 0, 5));
        MapAtlasStatus atlasStatus = static_cast<MapAtlasStatus>(std::clamp(std::stoi(fields[4]), 0, 6));
        MapVisibility visibility = static_cast<MapVisibility>(std::clamp(std::stoi(fields[5]), 0, 2));
        std::uint64_t revision = static_cast<std::uint64_t>(std::stoull(fields[6]));
        MapAtlasPlacement atlasPlacement = map.atlasPlacement;
        atlasPlacement.parentId = fields[7];
        atlasPlacement.parentRect.x = std::stof(fields[8]);
        atlasPlacement.parentRect.y = std::stof(fields[9]);
        atlasPlacement.parentRect.width = std::stof(fields[10]);
        atlasPlacement.parentRect.height = std::stof(fields[11]);
        atlasPlacement.parentRevision = static_cast<std::uint64_t>(std::stoull(fields[12]));
        atlasPlacement.sourceRevision = static_cast<std::uint64_t>(std::stoull(fields[13]));
        MapCreationIntent creationIntent = map.creationIntent;
        MapSizePreset sizePreset = map.sizePreset;
        MapSpatialReference spatialReference = map.spatialReference;
        MapImportedReference importedReference = map.importedReference;
        // V1-V3 records did not carry content-category consent.  Migrating
        // those records must never infer publication from MapVisibility.
        MapPlayerReleaseProfile playerRelease;
        if (fields.size() > 14 && !fields[14].empty())
            creationIntent = static_cast<MapCreationIntent>(std::clamp(std::stoi(fields[14]), 0, 7));
        if (fields.size() > 15 && !fields[15].empty())
            sizePreset = static_cast<MapSizePreset>(std::clamp(std::stoi(fields[15]), 0, 3));
        if (fields.size() > 16 && !fields[16].empty())
            spatialReference.calibrated = std::stoi(fields[16]) != 0;
        if (fields.size() > 17 && !fields[17].empty()) spatialReference.extentWidth = std::stof(fields[17]);
        if (fields.size() > 18 && !fields[18].empty()) spatialReference.extentHeight = std::stof(fields[18]);
        if (fields.size() > 19 && !fields[19].empty()) spatialReference.unit = static_cast<MapDistanceUnit>(std::clamp(std::stoi(fields[19]), 0, 4));
        if (fields.size() > 20 && !fields[20].empty()) spatialReference.originX = std::stof(fields[20]);
        if (fields.size() > 21 && !fields[21].empty()) spatialReference.originY = std::stof(fields[21]);
        if (fields.size() > 22 && !fields[22].empty()) spatialReference.rotationDegrees = std::stof(fields[22]);
        if (fields.size() > 23 && !fields[23].empty()) importedReference.enabled = std::stoi(fields[23]) != 0;
        if (fields.size() > 24) importedReference.assetId = fields[24];
        if (fields.size() > 25) importedReference.projectRelativePath = fields[25];
        if (fields.size() > 26) importedReference.sourceFileName = fields[26];
        if (fields.size() > 27) importedReference.contentHash = fields[27];
        if (fields.size() > 28 && !fields[28].empty()) importedReference.sourceWidth = std::max(0, std::stoi(fields[28]));
        if (fields.size() > 29 && !fields[29].empty()) importedReference.sourceHeight = std::max(0, std::stoi(fields[29]));
        if (fields.size() > 30 && !fields[30].empty()) importedReference.crop.x = std::stof(fields[30]);
        if (fields.size() > 31 && !fields[31].empty()) importedReference.crop.y = std::stof(fields[31]);
        if (fields.size() > 32 && !fields[32].empty()) importedReference.crop.width = std::stof(fields[32]);
        if (fields.size() > 33 && !fields[33].empty()) importedReference.crop.height = std::stof(fields[33]);
        if (fields.size() > 34 && !fields[34].empty()) importedReference.rotationDegrees = std::stof(fields[34]);
        if (fields.size() > 35 && !fields[35].empty()) importedReference.gmOnly = std::stoi(fields[35]) != 0;
        if (fields.size() > 36 && !fields[36].empty()) importedReference.semanticallyTraced = std::stoi(fields[36]) != 0;
        if (fields.size() > 37) importedReference.workingProxyAssetId = fields[37];
        if (fields.size() > 38) importedReference.workingProxyProjectRelativePath = fields[38];
        if (fields.size() > 39) importedReference.workingProxyContentHash = fields[39];
        if (fields.size() > 40 && !fields[40].empty()) importedReference.workingProxyWidth = std::max(0, std::stoi(fields[40]));
        if (fields.size() > 41 && !fields[41].empty()) importedReference.workingProxyHeight = std::max(0, std::stoi(fields[41]));
        if (fields.size() > 42 && !fields[42].empty()) importedReference.exifOrientation = std::clamp(std::stoi(fields[42]), 1, 8);
        if (fields.size() > 43 && !fields[43].empty()) importedReference.workingProxyGenerated = std::stoi(fields[43]) != 0;
        if (atlasVersion >= 4)
        {
            const auto consent = [](const std::string &value) {
                if (value == "0") return false;
                if (value == "1") return true;
                throw std::invalid_argument("invalid player release consent");
            };
            playerRelease.politicalBorders = consent(fields[44]);
            playerRelease.waysAndRoutes = consent(fields[45]);
            playerRelease.placesAndMarkers = consent(fields[46]);
            playerRelease.labels = consent(fields[47]);
            playerRelease.tokensAndAnnotations = consent(fields[48]);
        }

        // Commit only after the complete record parsed successfully. A corrupt
        // additive payload must not leave a half-updated legacy map behind.
        map.atlasLevel = atlasLevel;
        map.mapKind = mapKind;
        map.atlasStatus = atlasStatus;
        map.visibility = visibility;
        map.playerRelease = playerRelease;
        map.revision = revision;
        map.atlasPlacement = std::move(atlasPlacement);
        map.creationIntent = creationIntent;
        map.sizePreset = sizePreset;
        map.spatialReference = std::move(spatialReference);
        map.importedReference = std::move(importedReference);
        return true;
    }
    catch (const std::exception &)
    {
        // Corrupt additive atlas metadata must never prevent the legacy map
        // payload from loading. Optional player categories explicitly return
        // to their fail-closed defaults even if the caller reused a model.
        map.playerRelease = {};
        return false;
    }
}

void deserializeMapPersistenceMetadata(const std::string &serializedGeneration,
                                       const std::string &serializedAtlas,
                                       MapModel &map)
{
    constexpr char separator = '\x1f';
    const auto firstRecord = [&](const std::string &serialized, const char *type) {
        std::stringstream lines(serialized);
        std::string line;
        while (std::getline(lines, line))
        {
            std::vector<std::string> fields;
            std::stringstream values(line);
            std::string value;
            while (std::getline(values, value, separator))
                fields.push_back(value);
            if (!fields.empty() && fields.front() == type)
                return fields;
        }
        return std::vector<std::string>{};
    };

    const std::vector<std::string> generationFields = firstRecord(serializedGeneration, "M");
    bool generationRecordValid = false;
    if (generationFields.size() >= 13)
    {
        try
        {
            (void)std::stoi(generationFields[1]);
            (void)std::stoi(generationFields[2]);
            (void)std::stoi(generationFields[3]);
            (void)std::stoi(generationFields[6]);
            (void)std::stoi(generationFields[7]);
            (void)std::stoul(generationFields[8]);
            generationRecordValid = true;
        }
        catch (const std::exception &)
        {
            generationRecordValid = false;
        }
    }
    const bool explicitRecipeIntent = generationRecordValid && generationFields.size() > 17 && !generationFields[17].empty();
    const bool explicitRecipeSizePreset = generationRecordValid && generationFields.size() > 18 && !generationFields[18].empty();
    if (generationRecordValid)
        map.generation = deserializeMapGenerationMetadata(serializedGeneration);

    const std::vector<std::string> atlasFields = firstRecord(serializedAtlas, "A");
    const bool atlasLoaded = deserializeMapAtlasMetadata(serializedAtlas, map);
    const bool explicitMapIntent = atlasLoaded && atlasFields.size() > 14 && !atlasFields[14].empty();
    const bool explicitMapSizePreset = atlasLoaded && atlasFields.size() > 15 && !atlasFields[15].empty();
    if (!atlasLoaded)
    {
        map.atlasLevel = map.scope == MapScope::World ? MapLevel::World : MapLevel::Site;
        map.mapKind = map.scope == MapScope::World ? MapKind::Geographic :
                      (map.scope == MapScope::Dungeon ? MapKind::Dungeon : MapKind::General);
        map.atlasStatus = map.parentMapId.empty() ? MapAtlasStatus::Standalone : MapAtlasStatus::Unpositioned;
    }

    MapCreationIntent intent = map.creationIntent;
    if (!explicitMapIntent)
    {
        if (explicitRecipeIntent)
            intent = map.generation.recipe.creationIntent;
        else
            intent = mapCreationIntentFromLegacyPersistence(
                map.scope, map.mapKind,
                generationRecordValid ? map.generation.recipe.purpose : MapGenerationPurpose::Wilderness,
                map.width, map.height, atlasLoaded, map.atlasLevel);
        map.creationIntent = intent;
        // Missing/legacy atlas classification is an in-memory compatibility
        // view. It is not written until the user performs an ordinary save.
        map.atlasLevel = mapLevelForCreationIntent(intent);
    }

    if (!explicitMapSizePreset)
    {
        map.sizePreset = explicitRecipeSizePreset
                             ? map.generation.recipe.sizePreset
                             : mapSizePresetFromDimensions(map.creationIntent, map.width, map.height);
    }

    if (!explicitRecipeIntent)
    {
        map.generation.recipe.creationIntent = map.creationIntent;
        map.generation.recipe.scale = map.atlasLevel;
    }
    if (!explicitRecipeSizePreset)
        map.generation.recipe.sizePreset = map.sizePreset;
}
}
