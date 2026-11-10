#pragma once

#include "Models/Map/MapStyleSnapshotCatalog.hpp"

inline std::size_t mapStyleStableAssetIndex(const MapStyleContextualAssetRequest &request, const std::vector<std::string> &assetIds)
{
    if (assetIds.empty())
        return 0;

    std::uint32_t hash = 2166136261u;
    auto mixByte = [&](unsigned char byte) {
        hash ^= byte;
        hash *= 16777619u;
    };
    auto mixString = [&](const std::string &value) {
        for (unsigned char byte : value)
            mixByte(byte);
        mixByte(0xff);
    };

    mixString(request.styleId);
    mixString(request.owner);
    mixString(request.semanticContext);
    hash ^= static_cast<std::uint32_t>(request.tileX * 73856093);
    hash *= 16777619u;
    hash ^= static_cast<std::uint32_t>(request.tileY * 19349663);
    hash *= 16777619u;
    hash ^= static_cast<std::uint32_t>(request.assetClass);
    return static_cast<std::size_t>(hash % assetIds.size());
}

inline std::string mapStyleTrimSemanticContextPart(const std::string &value)
{
    std::size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first])))
        ++first;

    std::size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1])))
        --last;

    return value.substr(first, last - first);
}

inline std::string mapStyleSemanticContextValue(const std::string &context, const std::string &key)
{
    std::size_t start = 0;
    while (start <= context.size())
    {
        const std::size_t end = context.find(';', start);
        const std::string part = context.substr(start, (end == std::string::npos ? context.size() : end) - start);
        const std::size_t equals = part.find('=');
        if (equals != std::string::npos &&
            mapStyleTrimSemanticContextPart(part.substr(0, equals)) == key)
        {
            return mapStyleTrimSemanticContextPart(part.substr(equals + 1));
        }

        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return {};
}

inline int mapStyleSemanticContextInt(const std::string &context, const std::string &key, int fallback)
{
    const std::string value = mapStyleSemanticContextValue(context, key);
    if (value.empty())
        return fallback;

    int parsed = 0;
    std::size_t index = 0;
    while (index < value.size() && std::isdigit(static_cast<unsigned char>(value[index])))
    {
        parsed = (parsed * 10) + (value[index] - '0');
        ++index;
    }
    return index == 0 ? fallback : parsed;
}

inline std::string mapStyleMaskSlotToken(int mask)
{
    mask = std::clamp(mask, 0, 255);
    const std::string digits = std::to_string(mask);
    return "mask" + std::string(3 - std::min<std::size_t>(3, digits.size()), '0') + digits;
}

inline std::string mapStyleContextualRequestSlotToken(const MapStyleContextualAssetRequest &request)
{
    if (request.assetClass != MapStyleContextualAssetClass::IntegratedWayGround)
        return {};

    const int mask = mapStyleSemanticContextInt(request.semanticContext, "mask", -1);
    return mask < 0 ? std::string() : mapStyleMaskSlotToken(mask);
}

inline bool mapStyleRequestUsesSlotFiltering(const MapStyleContextualAssetRequest &request)
{
    if (request.assetClass == MapStyleContextualAssetClass::IntegratedWayGround)
        return !mapStyleContextualRequestSlotToken(request).empty();

    if (request.assetClass == MapStyleContextualAssetClass::BridgeSequence)
        return !mapStyleSemanticContextValue(request.semanticContext, "span_position").empty();

    if (request.assetClass == MapStyleContextualAssetClass::ComponentRoleSet)
        return !mapStyleSemanticContextValue(request.semanticContext, "component_role").empty();

    if (request.assetClass == MapStyleContextualAssetClass::StructureContextMatrix)
    {
        return !mapStyleSemanticContextValue(request.semanticContext, "ground").empty() ||
               !mapStyleSemanticContextValue(request.semanticContext, "entrance").empty() ||
               !mapStyleSemanticContextValue(request.semanticContext, "orientation").empty();
    }

    return false;
}

inline bool mapStyleAssetIdMatchesSlotToken(const std::string &assetId, const std::string &slotToken)
{
    return assetId.find("." + slotToken + ".") != std::string::npos ||
           assetId.find("__" + slotToken + "__") != std::string::npos;
}

inline std::vector<std::string> mapStyleFilterAssetIdsBySlotToken(const std::vector<std::string> &assetIds, const std::string &slotToken)
{
    std::vector<std::string> filtered;
    for (const std::string &assetId : assetIds)
    {
        if (mapStyleAssetIdMatchesSlotToken(assetId, slotToken))
            filtered.push_back(assetId);
    }
    return filtered;
}

inline int mapStyleMaskBitCount(int mask)
{
    int count = 0;
    while (mask != 0)
    {
        count += mask & 1;
        mask >>= 1;
    }
    return count;
}

inline void mapStyleAppendUniqueMask(std::vector<int> &masks, int mask)
{
    if (mask <= 0)
        return;
    if (std::find(masks.begin(), masks.end(), mask) == masks.end())
        masks.push_back(mask);
}
