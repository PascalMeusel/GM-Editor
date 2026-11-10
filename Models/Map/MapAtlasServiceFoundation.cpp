#include "Models/Map/MapAtlasService.hpp"
#include "Models/Map/MapAuthoring.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include "Models/Map/MapAtlasServiceInternal.hpp"

namespace MapAtlasServiceInternal
{
MapLookup buildLookup(const std::vector<MapModel> &maps)
{
    MapLookup lookup;
    for (const MapModel &map : maps)
    {
        if (!map.metadata.id.empty())
            lookup.emplace(map.metadata.id, &map);
    }
    return lookup;
}

bool pointInside(const MapNormalizedRect &rect, MapAtlasPoint point)
{
    return point.x >= rect.x - atlasEpsilon && point.y >= rect.y - atlasEpsilon &&
           point.x <= rect.x + rect.width + atlasEpsilon &&
           point.y <= rect.y + rect.height + atlasEpsilon;
}

MapAtlasPoint featurePointToNormalized(const MapModel &map, const MapFeaturePoint &point)
{
    return {
        point.x / static_cast<float>(std::max(1, map.width)),
        point.y / static_cast<float>(std::max(1, map.height))};
}

MapFeaturePoint normalizedToFeaturePoint(MapAtlasPoint point, int width, int height)
{
    return {
        point.x * static_cast<float>(std::max(1, width)),
        point.y * static_cast<float>(std::max(1, height))};
}

bool clipSegmentToRect(MapAtlasPoint start, MapAtlasPoint end, const MapNormalizedRect &rect, MapAtlasPoint &clippedStart, MapAtlasPoint &clippedEnd)
{
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    float minimum = 0.0f;
    float maximum = 1.0f;
    const float p[] = {-dx, dx, -dy, dy};
    const float q[] = {
        start.x - rect.x,
        rect.x + rect.width - start.x,
        start.y - rect.y,
        rect.y + rect.height - start.y};
    for (int index = 0; index < 4; ++index)
    {
        if (std::abs(p[index]) <= atlasEpsilon)
        {
            if (q[index] < 0.0f)
                return false;
            continue;
        }
        const float ratio = q[index] / p[index];
        if (p[index] < 0.0f)
            minimum = std::max(minimum, ratio);
        else
            maximum = std::min(maximum, ratio);
        if (minimum > maximum)
            return false;
    }
    clippedStart = {start.x + minimum * dx, start.y + minimum * dy};
    clippedEnd = {start.x + maximum * dx, start.y + maximum * dy};
    return true;
}

std::optional<MapAtlasBoundaryEdge> boundaryEdge(const MapNormalizedRect &rect, MapAtlasPoint point)
{
    if (std::abs(point.y - rect.y) <= atlasEpsilon)
        return MapAtlasBoundaryEdge::North;
    if (std::abs(point.x - (rect.x + rect.width)) <= atlasEpsilon)
        return MapAtlasBoundaryEdge::East;
    if (std::abs(point.y - (rect.y + rect.height)) <= atlasEpsilon)
        return MapAtlasBoundaryEdge::South;
    if (std::abs(point.x - rect.x) <= atlasEpsilon)
        return MapAtlasBoundaryEdge::West;
    return std::nullopt;
}

std::string edgeSuffix(MapAtlasBoundaryEdge edge)
{
    switch (edge)
    {
    case MapAtlasBoundaryEdge::North:
        return "north";
    case MapAtlasBoundaryEdge::East:
        return "east";
    case MapAtlasBoundaryEdge::South:
        return "south";
    case MapAtlasBoundaryEdge::West:
        return "west";
    }
    return "edge";
}

bool samePoint(const MapFeaturePoint &left, const MapFeaturePoint &right)
{
    return std::abs(left.x - right.x) <= atlasEpsilon && std::abs(left.y - right.y) <= atlasEpsilon;
}

bool sameFeature(const MapGeneratedFeature &left, const MapGeneratedFeature &right)
{
    if (left.kind != right.kind || left.name != right.name || left.materialId != right.materialId ||
        std::abs(left.width - right.width) > atlasEpsilon || left.closed != right.closed ||
        left.discovered != right.discovered || left.gmOnly != right.gmOnly ||
        left.points.size() != right.points.size())
        return false;
    for (std::size_t index = 0; index < left.points.size(); ++index)
    {
        if (!samePoint(left.points[index], right.points[index]))
            return false;
    }
    return true;
}

bool inheritedFrom(const MapGeneratedFeature &feature, const MapModel &parent)
{
    return feature.lineage.sourceMapId == parent.metadata.id && !feature.lineage.sourceFeatureId.empty();
}

constexpr const char *atlasSourceMapProperty = "atlas.source_map_id";
constexpr const char *atlasSourceEntityProperty = "atlas.source_entity_id";
constexpr const char *atlasSourceRevisionProperty = "atlas.source_revision";
constexpr const char *atlasSourceKindProperty = "atlas.source_kind";

void markAtlasDerived(EntityMetadata &metadata,
                      const MapModel &parent,
                      const EntityId &sourceId,
                      const char *kind)
{
    metadata.customProperties[atlasSourceMapProperty] = parent.metadata.id;
    metadata.customProperties[atlasSourceEntityProperty] = sourceId;
    metadata.customProperties[atlasSourceRevisionProperty] = std::to_string(parent.revision);
    metadata.customProperties[atlasSourceKindProperty] = kind;
}

bool metadataInheritedFrom(const EntityMetadata &metadata,
                           const MapModel &parent,
                           const char *kind)
{
    const auto sourceMap = metadata.customProperties.find(atlasSourceMapProperty);
    const auto sourceKind = metadata.customProperties.find(atlasSourceKindProperty);
    return sourceMap != metadata.customProperties.end() && sourceMap->second == parent.metadata.id &&
           sourceKind != metadata.customProperties.end() && sourceKind->second == kind;
}

std::string atlasSourceEntityId(const EntityMetadata &metadata)
{
    const auto source = metadata.customProperties.find(atlasSourceEntityProperty);
    return source == metadata.customProperties.end() ? std::string{} : source->second;
}

std::map<std::string, std::string> authoredCustomProperties(const EntityMetadata &metadata)
{
    std::map<std::string, std::string> properties = metadata.customProperties;
    properties.erase(atlasSourceMapProperty);
    properties.erase(atlasSourceEntityProperty);
    properties.erase(atlasSourceRevisionProperty);
    properties.erase(atlasSourceKindProperty);
    return properties;
}

bool sameAuthoredMetadata(const EntityMetadata &left, const EntityMetadata &right)
{
    // Creation/update timestamps and atlas bookkeeping are synchronization
    // metadata, not local authoring decisions. Tags and every other custom
    // property are user content and must participate in conflict detection.
    return left.id == right.id && left.tags == right.tags &&
           authoredCustomProperties(left) == authoredCustomProperties(right);
}

bool sameRegion(const MapRegionModel &left, const MapRegionModel &right)
{
    return sameAuthoredMetadata(left.metadata, right.metadata) &&
           left.name == right.name && left.description == right.description &&
           left.x == right.x && left.y == right.y && left.width == right.width &&
           left.height == right.height && left.movementCost == right.movementCost &&
           left.lighting == right.lighting &&
           left.difficultTerrain == right.difficultTerrain &&
           left.blocksLineOfSight == right.blocksLineOfSight &&
           left.gmOnly == right.gmOnly;
}

bool sameRoute(const MapRouteModel &left, const MapRouteModel &right)
{
    return sameAuthoredMetadata(left.metadata, right.metadata) &&
           left.name == right.name && left.fromMapId == right.fromMapId &&
           left.toMapId == right.toMapId && left.startX == right.startX &&
           left.startY == right.startY && left.endX == right.endX &&
           left.endY == right.endY &&
           std::abs(left.distanceMiles - right.distanceMiles) <= atlasEpsilon &&
           left.terrain == right.terrain && left.notes == right.notes &&
           left.discovered == right.discovered;
}

void detachAtlasLineage(EntityMetadata &metadata)
{
    metadata.customProperties.erase(atlasSourceMapProperty);
    metadata.customProperties.erase(atlasSourceEntityProperty);
    metadata.customProperties.erase(atlasSourceRevisionProperty);
    metadata.customProperties.erase(atlasSourceKindProperty);
}

const char *rebaseItemKindName(MapAtlasRebaseItemKind kind)
{
    switch (kind)
    {
    case MapAtlasRebaseItemKind::Feature: return "feature";
    case MapAtlasRebaseItemKind::Region: return "region";
    case MapAtlasRebaseItemKind::Route: return "route";
    }
    return "item";
}

std::string rebaseItemId(MapAtlasRebaseItemKind kind, const std::string &entityId)
{
    return std::string(rebaseItemKindName(kind)) + "::" + entityId;
}

void hashBytes(std::uint64_t &hash, const void *data, std::size_t size)
{
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t index = 0; index < size; ++index)
    {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }
}

