#pragma once

#include "Models/Map/MapStyleAssetSelection.hpp"

inline std::vector<int> mapStyleIntegratedWayFallbackMasks(int mask)
{
    std::vector<int> masks;
    const int cardinalMask = mask & 0x55;
    mapStyleAppendUniqueMask(masks, cardinalMask);
    if (mapStyleMaskBitCount(cardinalMask) >= 3)
        mapStyleAppendUniqueMask(masks, 0x55);
    return masks;
}

inline bool mapStyleStructureAssetMetadataMatchesRequest(
    const MapStyleContextualAssetMetadata &metadata,
    const std::string &ground,
    const std::string &entrance,
    const std::string &orientation)
{
    if (!ground.empty() && metadata.contextGround != ground)
        return false;
    if (!entrance.empty() && metadata.contextEntranceType != entrance)
        return false;
    if (!orientation.empty() && metadata.contextOrientation != orientation)
        return false;
    return !ground.empty() || !entrance.empty() || !orientation.empty();
}

inline bool mapStyleBridgeAssetMetadataMatchesRequest(
    const MapStyleContextualAssetMetadata &metadata,
    const std::string &spanPosition)
{
    if (spanPosition.empty())
        return false;
    return metadata.spanPosition == spanPosition || metadata.role == spanPosition;
}

inline bool mapStyleComponentRoleAssetMetadataMatchesRequest(
    const MapStyleContextualAssetMetadata &metadata,
    const std::string &componentRole)
{
    if (componentRole.empty())
        return false;
    return metadata.role == componentRole;
}

inline std::vector<std::string> mapStyleAssetIdsForRequestSlot(
    const MapStyleContextualAssetRequest &request,
    const MapStyleContextualAssetCoverage &coverage,
    const std::vector<std::string> &assetIds)
{
    if (request.assetClass == MapStyleContextualAssetClass::IntegratedWayGround)
    {
        const std::string slotToken = mapStyleContextualRequestSlotToken(request);
        if (slotToken.empty())
            return assetIds;

        std::vector<std::string> filtered = mapStyleFilterAssetIdsBySlotToken(assetIds, slotToken);
        if (!filtered.empty())
            return filtered;

        const int mask = mapStyleSemanticContextInt(request.semanticContext, "mask", -1);
        for (int fallbackMask : mapStyleIntegratedWayFallbackMasks(mask))
        {
            filtered = mapStyleFilterAssetIdsBySlotToken(assetIds, mapStyleMaskSlotToken(fallbackMask));
            if (!filtered.empty())
                return filtered;
        }
        return {};
    }

    if (request.assetClass == MapStyleContextualAssetClass::BridgeSequence)
    {
        const std::string spanPosition = mapStyleSemanticContextValue(request.semanticContext, "span_position");
        if (spanPosition.empty())
            return assetIds;

        std::vector<std::string> filtered;
        for (const std::string &assetId : assetIds)
        {
            const auto metadata = coverage.assetMetadataById.find(assetId);
            if (metadata != coverage.assetMetadataById.end() &&
                mapStyleBridgeAssetMetadataMatchesRequest(metadata->second, spanPosition))
                filtered.push_back(assetId);
        }
        return filtered;
    }

    if (request.assetClass == MapStyleContextualAssetClass::ComponentRoleSet)
    {
        const std::string componentRole = mapStyleSemanticContextValue(request.semanticContext, "component_role");
        if (componentRole.empty())
            return assetIds;

        std::vector<std::string> filtered;
        for (const std::string &assetId : assetIds)
        {
            const auto metadata = coverage.assetMetadataById.find(assetId);
            if (metadata != coverage.assetMetadataById.end() &&
                mapStyleComponentRoleAssetMetadataMatchesRequest(metadata->second, componentRole))
                filtered.push_back(assetId);
        }
        return filtered;
    }

    if (request.assetClass == MapStyleContextualAssetClass::StructureContextMatrix)
    {
        const std::string ground = mapStyleSemanticContextValue(request.semanticContext, "ground");
        const std::string entrance = mapStyleSemanticContextValue(request.semanticContext, "entrance");
        const std::string orientation = mapStyleSemanticContextValue(request.semanticContext, "orientation");
        if (ground.empty() && entrance.empty() && orientation.empty())
            return assetIds;

        std::vector<std::string> filtered;
        for (const std::string &assetId : assetIds)
        {
            const auto metadata = coverage.assetMetadataById.find(assetId);
            if (metadata != coverage.assetMetadataById.end() &&
                mapStyleStructureAssetMetadataMatchesRequest(metadata->second, ground, entrance, orientation))
                filtered.push_back(assetId);
        }
        return filtered;
    }

    return assetIds;
}

