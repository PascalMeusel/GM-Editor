#pragma once

#include "Models/Map/MapRuntimeBridgeVisualAssets.hpp"
#include "Models/Map/MapRuntimeStructureVisualAssets.hpp"

inline MapStyleContextualAssetRequest mapStyleContextualAssetRequestForWayVisual(const MapWayRenderContext &context, const std::string &styleId = defaultMapStyleId())
{
    MapStyleContextualAssetRequest request;
    request.styleId = styleId;
    request.tileX = context.tileX;
    request.tileY = context.tileY;

    if (!mapWayKindIsRoadLike(context.wayKind) && !mapWayKindIsWaterLike(context.wayKind))
        return request;

    if (mapWayKindIsRoadLike(context.wayKind) && mapStyleWayTerrainCanHostBridgeApproach(context.terrainKind))
    {
        const MapWayNeighborInfo *bridgeNeighbor = mapStyleBridgeApproachNeighbor(context);
        const std::string approachPosition = mapStyleBridgeApproachPositionKey(context);
        const std::string bridgeOwner = bridgeNeighbor
                                            ? mapStyleBridgeSequenceOwnerKey(context.wayKind, bridgeNeighbor->terrainKind)
                                            : std::string();
        if (bridgeNeighbor && !bridgeOwner.empty() && !approachPosition.empty())
        {
            request.assetClass = MapStyleContextualAssetClass::BridgeSequence;
            request.owner = bridgeOwner;
            request.semanticContext =
                "bridge_family=" + mapStyleBridgeFamilyKey(request.owner) +
                "; crossing=" + mapStyleBridgeCrossingKey(request.owner) +
                "; approach_way=" + mapStyleWayFamilyKey(context.wayKind) +
                "; approach_ground=" + mapStyleWayGroundFamilyKey(context.wayKind, context.terrainKind) +
                "; mask=" + std::to_string(mapWayCardinalMask(context)) +
                "; span_position=" + approachPosition;
            return request;
        }
    }

    if (mapWayRendersAsBridge(context))
    {
        request.assetClass = MapStyleContextualAssetClass::BridgeSequence;
        request.owner = mapStyleBridgeSequenceOwnerKey(context.wayKind, context.terrainKind);
        request.semanticContext =
            "bridge_family=" + mapStyleBridgeFamilyKey(request.owner) +
            "; crossing=" + mapStyleBridgeCrossingKey(request.owner) +
            "; approach_way=" + mapStyleWayFamilyKey(context.wayKind) +
            "; mask=" + std::to_string(mapWayCardinalMask(context));
        const std::string spanPosition = mapStyleBridgeSpanPositionKey(context);
        if (!spanPosition.empty())
            request.semanticContext += "; span_position=" + spanPosition;
        return request;
    }

    request.assetClass = MapStyleContextualAssetClass::IntegratedWayGround;
    request.owner = mapStyleIntegratedWayOwnerKey(context.wayKind, context.terrainKind);
    request.semanticContext =
        "way_family=" + std::string(mapStyleWayFamilyKey(context.wayKind)) +
        "; ground=" + mapStyleWayGroundFamilyKey(context.wayKind, context.terrainKind) +
        "; mask=" + std::to_string(mapWayEightNeighborMask(context));
    return request;
}

inline MapStyleContextualAssetRequest mapStyleContextualAssetRequestForTileVisual(const MapTileRenderContext &context, const std::string &styleId = defaultMapStyleId())
{
    MapStyleContextualAssetRequest request;
    request.styleId = styleId;
    request.tileX = context.tileX;
    request.tileY = context.tileY;

    const std::string componentOwner = mapStyleComponentRoleOwnerKey(context.kind, context.symbolVariantMode);
    if (!componentOwner.empty())
    {
        request.assetClass = MapStyleContextualAssetClass::ComponentRoleSet;
        request.owner = componentOwner;
        request.semanticContext =
            "component_family=" + componentOwner +
            "; variant_mode=" + mapStyleSymbolVariantModeKey(context.symbolVariantMode) +
            "; same_family_mask=" + std::to_string(connectedMapTileMask(context));
        const std::string componentRole = mapStyleComponentRoleKey(context);
        if (!componentRole.empty())
            request.semanticContext += "; component_role=" + componentRole;
        return request;
    }

    const std::string structureOwner = mapStyleStructureContextOwnerKey(context.kind);
    if (!structureOwner.empty())
    {
        request.assetClass = MapStyleContextualAssetClass::StructureContextMatrix;
        request.owner = structureOwner;
        const MapStyleStructureEntranceContext entrance = mapStyleStructureEntranceContext(context);
        request.semanticContext =
            "structure_family=" + structureOwner +
            "; ground=" + mapStyleStructureGroundKey(context) +
            "; entrance=" + entrance.entrance +
            "; orientation=" + entrance.orientation;
        return request;
    }

    return request;
}

inline MapStyleContextualAssetResolution resolveMapStyleContextualAssetForWayVisual(const MapWayRenderContext &context, const std::string &styleId = defaultMapStyleId())
{
    if (context.wayKind == MapWayKind::None || (!mapWayKindIsRoadLike(context.wayKind) && !mapWayKindIsWaterLike(context.wayKind)))
        return {};
    return resolveMapStyleContextualAsset(mapStyleContextualAssetRequestForWayVisual(context, styleId));
}

inline MapStyleContextualAssetResolution resolveMapStyleContextualAssetForTileVisual(const MapTileRenderContext &context, const std::string &styleId = defaultMapStyleId())
{
    const MapStyleContextualAssetRequest request = mapStyleContextualAssetRequestForTileVisual(context, styleId);
    if (request.owner.empty())
        return {};
    return resolveMapStyleContextualAsset(request);
}

inline std::vector<MapStyleContextualAssetResolution> resolveMapStyleContextualAssetsForCellVisual(const MapModel &map, int tileX, int tileY, const std::string &styleId = defaultMapStyleId())
{
    std::vector<MapStyleContextualAssetResolution> resolutions;

    const MapTileRenderContext tileContext = makeMapTileRenderContext(map, tileX, tileY);
    MapStyleContextualAssetResolution tileResolution = resolveMapStyleContextualAssetForTileVisual(tileContext, styleId);
    if (!tileResolution.selectedAssetId.empty() || !tileResolution.workOrderId.empty())
        resolutions.push_back(std::move(tileResolution));

    const MapWayRenderContext wayContext = makeMapWayRenderContext(map, tileX, tileY);
    MapStyleContextualAssetResolution wayResolution = resolveMapStyleContextualAssetForWayVisual(wayContext, styleId);
    if (!wayResolution.selectedAssetId.empty() || !wayResolution.workOrderId.empty())
        resolutions.push_back(std::move(wayResolution));

    return resolutions;
}
