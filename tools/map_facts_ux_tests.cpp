#include "Models/Localization/TranslationCatalog.hpp"
#include "Models/Map/MapWorldBrief.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapFactsPresentation.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapImageImportInspectorContract.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <set>
#include <string>

namespace
{
bool require(bool condition, const char *message)
{
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}
}

int main()
{
    bool ok = true;
    const auto natural = MapWorldBriefInterpreter::interpret(
        "Create a world with three nations: Orun, Skeld and Maravia. Havenfall is the coastal capital. Wolfden is a town in the east. A mountain range and a river cross the land. Roads connect the towns.");
    ok &= require(natural.nationNames == std::vector<std::string>{"Orun", "Skeld", "Maravia"}, "Natural nation lists must retain the supplied names");
    ok &= require(natural.capitalName == "Havenfall" && natural.cityNames == std::vector<std::string>{"Wolfden"}, "Natural capital and town sentences must retain names without absorbing geographical qualifiers");
    const auto vague = MapWorldBriefInterpreter::interpret("Three nations share a great river. The capital is near the coast. A town is in the east.");
    const auto desktopBrief = MapWorldBriefInterpreter::interpret(
        "Es gibt drei Nationen: Orun, Maravia und Skeld. Die Hauptstadt heißt Havenfall. "
        "Im Norden liegen Berge. Eine Straße verbindet Havenfall mit Stormport.");
    ok &= require(desktopBrief.capitalName == "Havenfall", "Common German naming grammar lost the requested capital");
    ok &= require(std::any_of(desktopBrief.unrecognizedText.begin(), desktopBrief.unrecognizedText.end(),
        [](const auto &text) { return text.find("Stormport") != std::string::npos; }),
        "An unresolved named road connection must stay visible as a note");
    ok &= require(std::any_of(desktopBrief.unrecognizedText.begin(), desktopBrief.unrecognizedText.end(),
        [](const auto &text) { return text.find("Norden") != std::string::npos; }),
        "An unresolved landscape direction must stay visible as a note");
    ok &= require(vague.nationNames.empty() && vague.capitalName.empty() && vague.cityNames.empty(), "Generic descriptions must not become invented proper names");
    constexpr std::array<MapGenerationFactType, 17> allTypes = {
        MapGenerationFactType::Note, MapGenerationFactType::Nation,
        MapGenerationFactType::NationCount, MapGenerationFactType::Region,
        MapGenerationFactType::Settlement, MapGenerationFactType::Capital,
        MapGenerationFactType::WaterBody, MapGenerationFactType::River,
        MapGenerationFactType::SharedRiver, MapGenerationFactType::Coast,
        MapGenerationFactType::Island, MapGenerationFactType::MountainRange,
        MapGenerationFactType::Forest, MapGenerationFactType::Road,
        MapGenerationFactType::RelativePosition, MapGenerationFactType::IncludedElement,
        MapGenerationFactType::ExcludedElement};
    std::set<std::string> labels;
    for (const MapGenerationFactType type : allTypes)
    {
        const std::string label = MapFactsPresentation::typeLabel(type);
        ok &= require(!label.empty() && label.find("brief_") == std::string::npos,
                      "Fact type leaked an empty or internal label");
        labels.insert(label);
    }
    ok &= require(labels.size() == allTypes.size(), "Fact types do not have distinct human labels");

    MapGenerationFact namedNation;
    namedNation.type = MapGenerationFactType::Nation;
    namedNation.label = "Alderreach";
    ok &= require(MapFactsPresentation::displayValue(namedNation) == "Alderreach" &&
                      !MapFactsPresentation::displayValueIsUiLiteral(namedNation),
                  "User-authored names must remain verbatim");
    MapGenerationFact forest;
    forest.type = MapGenerationFactType::Forest;
    forest.label = forest.value = "forest";
    ok &= require(MapFactsPresentation::displayValue(forest) == "Forest" &&
                      MapFactsPresentation::displayValueIsUiLiteral(forest),
                  "Semantic fact did not receive a localizable display value");

    const MapWorldBrief unknown = MapWorldBriefInterpreter::interpret(
        "A dragon constellation must govern the seasons.");
    ok &= require(unknown.facts.size() == 1 &&
                      MapFactsPresentation::needsReview(unknown.facts.front()) &&
                      std::string(MapFactsPresentation::recognitionLabel(unknown.facts.front())) == "Not understood",
                  "Unrecognized prose is not visibly marked for review");
    MapGenerationOptions unknownOptions;
    unknownOptions.generatorVersion = 5;
    unknownOptions.facts = unknown.facts;
    mapGenApplyFactsToOptions(unknownOptions);
    const MapGenerationValidationReport blocked = validateMapGenerationRequest(unknownOptions);
    ok &= require(!blocked.eligible && MapFactsPresentation::hasBlockingUnrecognizedNote(blocked),
                  "Required unrecognized prose did not block generation");
    ok &= require(MapFactsPresentation::validationForFact(blocked, unknown.facts.front()) != nullptr,
                  "Fact-level validation evidence was not linked to its card");

    unknownOptions.facts.front().strength = MapGenerationFactStrength::Automatic;
    unknownOptions.facts.front().confirmed = true;
    const MapGenerationValidationReport optional = validateMapGenerationRequest(unknownOptions);
    ok &= require(optional.eligible, "Optional contextual note still blocked generation");

    const gm::mapui::MapImageCropMarginsPercent margins =
        gm::mapui::mapImageCropMarginsPercent(0.10f, 0.20f, 0.60f, 0.50f);
    const auto closeTo = [](float lhs, float rhs) {
        return std::abs(lhs - rhs) < 0.001f;
    };
    ok &= require(closeTo(margins.left, 10.0f) && closeTo(margins.right, 30.0f) &&
                      closeTo(margins.top, 20.0f) && closeTo(margins.bottom, 30.0f) &&
                      closeTo(margins.visibleWidth(), 60.0f) &&
                      closeTo(margins.visibleHeight(), 50.0f),
                  "Crop margins are not presented as understandable percentages");
    const MapNormalizedRect reconstructedCrop =
        gm::mapui::mapImageCropFromMarginsPercent(margins);
    ok &= require(closeTo(reconstructedCrop.x, 0.10f) &&
                      closeTo(reconstructedCrop.y, 0.20f) &&
                      closeTo(reconstructedCrop.width, 0.60f) &&
                      closeTo(reconstructedCrop.height, 0.50f),
                  "Percentage crop inspector did not preserve normalized geometry");

    float fullX = 0.2f;
    float fullY = 0.3f;
    float fullWidth = 0.4f;
    float fullHeight = 0.5f;
    gm::mapui::mapImageInspectorUseWholeImage(fullX, fullY, fullWidth, fullHeight);
    ok &= require(fullX == 0.0f && fullY == 0.0f &&
                      fullWidth == 1.0f && fullHeight == 1.0f,
                  "Whole-image action did not restore every edge");
    ok &= require(gm::mapui::mapImageInspectorRotateQuarterTurn(0.0f, -1) == -90.0f &&
                      gm::mapui::mapImageInspectorRotateQuarterTurn(0.0f, 1) == 90.0f &&
                      gm::mapui::mapImageInspectorRotateQuarterTurn(180.0f, 1) == -90.0f,
                  "Quarter-turn image actions are not deterministic");
    ok &= require(gm::mapui::mapImageInspectorCropIsValid(0.1f, 0.1f, 0.8f, 0.8f) &&
                      !gm::mapui::mapImageInspectorCropIsValid(0.9f, 0.0f, 0.2f, 1.0f),
                  "Crop inspector validation accepted out-of-bounds geometry");
    ok &= require(gm::mapui::mapImageInspectorCalibrationIsValid(false, 0.0f, 0.0f, 0) &&
                      gm::mapui::mapImageInspectorCalibrationIsValid(true, 120.0f, 80.0f,
                          static_cast<int>(MapDistanceUnit::Kilometers)) &&
                      !gm::mapui::mapImageInspectorCalibrationIsValid(true, 0.0f, 80.0f,
                          static_cast<int>(MapDistanceUnit::Kilometers)) &&
                      !gm::mapui::mapImageInspectorCalibrationIsValid(true, 120.0f, 80.0f,
                          static_cast<int>(MapDistanceUnit::Unspecified)),
                  "Optional distance calibration is not gated by dimensions and unit");
    MapImportedReference unchangedReference;
    MapSpatialReference unchangedSpatial;
    ok &= require(!gm::mapui::mapImageInspectorHasPendingChanges(
                      unchangedReference, unchangedSpatial,
                      0.0f, 0.0f, 1.0f, 1.0f, 0.0f, false,
                      0.0f, 0.0f, static_cast<int>(MapDistanceUnit::Unspecified)) &&
                      gm::mapui::mapImageInspectorHasPendingChanges(
                          unchangedReference, unchangedSpatial,
                          0.0f, 0.0f, 0.9f, 1.0f, 0.0f, false,
                          0.0f, 0.0f, static_cast<int>(MapDistanceUnit::Unspecified)),
                  "Preview/apply state does not distinguish stored and draft values");
    ok &= require(gm::mapui::mapImageInspectorFieldColumnCount(480.0f, 18.0f) == 1 &&
                      gm::mapui::mapImageInspectorFieldColumnCount(720.0f, 18.0f) == 2 &&
                      gm::mapui::mapImageInspectorFieldColumnCount(720.0f, 30.0f) == 1,
                  "Import inspector does not stack fields responsively");

    const MapWorldBrief conflict = MapWorldBriefInterpreter::interpret(
        "The world must contain a river. No rivers.");
    MapGenerationOptions conflictOptions;
    conflictOptions.generatorVersion = 5;
    conflictOptions.facts = conflict.facts;
    mapGenApplyFactsToOptions(conflictOptions);
    const MapGenerationValidationReport conflictReport = validateMapGenerationRequest(conflictOptions);
    ok &= require(!conflictReport.eligible && !conflictReport.blockers.empty() &&
                      conflictReport.blockers.front().code == "request.include_exclude_conflict",
                  "Typed include/exclude conflict was not exposed before generation");

    using gm::localization::Locale;
    using gm::localization::uiLiteralTranslation;
    constexpr std::array<const char *, 24> localizedFactsCopy = {
        "Interpreted facts", "Unrecognized note", "Number of nations", "Fact priority",
        "Prefer", "Not understood", "Check interpretation", "Clearly recognized",
        "From your note", "Generation check", "Resolve before generating",
        "Edit original notes", "1. Choose the visible area", "Whole image",
        "Left margin", "Right margin", "Top margin", "Bottom margin",
        "Crop preview", "2. Orient the image", "Turn left 90 degrees",
        "Turn right 90 degrees", "3. Optional distance scale", "Use real-world size"};
    for (const char *key : localizedFactsCopy)
        ok &= require(uiLiteralTranslation(Locale::German, key) != key,
                      "German facts-step localization is missing");

    return ok ? 0 : 1;
}
