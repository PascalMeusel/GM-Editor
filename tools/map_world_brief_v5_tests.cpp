#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapWorldBrief.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <vector>

namespace
{
bool require(bool condition, const char *message)
{
    if (!condition) std::cerr << message << '\n';
    return condition;
}

std::vector<const MapGenerationFact *> executableFacts(const MapWorldBrief &brief)
{
    std::vector<const MapGenerationFact *> result;
    for (const MapGenerationFact &fact : brief.facts)
        if (fact.type != MapGenerationFactType::Note)
            result.push_back(&fact);
    return result;
}

const MapGenerationFact *findFact(const MapWorldBrief &brief, const std::string &id)
{
    const auto found = std::find_if(brief.facts.begin(), brief.facts.end(), [&](const MapGenerationFact &fact) {
        return fact.id == id;
    });
    return found == brief.facts.end() ? nullptr : &*found;
}

const MapGenerationFact *findRelation(const MapWorldBrief &brief, const std::string &canonicalValue)
{
    const auto found = std::find_if(brief.facts.begin(), brief.facts.end(), [&](const MapGenerationFact &fact) {
        return fact.type == MapGenerationFactType::RelativePosition && fact.value == canonicalValue;
    });
    return found == brief.facts.end() ? nullptr : &*found;
}

bool featureGraphsEqual(const MapFeatureGraph &left, const MapFeatureGraph &right)
{
    if (left.schemaVersion != right.schemaVersion || left.features.size() != right.features.size())
        return false;
    for (std::size_t index = 0; index < left.features.size(); ++index)
    {
        const MapGeneratedFeature &a = left.features[index];
        const MapGeneratedFeature &b = right.features[index];
        if (a.id != b.id || a.kind != b.kind || a.name != b.name ||
            a.materialId != b.materialId || a.width != b.width ||
            a.closed != b.closed || a.locked != b.locked ||
            a.discovered != b.discovered || a.gmOnly != b.gmOnly ||
            a.points.size() != b.points.size())
            return false;
        for (std::size_t point = 0; point < a.points.size(); ++point)
            if (a.points[point].x != b.points[point].x ||
                a.points[point].y != b.points[point].y)
                return false;
    }
    return true;
}
}