template <typename T>
void hashValue(std::uint64_t &hash, const T &value)
{
    hashBytes(hash, &value, sizeof(value));
}

void hashString(std::uint64_t &hash, const std::string &value)
{
    hashBytes(hash, value.data(), value.size());
    constexpr unsigned char separator = 0xff;
    hashBytes(hash, &separator, sizeof(separator));
}

void hashMetadata(std::uint64_t &hash, const EntityMetadata &metadata)
{
    hashString(hash, metadata.id);
    hashString(hash, metadata.createdAtUtc);
    hashString(hash, metadata.updatedAtUtc);
    for (const std::string &tag : metadata.tags)
        hashString(hash, tag);
    for (const auto &property : metadata.customProperties)
    {
        hashString(hash, property.first);
        hashString(hash, property.second);
    }
}

void hashFeature(std::uint64_t &hash, const MapGeneratedFeature &feature)
{
    hashString(hash, feature.id);
    hashValue(hash, feature.kind);
    hashString(hash, feature.name);
    hashString(hash, feature.materialId);
    hashValue(hash, feature.width);
    hashValue(hash, feature.closed);
    hashValue(hash, feature.locked);
    hashValue(hash, feature.discovered);
    hashValue(hash, feature.gmOnly);
    for (const MapFeaturePoint &point : feature.points)
    {
        hashValue(hash, point.x);
        hashValue(hash, point.y);
    }
    hashString(hash, feature.lineage.sourceMapId);
    hashString(hash, feature.lineage.sourceFeatureId);
    hashValue(hash, feature.lineage.sourceRevision);
}

