#pragma once

#include "Controllers/Editor/Widgets/MapCreator/MapCreatorPersistencePolicy.hpp"
#include "Models/Session/PlayerDisplayReviewService.hpp"

// A sanitized source can have its names removed by Labels consent. Resolve a
// feature back to the authored graph before repeating legacy name-based
// campaign checks; the ID lookup must not invent a match for ambiguous IDs.
inline bool mapCreatorPlayerOutputFeatureIsPublic(
    const Campaign &campaign, const MapModel &draft, const MapGeneratedFeature &outputFeature)
{
    const MapGeneratedFeature *original = nullptr;
    for (const auto &feature : draft.generation.graph.features)
        if (feature.id == outputFeature.id)
        {
            if (original)
                return false;
            original = &feature;
        }
    return original && PlayerDisplayReview::generatedFeatureIsPublic(campaign, draft, *original);
}

// Preserve the editable content while projecting current committed sharing
// and campaign-object consent into a disposable player source. Filter the full
// graph before cropping/export: rejected point features are needed to remove
// their legacy raster projections as well as their symbols and names.
inline MapModel makeMapCreatorCampaignPlayerOutputSnapshot(
    const MapModel &draft,
    const MapCreatorPlayerSettingsBaseline &baseline,
    const Campaign &campaign)
{
    const MapModel sharingSource = makeMapCreatorPlayerOutputSnapshot(draft, baseline, campaign.maps);
    MapRenderPlanRequest request;
    request.audience = MapRenderAudience::Player;
    request.layers = MapRenderPlanAll;
    request.applyFog = true;
    request.showPoliticalOverlay = sharingSource.playerRelease.politicalBorders;
    request.additionalGeneratedFeatureVisibility = [&](const MapGeneratedFeature &feature) {
        return PlayerDisplayReview::generatedFeatureIsPublic(campaign, sharingSource, feature);
    };
    return filterMapForRenderPlan(sharingSource, compileMapRenderPlan(sharingSource, request));
}
