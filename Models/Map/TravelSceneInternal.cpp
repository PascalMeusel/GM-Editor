#include "Models/Map/TravelSceneGenerator.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <sstream>
#include "Models/Map/TravelSceneInternal.hpp"


namespace TravelSceneGenerator::detail
{
std::string trim(std::string value)
{
    const auto useful = [](unsigned char c) { return !std::isspace(c); };
    while (!value.empty() && !useful(static_cast<unsigned char>(value.front())))
        value.erase(value.begin());
    while (!value.empty() && !useful(static_cast<unsigned char>(value.back())))
        value.pop_back();
    return value;
}

std::uint32_t stableHash(const std::string &value)
{
    std::uint32_t hash = 2166136261u;
    for (unsigned char c : value)
    {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

std::string stableIdPart(std::string value)
{
    for (char &c : value)
    {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (!std::isalnum(byte))
            c = '_';
    }
    return value.empty() ? "route" : value;
}

std::string normalizedContentLanguage(std::string value)
{
    value = trim(std::move(value));
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    std::replace(value.begin(), value.end(), '_', '-');
    return value == "de" || value.rfind("de-", 0) == 0 || value == "german" || value == "deutsch"
               ? "de"
               : "en";
}

std::string normalizedNameCulture(const TravelSceneRecipe &recipe)
{
    return recipe.nameCulture.empty() ? "neutral_fantasy" : mapGenNormalizedNameCulture(recipe.nameCulture);
}

bool usesGermanNarrative(const TravelSceneRecipe &recipe)
{
    return normalizedContentLanguage(recipe.contentLanguage) == "de";
}

const char *travelPurposeName(TravelScenePurpose purpose, bool german)
{
    if (german)
    {
        switch (purpose)
        {
        case TravelScenePurpose::Quiet: return "Ruhige Rast";
        case TravelScenePurpose::Discovery: return "Entdeckung";
        case TravelScenePurpose::Social: return "Begegnung";
        case TravelScenePurpose::Obstacle: return "Hindernis";
        case TravelScenePurpose::Danger: return "Gefahr";
        }
    }
    switch (purpose)
    {
    case TravelScenePurpose::Quiet: return "Quiet Rest";
    case TravelScenePurpose::Discovery: return "Discovery";
    case TravelScenePurpose::Social: return "Meeting";
    case TravelScenePurpose::Obstacle: return "Obstacle";
    case TravelScenePurpose::Danger: return "Danger";
    }
    return german ? "Reiseszene" : "Travel Scene";
}

const char *travelSummary(TravelScenePurpose purpose, bool german)
{
    if (german)
    {
        switch (purpose)
        {
        case TravelScenePurpose::Quiet:
            return "Eine ruhige Reiseszene für Rast, Gespräche und Charakterspiel.";
        case TravelScenePurpose::Discovery:
            return "Eine Entdeckung am Reiseweg mit einem sichtbaren Bezug zur Umgebung.";
        case TravelScenePurpose::Social:
            return "Eine soziale Begegnung am Reiseweg mit mehreren möglichen Ausgängen.";
        case TravelScenePurpose::Obstacle:
            return "Ein räumlich lesbares Hindernis zwingt die Gruppe zu einer Entscheidung.";
        case TravelScenePurpose::Danger:
            return "Eine gefährliche Reiseszene, die direkt als Begegnung vorbereitet werden kann.";
        }
    }
    switch (purpose)
    {
    case TravelScenePurpose::Quiet:
        return "A quiet travel scene for rest, conversation, and character moments.";
    case TravelScenePurpose::Discovery:
        return "A discovery along the route with a visible connection to the surroundings.";
    case TravelScenePurpose::Social:
        return "A social encounter along the route with several possible outcomes.";
    case TravelScenePurpose::Obstacle:
        return "A spatially readable obstacle forces the party to make a choice.";
    case TravelScenePurpose::Danger:
        return "A dangerous travel scene that can be prepared directly as an encounter.";
    }
    return german ? "Eine Reiseszene entlang der aktiven Route." : "A travel scene along the active route.";
}

MapLandscapeProfile landscapeFor(const TravelSceneRecipe &recipe)
{
    std::string lower = recipe.biome;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower.find("mount") != std::string::npos || lower.find("berg") != std::string::npos || lower.find("pass") != std::string::npos)
        return MapLandscapeProfile::MountainPass;
    if (lower.find("forest") != std::string::npos || lower.find("wald") != std::string::npos)
        return MapLandscapeProfile::ForestFrontier;
    if (lower.find("lake") != std::string::npos || lower.find("see") != std::string::npos || lower.find("coast") != std::string::npos || lower.find("küste") != std::string::npos)
        return MapLandscapeProfile::LakeCountry;
    if (lower.find("hill") != std::string::npos || lower.find("hochland") != std::string::npos ||
        (recipe.schemaVersion >= 3 && lower.find("highland") != std::string::npos))
        return MapLandscapeProfile::Highlands;
    return MapLandscapeProfile::RiverValley;
}

bool travelRecipeRequestsWater(const TravelSceneRecipe &recipe)
{
    auto mentionsWater = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value.find("river") != std::string::npos ||
               value.find("stream") != std::string::npos ||
               value.find("creek") != std::string::npos ||
               value.find("lake") != std::string::npos ||
               value.find("coast") != std::string::npos ||
               value.find("water") != std::string::npos ||
               value.find("fluss") != std::string::npos ||
               value.find("bach") != std::string::npos ||
               value.find("see") != std::string::npos ||
               value.find("kuste") != std::string::npos ||
               value.find("kueste") != std::string::npos ||
               value.find("wasser") != std::string::npos;
    };
    if (mentionsWater(recipe.biome))
        return true;
    return std::any_of(recipe.requiredElements.begin(), recipe.requiredElements.end(), mentionsWater);
}
} // namespace TravelSceneGenerator::detail