int main()
{
    bool ok = true;
    const MapWorldBrief german = MapWorldBriefInterpreter::interpret(
        "Die Welt muss eine K\xC3\xBCste, eine Insel, ein Meer, einen See, einen Fluss, ein Gebirge, einen Wald und eine Stra\xC3\x9F" "e enthalten.");
    const MapWorldBrief english = MapWorldBriefInterpreter::interpret(
        "The world must contain a coast, an island, a sea, a lake, a river, a mountain range, a forest, and a road.");
    const auto germanFacts = executableFacts(german);
    const auto englishFacts = executableFacts(english);
    ok &= require(germanFacts.size() == 8 && englishFacts.size() == germanFacts.size(),
                  "DE/EN vocabulary did not produce eight facts");
    for (std::size_t index = 0; index < std::min(germanFacts.size(), englishFacts.size()); ++index)
        ok &= require(germanFacts[index]->id == englishFacts[index]->id &&
                      germanFacts[index]->type == englishFacts[index]->type &&
                      germanFacts[index]->value == englishFacts[index]->value &&
                      germanFacts[index]->strength == MapGenerationFactStrength::Must &&
                      germanFacts[index]->strength == englishFacts[index]->strength &&
                      germanFacts[index]->confirmed && germanFacts[index]->confidence >= 0.9f,
                      "DE/EN canonical fact parity failed");

    const MapWorldBrief modifiersDe = MapWorldBriefInterpreter::interpret(
        "Ein Wald w\xC3\xA4re sch\xC3\xB6n. Ohne Stra\xC3\x9F" "en.");
    const MapWorldBrief modifiersEn = MapWorldBriefInterpreter::interpret(
        "A forest would be nice. Without roads.");
    for (const MapWorldBrief *brief : {&modifiersDe, &modifiersEn})
    {
        const MapGenerationFact *forest = findFact(*brief, "brief_feature_forest");
        const MapGenerationFact *road = findFact(*brief, "brief_exclude_road");
        ok &= require(forest && forest->strength == MapGenerationFactStrength::Prefer,
                      "Wish was not parsed as Prefer");
        ok &= require(road && road->type == MapGenerationFactType::ExcludedElement &&
                      road->strength == MapGenerationFactStrength::Must,
                      "Negative was not parsed as a required exclusion");
    }

    // Regression for the built-in Forest Frontier idea.  Older builds
    // converted the plural subject plus landscape into the invalid confirmed
    // relation {Liora, "a vast forest"}, so the application's own suggestion
    // could never reach generation.
    const MapWorldBrief legacyForestIdeaDe = MapWorldBriefInterpreter::interpret(
        "Zwei Nationen namens Arken und Liora grenzen an einen gewaltigen Wald. "
        "Es gibt eine Hauptstadt namens Greenwatch und St\xC3\xA4" "dte namens Harrowfield und Oldmere.");
    const MapWorldBrief legacyForestIdeaEn = MapWorldBriefInterpreter::interpret(
        "Two nations named Arken and Liora border a vast forest. "
        "There is a capital named Greenwatch and cities named Harrowfield and Oldmere.");
    for (const MapWorldBrief *brief : {&legacyForestIdeaDe, &legacyForestIdeaEn})
    {
        const MapGenerationFact *forest = findFact(*brief, "brief_feature_forest");
        const auto note = std::find_if(brief->facts.begin(), brief->facts.end(), [](const MapGenerationFact &fact) {
            return fact.type == MapGenerationFactType::Note;
        });
        ok &= require(brief->statedNationCount == 2 &&
                          brief->nationNames == std::vector<std::string>({"Arken", "Liora"}) &&
                          brief->capitalName == "Greenwatch" &&
                          brief->cityNames == std::vector<std::string>({"Harrowfield", "Oldmere"}),
                      "Forest Frontier idea did not preserve all named world facts");
        ok &= require(forest && forest->strength == MapGenerationFactStrength::Must &&
                          std::none_of(brief->facts.begin(), brief->facts.end(), [](const MapGenerationFact &fact) {
                              return fact.type == MapGenerationFactType::RelativePosition;
                          }),
                      "Landscape prose became an invalid executable relative-position fact");
        ok &= require(note != brief->facts.end() &&
                          note->strength == MapGenerationFactStrength::Automatic &&
                          !note->confirmed,
                      "Unsupported landscape nuance was not retained as non-blocking review context");

        MapGenerationOptions options;
        options.generatorVersion = 5;
        options.creationIntent = MapCreationIntent::WorldOverview;
        options.scale = MapLevel::World;
        options.scope = MapScope::World;
        options.width = 72;
        options.height = 48;
        options.requestedSettlements = 3;
        options.requestedLandmarks = 0;
        options.qualityAttempts = 12;
        options.seed = 513100u;
        options.facts = brief->facts;
        mapGenApplyFactsToOptions(options);
        ok &= require(validateMapGenerationRequest(options).eligible,
                      "Forest Frontier idea still blocks before generation");
    }

    const MapWorldBrief currentForestIdeaDe = MapWorldBriefInterpreter::interpret(
        "Zwei Nationen namens Arken und Liora. Ein gewaltiger Wald pr\xC3\xA4gt ihre Grenzlande. "
        "Es gibt eine Hauptstadt namens Greenwatch und St\xC3\xA4" "dte namens Harrowfield und Oldmere.");
    const MapWorldBrief currentForestIdeaEn = MapWorldBriefInterpreter::interpret(
        "Two nations named Arken and Liora. A vast forest shapes their borderlands. "
        "There is a capital named Greenwatch and cities named Harrowfield and Oldmere.");
    for (const MapWorldBrief *brief : {&currentForestIdeaDe, &currentForestIdeaEn})
    {
        MapGenerationOptions options;
        options.generatorVersion = 5;
        options.creationIntent = MapCreationIntent::WorldOverview;
        options.scale = MapLevel::World;
        options.scope = MapScope::World;
        options.width = 72;
        options.height = 48;
        options.requestedSettlements = 3;
        options.requestedLandmarks = 0;
        options.qualityAttempts = 12;
        options.seed = 513101u;
        options.facts = brief->facts;
        mapGenApplyFactsToOptions(options);
        const MapGenerationBatchResult batch = generateSemanticMapCandidateBatch(options, 1);
        ok &= require(brief->unrecognizedText.empty() &&
                          std::none_of(brief->facts.begin(), brief->facts.end(), [](const MapGenerationFact &fact) {
                              return fact.type == MapGenerationFactType::Note ||
                                     fact.type == MapGenerationFactType::RelativePosition;
                          }) &&
                          batch.succeeded() && batch.requestValidation.eligible,
                      "Current Forest Frontier idea did not produce a usable candidate");
    }

    const MapWorldBrief pluralSubject = MapWorldBriefInterpreter::interpret(
        "Three nations named Arken, Liora, and Beloria. Arken and Liora border Beloria.");
    ok &= require(std::none_of(pluralSubject.facts.begin(), pluralSubject.facts.end(),
                              [](const MapGenerationFact &fact) {
                                  return fact.type == MapGenerationFactType::RelativePosition;
                              }) &&
                      std::any_of(pluralSubject.facts.begin(), pluralSubject.facts.end(),
                                  [](const MapGenerationFact &fact) {
                                      return fact.type == MapGenerationFactType::Note &&
                                             fact.strength == MapGenerationFactStrength::Must;
                                  }),
                  "A plural relationship silently discarded one of its subjects");

    const MapWorldBrief unknown = MapWorldBriefInterpreter::interpret(
        "A dragon constellation must govern the seasons.");
    ok &= require(unknown.unrecognizedText.size() == 1 && unknown.facts.size() == 1 &&
                  unknown.facts.front().type == MapGenerationFactType::Note &&
                  unknown.facts.front().strength == MapGenerationFactStrength::Must &&
                  !unknown.facts.front().confirmed && unknown.facts.front().confidence == 0.0f,
                  "Unknown required text was not preserved fail-closed");
    MapGenerationOptions unknownOptions;
    unknownOptions.generatorVersion = 5;
    unknownOptions.facts = unknown.facts;
    const MapGenerationBatchResult unknownBatch = generateSemanticMapCandidateBatch(unknownOptions, 1);
    ok &= require(unknownBatch.failure == MapGenerationBatchFailureCode::InvalidRequest &&
                  unknownBatch.attemptsEvaluated == 0,
                  "Unknown required text did not fail before generation");

    const MapWorldBrief contradiction = MapWorldBriefInterpreter::interpret(
        "The world must contain a river. No rivers.");
    MapGenerationOptions contradictionOptions;
    contradictionOptions.generatorVersion = 5;
    contradictionOptions.facts = contradiction.facts;
    const MapGenerationBatchResult contradictionBatch = generateSemanticMapCandidateBatch(contradictionOptions, 1);
    ok &= require(contradictionBatch.failure == MapGenerationBatchFailureCode::InvalidRequest &&
                  contradictionBatch.attemptsEvaluated == 0,
                  "Typed include/exclude contradiction did not fail before generation");

    const MapWorldBrief relationsDe = MapWorldBriefInterpreter::interpret(
        "Drei Nationen namens Arven, Beloria und Cyndor. Eine Hauptstadt namens Typo. "
        "Arven liegt n\xC3\xB6rdlich von Beloria. Beloria liegt s\xC3\xBC" "dlich von Arven. "
        "Cyndor liegt \xC3\xB6stlich von Arven. Beloria liegt westlich von Cyndor. "
        "Cyndor grenzt an Beloria. "
        "Typo liegt zwischen Arven und Cyndor.");
    const MapWorldBrief relationsEn = MapWorldBriefInterpreter::interpret(
        "Three nations named Arven, Beloria, and Cyndor. A capital named Typo. "
        "Arven is north of Beloria. Beloria is south of Arven. "
        "Cyndor is east of Arven. Beloria is west of Cyndor. Cyndor borders Beloria. "
        "Typo lies between Arven and Cyndor.");
    const std::array<std::pair<const char *, std::vector<std::string>>, 6> expectedRelations = {{
        {"north_of", {"Arven", "Beloria"}},
        {"south_of", {"Beloria", "Arven"}},
        {"east_of", {"Cyndor", "Arven"}},
        {"west_of", {"Beloria", "Cyndor"}},
        {"borders", {"Cyndor", "Beloria"}},
        {"between", {"Typo", "Arven", "Cyndor"}}}};
    for (const auto &[relationValue, entityRefs] : expectedRelations)
    {
        const MapGenerationFact *germanFact = findRelation(relationsDe, relationValue);
        const MapGenerationFact *englishFact = findRelation(relationsEn, relationValue);
        ok &= require(germanFact && englishFact && germanFact->id == englishFact->id &&
                          germanFact->strength == MapGenerationFactStrength::Must &&
                          englishFact->strength == MapGenerationFactStrength::Must &&
                          germanFact->entityRefs == entityRefs && englishFact->entityRefs == entityRefs &&
                          germanFact->confirmed && englishFact->confirmed &&
                          germanFact->confidence >= 0.9f && englishFact->confidence >= 0.9f,
                      "DE/EN directional relationship did not normalize to the same typed fact");
    }
    ok &= require(relationsDe.unrecognizedText.empty() && relationsEn.unrecognizedText.empty(),
                  "Fully parsed relationship clauses were also reported as unknown text");

    MapGenerationOptions relationOptions;
    relationOptions.generatorVersion = 5;
    relationOptions.creationIntent = MapCreationIntent::WorldOverview;
    relationOptions.scale = MapLevel::World;
    relationOptions.scope = MapScope::World;
    relationOptions.width = 72;
    relationOptions.height = 48;
    relationOptions.requestedSettlements = 3;
    relationOptions.requestedLandmarks = 0;
    relationOptions.qualityAttempts = 12;
    relationOptions.seed = 512901u;
    relationOptions.facts = relationsDe.facts;
    MapGenerationConstraint briefCapitalPlacement;
    briefCapitalPlacement.id = "brief_capital";
    briefCapitalPlacement.label = "Typo";
    briefCapitalPlacement.featureKind = MapGeneratedFeatureKind::Settlement;
    briefCapitalPlacement.x = relationOptions.width * 0.52f;
    briefCapitalPlacement.y = relationOptions.height * 0.48f;
    briefCapitalPlacement.required = true;
    briefCapitalPlacement.locked = true;
    relationOptions.constraints.push_back(std::move(briefCapitalPlacement));
    const MapGenerationBatchResult relationBatch = generateSemanticMapCandidateBatch(relationOptions, 1);
    ok &= require(relationBatch.succeeded() && relationBatch.failure == MapGenerationBatchFailureCode::None &&
                      relationBatch.recommendedIndex == 0 && relationBatch.attemptsEvaluated == 12,
                  "Executable required relationships did not produce a valid deterministic world candidate");
    const MapGenerationBatchResult repeatedRelationBatch = generateSemanticMapCandidateBatch(relationOptions, 1);
    ok &= require(relationBatch.succeeded() && repeatedRelationBatch.succeeded() &&
                      featureGraphsEqual(relationBatch.candidates.front().map.generation.graph,
                                         repeatedRelationBatch.candidates.front().map.generation.graph),
                  "Relationship-driven world generation was not deterministic for the same seed and request");
    MapGenerationOptions englishRelationOptions = relationOptions;
    englishRelationOptions.facts = relationsEn.facts;
    const MapGenerationBatchResult englishRelationBatch =
        generateSemanticMapCandidateBatch(englishRelationOptions, 1);
    ok &= require(relationBatch.succeeded() && englishRelationBatch.succeeded() &&
                      featureGraphsEqual(relationBatch.candidates.front().map.generation.graph,
                                         englishRelationBatch.candidates.front().map.generation.graph),
                  "Equivalent DE/EN relationships did not generate the same deterministic feature graph");
    for (const auto &[relationValue, entityRefs] : expectedRelations)
    {
        (void)entityRefs;
        const MapGenerationFact *fact = findRelation(relationsDe, relationValue);
        const auto validation = fact == nullptr || relationBatch.candidates.empty()
                                    ? (relationBatch.candidates.empty()
                                           ? relationBatch.requestValidation.facts.end()
                                           : relationBatch.candidates.front().validation.facts.end())
                                    :
            std::find_if(relationBatch.candidates.front().validation.facts.begin(),
                         relationBatch.candidates.front().validation.facts.end(),
                         [&](const MapGenerationFactValidation &entry) { return entry.factId == fact->id; });
        ok &= require(fact && !relationBatch.candidates.empty() &&
                          validation != relationBatch.candidates.front().validation.facts.end() &&
                          validation->fulfillment == MapGenerationFactFulfillment::Fulfilled &&
                          validation->issueCode.empty() && validation->affectedFeatureIds.size() >= 2 &&
                          validation->positions.size() >= 2,
                      "Fulfilled required relationship did not expose stable feature/position evidence");
    }

    MapModel violatedRelationMap;
    violatedRelationMap.width = 40;
    violatedRelationMap.height = 30;
    violatedRelationMap.cells.resize(1200);
    MapGeneratedFeature arven;
    arven.id = "arven";
    arven.kind = MapGeneratedFeatureKind::Settlement;
    arven.name = "Arven";
    arven.points.push_back({20.0f, 24.0f});
    MapGeneratedFeature beloria;
    beloria.id = "beloria";
    beloria.kind = MapGeneratedFeatureKind::Settlement;
    beloria.name = "Beloria";
    beloria.points.push_back({20.0f, 5.0f});
    violatedRelationMap.generation.graph.features = {arven, beloria};
    const MapGenerationFact *northFact = findRelation(relationsDe, "north_of");
    const MapGenerationFactValidation violatedEvidence =
        northFact ? mapGenValidateFactWithEvidence(violatedRelationMap, *northFact)
                  : MapGenerationFactValidation{};
    ok &= require(northFact && violatedEvidence.fulfillment == MapGenerationFactFulfillment::Unsatisfied &&
                      violatedEvidence.issueCode == "fact.must_unsatisfied" &&
                      violatedEvidence.affectedFeatureIds.size() == 2 && violatedEvidence.positions.size() == 2,
                  "A geometrically violated Must relationship was not rejected with participant evidence");
    violatedRelationMap.generation.recipe.generatorVersion = 5;
    if (northFact)
        violatedRelationMap.generation.recipe.facts.push_back(*northFact);
    const MapGenerationValidationReport violatedValidation =
        validateMapGeneration(violatedRelationMap, MapGenerationQualityReport{});
    ok &= require(!violatedValidation.eligible && violatedValidation.criticalIssueCount > 0 &&
                      std::any_of(violatedValidation.blockers.begin(), violatedValidation.blockers.end(),
                                  [](const MapGenerationIssue &issue) {
                                      return issue.code == "fact.must_unsatisfied";
                                  }),
                  "A map violating a required spatial relationship remained an eligible candidate");

    MapGenerationOptions contradictoryRelationOptions = relationOptions;
    contradictoryRelationOptions.qualityAttempts = 3;
    MapGenerationFact contradictoryDirection;
    contradictoryDirection.id = "brief_relation_contradictory_direction";
    contradictoryDirection.type = MapGenerationFactType::RelativePosition;
    contradictoryDirection.strength = MapGenerationFactStrength::Must;
    contradictoryDirection.label = "Arven | Beloria";
    contradictoryDirection.value = "south_of";
    contradictoryDirection.entityRefs = {"Arven", "Beloria"};
    contradictoryDirection.confirmed = true;
    contradictoryRelationOptions.facts.push_back(std::move(contradictoryDirection));
    const MapGenerationBatchResult contradictoryRelationBatch =
        generateSemanticMapCandidateBatch(contradictoryRelationOptions, 1);
    ok &= require(!contradictoryRelationBatch.succeeded() &&
                      contradictoryRelationBatch.failure == MapGenerationBatchFailureCode::NoEligibleCandidate &&
                      contradictoryRelationBatch.attemptsEvaluated == 3 &&
                      contradictoryRelationBatch.candidates.empty() &&
                      !contradictoryRelationBatch.rejectedCandidates.empty() &&
                      std::all_of(contradictoryRelationBatch.rejectedCandidates.begin(),
                                  contradictoryRelationBatch.rejectedCandidates.end(),
                                  [](const MapGenerationResult &candidate) {
                                      return !candidate.validation.eligible &&
                                             std::any_of(candidate.validation.blockers.begin(),
                                                         candidate.validation.blockers.end(),
                                                         [](const MapGenerationIssue &issue) {
                                                             return issue.code == "fact.must_unsatisfied";
                                                         });
                                  }),
                  "Candidates violating contradictory Must directions were offered to the user");

    const MapWorldBrief settlementRelation = MapWorldBriefInterpreter::interpret(
        "St\xC3\xA4" "dte namens Nordwacht und S\xC3\xBC" "dwacht. "
        "Nordwacht liegt n\xC3\xB6rdlich von S\xC3\xBC" "dwacht.");
    MapGenerationOptions settlementRelationOptions;
    settlementRelationOptions.generatorVersion = 5;
    settlementRelationOptions.creationIntent = MapCreationIntent::WorldOverview;
    settlementRelationOptions.scale = MapLevel::World;
    settlementRelationOptions.scope = MapScope::World;
    settlementRelationOptions.width = 72;
    settlementRelationOptions.height = 48;
    settlementRelationOptions.requestedSettlements = 2;
    settlementRelationOptions.requestedLandmarks = 0;
    settlementRelationOptions.qualityAttempts = 12;
    settlementRelationOptions.seed = 512902u;
    settlementRelationOptions.facts = settlementRelation.facts;
    const MapGenerationBatchResult settlementRelationBatch =
        generateSemanticMapCandidateBatch(settlementRelationOptions, 1);
    const MapGenerationFact *settlementNorth = findRelation(settlementRelation, "north_of");
    ok &= require(settlementNorth && settlementNorth->entityRefs ==
                          std::vector<std::string>({"Nordwacht", "S\xC3\xBC" "dwacht"}) &&
                      settlementRelationBatch.succeeded() &&
                      mapGenFactIsSatisfied(settlementRelationBatch.candidates.front().map, *settlementNorth),
                  "A cardinal relationship between named settlements was not executed");

    MapGenerationOptions malformedTypedOptions;
    malformedTypedOptions.generatorVersion = 5;
    MapGenerationFact malformedTyped;
    malformedTyped.id = "malformed_relative_position";
    malformedTyped.type = MapGenerationFactType::RelativePosition;
    malformedTyped.strength = MapGenerationFactStrength::Must;
    malformedTyped.value = "north_of";
    malformedTyped.entityRefs = {"Arven"};
    malformedTypedOptions.facts.push_back(std::move(malformedTyped));
    const MapGenerationBatchResult malformedTypedBatch =
        generateSemanticMapCandidateBatch(malformedTypedOptions, 1);
    ok &= require(malformedTypedBatch.failure == MapGenerationBatchFailureCode::InvalidRequest &&
                      malformedTypedBatch.attemptsEvaluated == 0 &&
                      !malformedTypedBatch.requestValidation.blockers.empty() &&
                      malformedTypedBatch.requestValidation.blockers.front().code ==
                          "request.relative_relation_invalid",
                  "A malformed typed relationship did not fail closed before generation");

    for (const MapGenerationFactStrength strength : {
             MapGenerationFactStrength::Prefer,
             MapGenerationFactStrength::Automatic})
    {
        MapGenerationOptions optionalMalformedOptions;
        optionalMalformedOptions.generatorVersion = 5;
        MapGenerationFact optionalMalformed;
        optionalMalformed.id = strength == MapGenerationFactStrength::Prefer
                                   ? "preferred_malformed_relative_position"
                                   : "automatic_malformed_relative_position";
        optionalMalformed.type = MapGenerationFactType::RelativePosition;
        optionalMalformed.strength = strength;
        optionalMalformed.value = "borders";
        optionalMalformed.entityRefs = {"Liora", "a vast forest"};
        optionalMalformed.confirmed = false;
        optionalMalformedOptions.facts.push_back(std::move(optionalMalformed));
        const MapGenerationValidationReport optionalValidation =
            validateMapGenerationRequest(optionalMalformedOptions);
        ok &= require(optionalValidation.eligible && optionalValidation.blockers.empty() &&
                          !optionalValidation.warnings.empty() &&
                          optionalValidation.warnings.front().code == "request.relative_relation_invalid",
                      "A non-required malformed relationship still blocked generation");
    }

    const MapWorldBrief malformedRelation = MapWorldBriefInterpreter::interpret("Arven liegt n\xC3\xB6rdlich.");
    ok &= require(malformedRelation.facts.size() == 1 &&
                      malformedRelation.facts.front().type == MapGenerationFactType::Note &&
                      malformedRelation.facts.front().strength == MapGenerationFactStrength::Must &&
                      !malformedRelation.facts.front().confirmed && malformedRelation.facts.front().confidence == 0.0f,
                  "An incomplete directional statement was falsely marked as understood");

    MapGenerationResult raster;
    raster.map.width = 3; raster.map.height = 3; raster.map.cells.resize(9);
    raster.map.cells[4].kind = MapTileKind::Forest;
    MapGenerationFact excludedForest;
    excludedForest.id = "brief_exclude_forest";
    excludedForest.type = MapGenerationFactType::ExcludedElement;
    excludedForest.strength = MapGenerationFactStrength::Must;
    excludedForest.label = excludedForest.value = "forest";
    excludedForest.confirmed = true;
    ok &= require(!mapGenFactIsSatisfied(raster.map, excludedForest),
                  "Exclusion ignored raster-only forest semantics");
    MapGenerationOptions exclusionOptions; exclusionOptions.facts.push_back(excludedForest);
    mapGenApplyFactsToOptions(exclusionOptions);
    mapGenApplyExcludedElements(raster, exclusionOptions);
    ok &= require(mapGenFactIsSatisfied(raster.map, excludedForest),
                  "Exclusion did not remove raster-only forest semantics");
    return ok ? 0 : 1;
}