void hashCell(std::uint64_t &hash, const MapCellModel &cell)
{
    hashValue(hash, cell.kind);
    hashValue(hash, cell.rotationQuarterTurns);
    hashValue(hash, cell.rotationLocked);
    hashValue(hash, cell.symbolVariantMode);
    hashValue(hash, cell.wayKind);
    hashValue(hash, cell.movementCost);
    hashValue(hash, cell.difficultTerrain);
    hashValue(hash, cell.blocksLineOfSight);
    hashValue(hash, cell.lighting);
    hashValue(hash, cell.fogged);
    hashValue(hash, cell.elevation);
    hashValue(hash, cell.movementBlockedMask);
    hashValue(hash, cell.lineOfSightBlockedMask);
    hashValue(hash, cell.movementOneWayBlockedMask);
    hashValue(hash, cell.lineOfSightOneWayBlockedMask);
}

std::uint64_t rebaseContentFingerprint(const MapModel &map)
{
    std::uint64_t hash = 1469598103934665603ull;
    hashString(hash, map.metadata.id);
    hashValue(hash, map.revision);
    hashValue(hash, map.atlasLevel);
    hashValue(hash, map.width);
    hashValue(hash, map.height);
    hashString(hash, map.parentMapId);
    hashString(hash, map.atlasPlacement.parentId);
    hashValue(hash, map.atlasPlacement.parentRect.x);
    hashValue(hash, map.atlasPlacement.parentRect.y);
    hashValue(hash, map.atlasPlacement.parentRect.width);
    hashValue(hash, map.atlasPlacement.parentRect.height);
    hashValue(hash, map.atlasPlacement.parentRevision);
    hashValue(hash, map.atlasPlacement.sourceRevision);
    hashValue(hash, map.generation.graph.schemaVersion);
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
        hashFeature(hash, feature);
    for (const MapCellModel &cell : map.cells)
        hashCell(hash, cell);
    for (const std::string &id : map.generation.recipe.lockedFeatureIds)
        hashString(hash, id);
    for (const MapGenerationConstraint &constraint : map.generation.recipe.constraints)
    {
        hashString(hash, constraint.id);
        hashString(hash, constraint.label);
        hashValue(hash, constraint.featureKind);
        hashValue(hash, constraint.x);
        hashValue(hash, constraint.y);
        hashValue(hash, constraint.required);
        hashValue(hash, constraint.locked);
    }
    for (const MapRegionModel &region : map.regions)
    {
        hashMetadata(hash, region.metadata);
        hashString(hash, region.name);
        hashString(hash, region.description);
        hashValue(hash, region.x);
        hashValue(hash, region.y);
        hashValue(hash, region.width);
        hashValue(hash, region.height);
        hashValue(hash, region.movementCost);
        hashValue(hash, region.lighting);
        hashValue(hash, region.difficultTerrain);
        hashValue(hash, region.blocksLineOfSight);
        hashValue(hash, region.gmOnly);
    }
    for (const MapRouteModel &route : map.routes)
    {
        hashMetadata(hash, route.metadata);
        hashString(hash, route.name);
        hashString(hash, route.fromMapId);
        hashString(hash, route.toMapId);
        hashValue(hash, route.startX);
        hashValue(hash, route.startY);
        hashValue(hash, route.endX);
        hashValue(hash, route.endY);
        hashValue(hash, route.distanceMiles);
        hashString(hash, route.terrain);
        hashString(hash, route.notes);
        hashValue(hash, route.discovered);
    }
    return hash;
}
}
