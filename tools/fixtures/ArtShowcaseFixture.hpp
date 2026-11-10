#pragma once

#include "DeterministicSessionFixture.hpp"
#include "Models/Map/CurrentMapArtRecipe.hpp"
#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/TacticalMapGenerator.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include "Models/Session/PlayerDisplayReviewMapPolicy.hpp"
#include <stdexcept>

namespace gm::qa
{
// A disposable variant of the interaction fixture. The normal privacy and
// session tests retain their small diagnostic maps; media QA opts into this
// richer map explicitly and still exercises the real review/arm/publish UI.
inline Campaign artShowcaseCampaign(const std::string &kind,bool german=false)
{
    Campaign campaign=threeSceneCampaign(german);
    MapModel map;
    if(kind=="tactical" || kind=="forest" || kind=="river" || kind=="ruins" || kind=="city" || kind=="cave" || kind=="dungeon")
    {
        TacticalMapRecipe recipe;
        recipe.generatorVersion=12;recipe.illustrationRecipeVersion=kCurrentMapIllustrationRecipe;
        recipe.width=kind=="tactical"?24:32;recipe.height=24;
        recipe.seed=kind=="tactical"?464468523u:612345u;
        if(kind=="forest")recipe.mapTemplate=TacticalMapTemplate::ForestRoad;
        if(kind=="river")recipe.mapTemplate=TacticalMapTemplate::RiverCrossing;
        if(kind=="ruins")recipe.mapTemplate=TacticalMapTemplate::Ruin;
        if(kind=="city")recipe.mapTemplate=TacticalMapTemplate::CitySquare;
        if(kind=="cave")recipe.mapTemplate=TacticalMapTemplate::Cave;
        if(kind=="dungeon")recipe.mapTemplate=TacticalMapTemplate::StoneDungeon;
        recipe.coverDensity=18;
        const auto candidates=TacticalMapGenerator::generate(recipe,nullptr,1);
        if(candidates.empty() || candidates.front().quality.criticalIssueCount>0 || !candidates.front().quality.criticalRouteConnected)
            throw std::runtime_error("Art showcase tactical generation was not accepted");
        map=candidates.front().map;
    }
    else
    {
        const bool world=kind=="world";
        if(!world && kind!="region")throw std::runtime_error("Unknown art showcase kind");
        MapGenerationOptions options;
        options.generatorVersion=world?11:12;options.recipeVersion=kCurrentMapIllustrationRecipe;
        options.styleId="illustrated_atlas";
        options.scope=world?MapScope::World:MapScope::Local;
        options.scale=world?MapLevel::World:MapLevel::Region;
        options.creationIntent=world?MapCreationIntent::WorldOverview:MapCreationIntent::RegionExploration;
        options.width=40;options.height=30;options.mountainDensity=40;
        options.seed=world?2609465013u:1038803534u;
        options.qualityAttempts=12;options.contentLanguage=german?"de":"en";
        const auto batch=generateSemanticMapCandidateBatch(options,1);
        if(batch.candidates.empty() || !batch.candidates.front().validation.eligible)
            throw std::runtime_error("Art showcase atlas generation was not accepted");
        map=batch.candidates.front().map;
    }
    const auto previous=campaign.maps.front();
    // Keep generated presentation metadata (including the tactical template).
    // Only transplant the stable fixture identity used by the story links.
    map.metadata.id=previous.metadata.id;
    map.metadata.createdAtUtc=previous.metadata.createdAtUtc;
    map.metadata.updatedAtUtc=previous.metadata.updatedAtUtc;
    map.name=previous.name;map.revision=1;
    map.visibility=MapVisibility::PlayersVisible;
    map.layers=previous.layers;map.tokens=previous.tokens;
    map.playerRelease.waysAndRoutes=true;
    map.playerRelease.placesAndMarkers=true;
    map.playerRelease.labels=true;
    // Hidden token remains present even when its category is released.
    map.playerRelease.tokensAndAnnotations=true;
    for(auto &cell:map.cells)cell.fogged=false;
    // The showcase deliberately publishes its generated places. World-map
    // publication also requires a matching public campaign entity; a category
    // checkbox alone must never bypass that consent.
    for(auto &feature:map.generation.graph.features)
    {
        if(feature.gmOnly)continue;
        feature.discovered=true;
        if(map.atlasLevel==MapLevel::World &&
           (feature.kind==MapGeneratedFeatureKind::Settlement || feature.kind==MapGeneratedFeatureKind::Landmark))
        {
            WorldPlaceModel place;
            identify(place.metadata,"art-place-"+feature.id);
            place.name=feature.name;
            place.linkedMapId=map.metadata.id;
            place.linkedFeatureId=feature.id;
            place.visibility=WorldObjectVisibility::Public;
            campaign.worldPlaces.push_back(std::move(place));
        }
    }
    // These are optional travel/objective overlays, not the physical roads
    // and sanctuary art. Keep them in the fixture but leave them private.
    for(auto &route:map.routes)route.discovered=false;
    for(auto &region:map.regions)region.gmOnly=true;
    for(auto &feature:map.features)feature.gmOnly=true;
    MapGeneratedFeature secret;
    secret.id="showcase-private-landmark";secret.kind=MapGeneratedFeatureKind::Landmark;
    secret.name=secretMarker;secret.points={{3.f,3.f}};secret.gmOnly=true;secret.discovered=true;
    map.generation.graph.features.push_back(secret);
    MapRenderPlanRequest request;request.audience=MapRenderAudience::Player;
    request.locale=german?"de":"en";
    request.additionalGeneratedFeatureVisibility=[&](const MapGeneratedFeature &feature) {
        return PlayerDisplayReview::generatedFeatureIsPublic(campaign,map,feature);
    };
    const auto plan=compileMapRenderPlan(map,request);
    if(std::find(plan.generatedFeatureIds.begin(),plan.generatedFeatureIds.end(),secret.id)!=plan.generatedFeatureIds.end() ||
       !plan.tokenIds.empty() || !plan.routeIds.empty() || !plan.regionIds.empty() ||
       (plan.layers&MapRenderPlanWays)==0 || plan.atlasInk.empty() ||
       (map.atlasLevel==MapLevel::World && std::none_of(plan.primitives.begin(),plan.primitives.end(),
           [](const auto &primitive){return primitive.kind==MapRenderPrimitiveKind::Label;})))
        throw std::runtime_error("Art showcase did not preserve the player privacy contract");
    campaign.maps.front()=std::move(map);
    campaign.description=german?"Generierte Kunstvorschau; keine persönlichen Kampagnendaten.":
        "Generated art review fixture; no personal campaign data.";
    return campaign;
}
} // namespace gm::qa
