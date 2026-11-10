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
std::string serializeMapFeatures(const std::vector<MapFeatureModel> &features)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::stringstream stream;
    for (const MapFeatureModel &feature : features)
    {
        stream << feature.metadata.id << fieldSeparator
               << static_cast<int>(feature.kind) << fieldSeparator
               << feature.name << fieldSeparator
               << feature.description << fieldSeparator
               << feature.gmNotes << fieldSeparator
               << feature.linkedEntityId << fieldSeparator
               << feature.targetMapId << fieldSeparator
               << feature.x << fieldSeparator
               << feature.y << fieldSeparator
               << (feature.discovered ? 1 : 0) << fieldSeparator
               << (feature.gmOnly ? 1 : 0) << fieldSeparator
               << feature.difficultyClass << fieldSeparator
               << feature.metadata.createdAtUtc << fieldSeparator
               << feature.metadata.updatedAtUtc << entrySeparator;
    }
    return stream.str();
}

std::vector<MapFeatureModel> deserializeMapFeatures(const std::string &serializedFeatures)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::vector<MapFeatureModel> features;
    std::stringstream entryStream(serializedFeatures);
    std::string value;
    while (std::getline(entryStream, value, entrySeparator))
    {
        if (value.empty())
            continue;

        std::stringstream fieldStream(value);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(fieldStream, field, fieldSeparator))
            fields.push_back(field);

        MapFeatureModel feature;
        if (!fields.empty() && !fields[0].empty())
            feature.metadata.id = fields[0];
        try
        {
            if (fields.size() > 1)
                feature.kind = static_cast<MapFeatureKind>(std::clamp(std::stoi(fields[1]), 0, static_cast<int>(MapFeatureKind::Pin)));
            if (fields.size() > 7)
                feature.x = std::stoi(fields[7]);
            if (fields.size() > 8)
                feature.y = std::stoi(fields[8]);
            if (fields.size() > 11)
                feature.difficultyClass = std::max(0, std::stoi(fields[11]));
        }
        catch (const std::exception &)
        {
            feature.kind = MapFeatureKind::Pin;
        }
        if (fields.size() > 2)
            feature.name = fields[2];
        if (fields.size() > 3)
            feature.description = fields[3];
        if (fields.size() > 4)
            feature.gmNotes = fields[4];
        if (fields.size() > 5)
            feature.linkedEntityId = fields[5];
        if (fields.size() > 6)
            feature.targetMapId = fields[6];
        if (fields.size() > 9)
            feature.discovered = fields[9] == "1";
        if (fields.size() > 10)
            feature.gmOnly = fields[10] == "1";
        if (fields.size() > 12 && !fields[12].empty())
            feature.metadata.createdAtUtc = fields[12];
        if (fields.size() > 13 && !fields[13].empty())
            feature.metadata.updatedAtUtc = fields[13];
        if (!feature.name.empty())
            features.push_back(feature);
    }
    return features;
}

std::string serializeMapRoutes(const std::vector<MapRouteModel> &routes)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::stringstream stream;
    for (const MapRouteModel &route : routes)
    {
        stream << route.metadata.id << fieldSeparator
               << route.name << fieldSeparator
               << route.fromMapId << fieldSeparator
               << route.toMapId << fieldSeparator
               << route.startX << fieldSeparator
               << route.startY << fieldSeparator
               << route.endX << fieldSeparator
               << route.endY << fieldSeparator
               << route.distanceMiles << fieldSeparator
               << route.terrain << fieldSeparator
               << route.notes << fieldSeparator
               << (route.discovered ? 1 : 0) << fieldSeparator
               << route.metadata.createdAtUtc << fieldSeparator
               << route.metadata.updatedAtUtc << entrySeparator;
    }
    return stream.str();
}

std::vector<MapRouteModel> deserializeMapRoutes(const std::string &serializedRoutes)
{
    constexpr char fieldSeparator = '\x1f';
    constexpr char entrySeparator = '\x1e';

    std::vector<MapRouteModel> routes;
    std::stringstream entryStream(serializedRoutes);
    std::string value;
    while (std::getline(entryStream, value, entrySeparator))
    {
        if (value.empty())
            continue;

        std::stringstream fieldStream(value);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(fieldStream, field, fieldSeparator))
            fields.push_back(field);

        MapRouteModel route;
        if (!fields.empty() && !fields[0].empty())
            route.metadata.id = fields[0];
        if (fields.size() > 1)
            route.name = fields[1];
        if (fields.size() > 2)
            route.fromMapId = fields[2];
        if (fields.size() > 3)
            route.toMapId = fields[3];
        try
        {
            if (fields.size() > 4)
                route.startX = std::stoi(fields[4]);
            if (fields.size() > 5)
                route.startY = std::stoi(fields[5]);
            if (fields.size() > 6)
                route.endX = std::stoi(fields[6]);
            if (fields.size() > 7)
                route.endY = std::stoi(fields[7]);
            if (fields.size() > 8)
                route.distanceMiles = std::stof(fields[8]);
        }
        catch (const std::exception &)
        {
            route.distanceMiles = 0.0f;
        }
        if (fields.size() > 9)
            route.terrain = fields[9];
        if (fields.size() > 10)
            route.notes = fields[10];
        if (fields.size() > 11)
            route.discovered = fields[11] != "0";
        if (fields.size() > 12 && !fields[12].empty())
            route.metadata.createdAtUtc = fields[12];
        if (fields.size() > 13 && !fields[13].empty())
            route.metadata.updatedAtUtc = fields[13];
        if (!route.name.empty())
            routes.push_back(route);
    }
    return routes;
}
}