inline MapStyleContextualAssetResolution resolveMapStyleContextualAsset(const MapStyleContextualAssetRequest &request)
{
    // Contextual slot filtering walks manifest vectors. Map rendering asks for
    // the same semantic cells every frame, so doing that lookup repeatedly is
    // pure overhead. The registry is immutable for the process lifetime; a
    // bounded thread-local cache keeps UI rendering lock-free and deterministic.
    static thread_local std::unordered_map<std::string, MapStyleContextualAssetResolution> resolutionCache;
    const std::string cacheKey = request.styleId + "\x1f" + std::to_string(static_cast<int>(request.assetClass)) + "\x1f" +
                                 request.owner + "\x1f" + request.semanticContext + "\x1f" +
                                 std::to_string(request.tileX) + "\x1f" + std::to_string(request.tileY);
    if (const auto cached = resolutionCache.find(cacheKey); cached != resolutionCache.end())
        return cached->second;
    const auto remember = [&](const MapStyleContextualAssetResolution &value) {
        if (resolutionCache.size() >= 32768)
            resolutionCache.clear();
        resolutionCache.emplace(cacheKey, value);
        return value;
    };

    MapStyleContextualAssetResolution resolution;
    resolution.requestOwner = request.owner;
    resolution.requestSemanticContext = request.semanticContext;
    const MapStyleContextualAssetCoverage *coverage = findMapStyleContextualAssetCoverage(request.styleId, request.assetClass, request.owner);
    if (!findMapStylePack(request.styleId))
    {
        resolution.qaLabel = "style_family_fallback";
        resolution.selectedAssetId = "style_registry:" + request.styleId;
        resolution.fallbackReason = "missing_style_pack";
        return remember(resolution);
    }

    if (!coverage)
    {
        resolution.qaLabel = "style_family_fallback";
        resolution.selectedAssetId = mapStyleFallbackAssetId(request.assetClass);
        resolution.fallbackReason = "missing_contextual_work_order";
        return remember(resolution);
    }

    resolution.workOrderId = coverage->workOrderId;
    resolution.targetAssetCount = coverage->targetAssetCount;
    resolution.acceptedAssetCount = coverage->acceptedAssetCount;
    resolution.candidateAssetCount = coverage->candidateAssetCount;
    const std::vector<std::string> acceptedAssetIds = mapStyleAssetIdsForRequestSlot(request, *coverage, coverage->acceptedAssetIds);
    const std::vector<std::string> candidateAssetIds = mapStyleAssetIdsForRequestSlot(request, *coverage, coverage->candidateAssetIds);
    resolution.hasExactAcceptedAsset = !acceptedAssetIds.empty();
    resolution.hasCandidateAsset = !candidateAssetIds.empty();

    if (resolution.hasExactAcceptedAsset)
    {
        resolution.usedFallback = false;
        resolution.selectedAssetId = acceptedAssetIds.empty()
                                         ? "accepted:" + coverage->workOrderId
                                         : acceptedAssetIds[mapStyleStableAssetIndex(request, acceptedAssetIds)];
        return remember(resolution);
    }

    resolution.usedFallback = true;
    if (resolution.hasCandidateAsset)
        resolution.candidatePreviewAssetId = candidateAssetIds[mapStyleStableAssetIndex(request, candidateAssetIds)];
    resolution.selectedAssetId = mapStyleFallbackAssetId(request.assetClass);
    resolution.fallbackReason = resolution.hasCandidateAsset
                                    ? "candidate_not_runtime_accepted"
                                    : (coverage->candidateAssetCount > 0 && mapStyleRequestUsesSlotFiltering(request)
                                           ? "missing_contextual_asset_slot"
                                           : mapStyleFallbackReason(request.assetClass));
    resolution.qaLabel = coverage->qaLabels.empty() ? mapStylePrimaryQaLabel(request.assetClass) : coverage->qaLabels.front();
    return remember(resolution);
}
