#pragma once

#include "Models/Session/PlayerDisplayReviewFingerprint.hpp"
#include "Models/Session/PlayerReviewMeasurements.hpp"
#include "Models/Map/MapRenderCompileMemo.hpp"
#include "Models/Session/PlayerReviewMapCompile.hpp"

namespace PlayerDisplayReview
{
using gm::map_render::MapRenderCompileMemo;
inline PlayerDisplayReviewSnapshot build(const Campaign &campaign, const std::string &locale = currentLocaleCode(),
    BuildPurpose purpose = BuildPurpose::Comparison, Measurements *measurements = nullptr, Origin origin = Origin::Unspecified,
    MapRenderCompileMemo *compileMemo = nullptr, MapRenderCompileMemo *gmCompileMemo = nullptr,
    bool cacheOnly = false, MapRenderCompileStatus *compileStatus = nullptr)
{
    Measure buildTime(measurements, Stage::Build, origin);
    PlayerDisplayReviewSnapshot review;
    if(compileStatus)*compileStatus=MapRenderCompileStatus::Hit;
    review.locale = locale;
    review.campaignName = campaign.name;
    const LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    const bool mapRequested = (display.showMap || display.showSharedArt) && !display.publicMapId.empty();
    const MapModel *map = mapRequested ? publicMap(campaign) : nullptr;
    review.hasMapPreview = mapRequested;

    if (mapRequested && map == nullptr)
    {
        review.issue = PlayerDisplayReviewIssue::MapUnavailable;
        return review;
    }
    if (display.mapOnly && (!display.showMap || map == nullptr))
    {
        review.issue = PlayerDisplayReviewIssue::MapOnlyNeedsPublicMap;
        return review;
    }

    if (map != nullptr)
    {
        if (purpose == BuildPurpose::Comparison)
        {
        Measure timing(measurements, Stage::GmCompile, origin);
        const auto gmRequest=gmCompileRequest(*map,locale);
        if(!compileReviewMap(*map,gmRequest,gmCompileMemo?gmCompileMemo:compileMemo,
                             cacheOnly,review.gmMapPlan,compileStatus))
        {review.issue=PlayerDisplayReviewIssue::MapUnavailable;return review;}
        }

        const MapModel playerSource = [&]{ Measure timing(measurements, Stage::PlayerSource, origin);
            return playerSafeMapSource(*map); }();
        auto playerRequest=playerCompileRequest(playerSource,locale);
        playerRequest.additionalGeneratedFeatureVisibility = [&campaign, map](const MapGeneratedFeature &feature) {
            return generatedFeatureIsPublic(campaign, *map, feature);
        };
        { Measure timing(measurements, Stage::PlayerCompile, origin);
          if(!compileReviewMap(playerSource,playerRequest,compileMemo,cacheOnly,review.playerMapPlan,compileStatus))
          {review.issue=PlayerDisplayReviewIssue::MapUnavailable;return review;} }
        if ((purpose == BuildPurpose::Comparison && !mapRenderPlanMatchesMap(review.gmMapPlan, *map)) ||
            !mapRenderPlanMatchesMap(review.playerMapPlan, *map))
        {
            review.issue = PlayerDisplayReviewIssue::MapUnavailable;
            return review;
        }
    }

    bool hasPayload = display.showMap && map != nullptr;
    const PlayerSafeStoryScene scene = buildPlayerSafeStoryScene(campaign);
    if (!display.publicSceneId.empty())
    {
        if (!scene.available)
        {
            review.issue = PlayerDisplayReviewIssue::SceneUnavailable;
            return review;
        }
        review.playerScene = scene;
        hasPayload = true;
    }

    const EncounterModel *encounter = nullptr;
    if (display.showInitiative && !display.publicEncounterId.empty())
    {
        const auto found = std::find_if(campaign.encounters.begin(), campaign.encounters.end(),
                                        [&](const EncounterModel &candidate) { return candidate.metadata.id == display.publicEncounterId; });
        if (found == campaign.encounters.end())
        {
            review.issue = PlayerDisplayReviewIssue::InitiativeUnavailable;
            return review;
        }
        encounter = &*found;
        if (encounter->combatPhase != CombatPhase::WrapUp)
        {
            review.playerEncounterName = encounter->name;
            review.playerEncounterRound = encounter->round;
            review.playerInitiative = buildPlayerSafeInitiative(*encounter);
            hasPayload = true;
        }
    }

    if (display.showVisibleNpcs)
    {
        for (const EntityId &id : display.visibleNpcIds)
        {
            if (id.empty() || std::none_of(campaign.npcs.begin(), campaign.npcs.end(),
                                           [&](const NPCModel &npc) { return npc.metadata.id == id; }))
            {
                review.issue = PlayerDisplayReviewIssue::NpcUnavailable;
                return review;
            }
        }
        review.playerNpcs = buildPlayerSafeNpcSummaries(campaign);
        hasPayload = hasPayload || !display.visibleNpcIds.empty();
    }

    if (display.showParty)
    {
        for (const EntityId &id : display.visibleCharacterIds)
        {
            if (id.empty() || std::none_of(campaign.characters.begin(), campaign.characters.end(),
                                           [&](const CharacterModel &character) { return character.metadata.id == id; }))
            {
                review.issue = PlayerDisplayReviewIssue::PartyUnavailable;
                return review;
            }
        }
        review.playerParty = buildPlayerSafeCharacterSummaries(campaign);
        review.playerInventory = buildPlayerSafeInventorySummaries(campaign);
        hasPayload = hasPayload || !review.playerParty.empty() || !review.playerInventory.empty();
    }

    if (display.showPublicHandouts)
    {
        for (const EntityId &id : display.publicHandoutIds)
        {
            if (id.empty() || std::none_of(campaign.handouts.begin(), campaign.handouts.end(),
                                           [&](const HandoutModel &handout) { return handout.metadata.id == id; }))
            {
                review.issue = PlayerDisplayReviewIssue::HandoutUnavailable;
                return review;
            }
        }
        review.playerHandouts = buildPlayerSafeHandouts(campaign);
        for (PlayerSafeHandout &handout : review.playerHandouts)
        {
            Measure timing(measurements, Stage::HandoutBinding, origin);
            const auto source = std::find_if(
                campaign.handouts.begin(), campaign.handouts.end(),
                [&](const HandoutModel &candidate) {
                    return candidate.metadata.id == handout.assetId;
                });
            if (source == campaign.handouts.end() ||
                !bindManagedHandout(campaign, *source, handout))
            {
                review.playerHandouts.clear();
                review.issue = PlayerDisplayReviewIssue::HandoutUnavailable;
                return review;
            }
        }
        hasPayload = hasPayload || !review.playerHandouts.empty();
    }

    const bool sharedBackground = display.showSharedArt && map != nullptr && review.playerMapPlan.backgroundVisible;
    hasPayload = hasPayload || sharedBackground;
    review.playerObjectives = TabletopToolkit::playerSafeObjectives(campaign);
    review.playerClues = TabletopToolkit::playerSafeClues(campaign);
    review.playerSafetyBoundaries = TabletopToolkit::playerSafeBoundaries(campaign);
    review.playerContentWarnings = TabletopToolkit::playerSafeWarnings(campaign);
    hasPayload = hasPayload || !review.playerObjectives.empty() || !review.playerClues.empty() ||
                 !review.playerSafetyBoundaries.empty() || !review.playerContentWarnings.empty();
    if (!hasPayload)
    {
        review.issue = PlayerDisplayReviewIssue::NoPublicPayload;
        return review;
    }

    std::uint64_t hash = 1469598103934665603ull;
    appendString(hash, locale);
    appendUint64(hash, localPlayerDisplayPayloadFingerprint(display));
    appendString(hash, campaign.name);
    if (map != nullptr)
    {
        appendString(hash, map->name);
        appendString(hash, review.playerMapPlan.mapId);
        appendUint64(hash, review.playerMapPlan.mapRevision);
        appendUint64(hash, review.playerMapPlan.contentHash);
    }
    appendByte(hash, review.playerScene.available ? 1u : 0u);
    if (review.playerScene.available)
    {
        appendString(hash, review.playerScene.title);
        appendString(hash, review.playerScene.playerSummary);
        appendString(hash, review.playerScene.readAloudText);
    }
    if (encounter != nullptr)
    {
        appendString(hash, review.playerEncounterName);
        appendInt(hash, review.playerEncounterRound);
        appendUint64(hash, static_cast<std::uint64_t>(review.playerInitiative.size()));
        for (const PlayerSafeInitiativeEntry &entry : review.playerInitiative)
        {
            appendString(hash, entry.displayName);
            appendInt(hash, entry.initiative);
            appendByte(hash, entry.active ? 1u : 0u);
        }
    }
    appendUint64(hash, static_cast<std::uint64_t>(review.playerNpcs.size()));
    for (const PlayerSafeNpcSummary &npc : review.playerNpcs)
    {
        appendString(hash, npc.name);
        appendString(hash, npc.role);
    }
    appendUint64(hash, static_cast<std::uint64_t>(review.playerParty.size()));
    for (const PlayerSafeCharacterSummary &character : review.playerParty)
    {
        appendString(hash, character.entityId);
        appendString(hash, character.name);
        appendString(hash, character.characterClass);
        appendInt(hash, character.level);
        appendInt(hash, character.currentHitPoints);
        appendInt(hash, character.maxHitPoints);
        appendInt(hash, character.temporaryHitPoints);
        appendInt(hash, character.speedFeet);
        appendUint64(hash, static_cast<std::uint64_t>(character.conditions.size()));
        for (const std::string &condition : character.conditions)
            appendString(hash, condition);
        appendUint64(hash, static_cast<std::uint64_t>(character.spellSlots.size()));
        for (const auto &slot : character.spellSlots)
        { appendInt(hash, slot.level); appendInt(hash, slot.available); appendInt(hash, slot.maximum); }
    }
    appendUint64(hash, static_cast<std::uint64_t>(review.playerInventory.size()));
    for (const PlayerSafeInventoryItemSummary &item : review.playerInventory)
    { appendString(hash, item.ownerName); appendString(hash, item.itemName); appendInt(hash, item.quantity); appendBool(hash, item.partyOwned); }
    if (display.showPublicHandouts)
    {
        for (const PlayerSafeHandout &handout : review.playerHandouts)
        {
            appendString(hash, handout.assetId);
            appendString(hash, handout.title);
            appendString(hash, handout.body);
            appendString(hash, handout.projectRelativePath);
            appendString(hash, handout.contentHash);
        }
    }
    if (display.showSharedArt)
        for (const EntityId &assetId : display.sharedArtAssetIds)
            appendString(hash, assetId);
    for (const PlayerSafeObjective &objective : review.playerObjectives)
    {
        appendString(hash, objective.id);
        appendString(hash, objective.title);
        appendString(hash, objective.summary);
        appendInt(hash, static_cast<int>(objective.status));
    }
    for (const PlayerSafeClue &clue : review.playerClues)
    {
        appendString(hash, clue.id);
        appendString(hash, clue.title);
        appendString(hash, clue.text);
        appendString(hash, clue.discoveredAtUtc);
    }
    for (const PlayerSafeSafetyBoundary &boundary : review.playerSafetyBoundaries)
    {
        appendInt(hash, static_cast<int>(boundary.kind));
        appendString(hash, boundary.topic);
        appendString(hash, boundary.guidance);
    }
    for (const PlayerSafeContentWarning &warning : review.playerContentWarnings)
    {
        appendString(hash, warning.targetEntityId);
        appendString(hash, warning.targetEntityType);
        appendString(hash, warning.targetLabel);
        appendString(hash, warning.warning);
    }

    review.contentFingerprint = hash == 0 ? 1 : hash;
    review.issue = PlayerDisplayReviewIssue::None;
    review.eligible = true;
    return review;
}

} // namespace PlayerDisplayReview
