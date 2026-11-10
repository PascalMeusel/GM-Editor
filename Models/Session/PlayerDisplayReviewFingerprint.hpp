#pragma once

#include "Models/Session/PlayerDisplayReviewFingerprintSupport.hpp"

namespace PlayerDisplayReview
{
// Cheap, allocation-light invalidation key for the detached display's cached
// review. MapModel::revision remains the ordinary render-content generation,
// while every publication/security input is also hashed explicitly so a
// missed revision bump can only make the display fail closed, never retain a
// stale reveal. Camera and ping deliberately remain outside this key.
inline std::uint64_t sourceFingerprint(const Campaign &campaign,
                                       const std::string &locale = currentLocaleCode(),
                                       bool normalizeLiveCombatStats = false,
                                       bool verifyAssetBytes = true)
{
    std::uint64_t hash = 1469598103934665603ull;
    const LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    appendString(hash, locale);
    appendString(hash, campaign.name);
    appendUint64(hash, localPlayerDisplayPayloadFingerprint(display));

    const bool mapRequested = (display.showMap || display.showSharedArt) &&
                              !display.publicMapId.empty();
    const MapModel *map = mapRequested ? publicMap(campaign) : nullptr;
    appendBool(hash, mapRequested);
    appendBool(hash, map != nullptr);
    if (map != nullptr)
    {
        appendString(hash, map->metadata.id);
        appendString(hash, map->name);
        appendUint64(hash, map->revision);
        appendUint64(hash, static_cast<std::uint64_t>(map->atlasLevel));
        appendUint64(hash, static_cast<std::uint64_t>(map->scope));
        appendUint64(hash, static_cast<std::uint64_t>(map->visibility));
        appendUint64(hash, static_cast<std::uint64_t>(map->gridType));
        appendInt(hash, map->width);
        appendInt(hash, map->height);
        appendString(hash, map->generation.recipe.styleSnapshotId);
        appendString(hash, map->generation.recipe.styleId);
        appendInt(hash, map->generation.recipe.styleVersion);
        appendUint64(hash, map->generation.recipe.seed);
        appendBool(hash, map->generation.continuousRendering);
        appendBool(hash, map->playerRelease.politicalBorders);
        appendBool(hash, map->playerRelease.waysAndRoutes);
        appendBool(hash, map->playerRelease.placesAndMarkers);
        appendBool(hash, map->playerRelease.labels);
        appendBool(hash, map->playerRelease.tokensAndAnnotations);

        appendString(hash, map->backgroundImagePath);
        appendFloat(hash, map->backgroundImageOpacity);
        appendBool(hash, map->importedReference.enabled);
        appendBool(hash, map->importedReference.gmOnly);
        appendString(hash, map->importedReference.assetId);
        appendString(hash, map->importedReference.projectRelativePath);
        appendString(hash, map->importedReference.contentHash);
        appendString(hash, map->importedReference.workingProxyAssetId);
        appendString(hash, map->importedReference.workingProxyProjectRelativePath);
        appendString(hash, map->importedReference.workingProxyContentHash);
        appendBool(hash, map->importedReference.workingProxyGenerated);
        appendFloat(hash, map->importedReference.crop.x);
        appendFloat(hash, map->importedReference.crop.y);
        appendFloat(hash, map->importedReference.crop.width);
        appendFloat(hash, map->importedReference.crop.height);
        appendFloat(hash, map->importedReference.rotationDegrees);

        appendUint64(hash, static_cast<std::uint64_t>(map->cells.size()));
        for (const MapCellModel &cell : map->cells)
        {
            appendUint64(hash, static_cast<std::uint64_t>(cell.kind));
            appendUint64(hash, static_cast<std::uint64_t>(cell.wayKind));
            appendInt(hash, cell.rotationQuarterTurns);
            appendBool(hash, cell.rotationLocked);
            appendUint64(hash, static_cast<std::uint64_t>(cell.symbolVariantMode));
            appendInt(hash, cell.elevation);
            appendInt(hash, cell.movementCost);
            appendBool(hash, cell.difficultTerrain);
            appendBool(hash, cell.blocksLineOfSight);
            appendUint64(hash, static_cast<std::uint64_t>(cell.lighting));
            appendInt(hash, cell.movementBlockedMask);
            appendInt(hash, cell.lineOfSightBlockedMask);
            appendInt(hash, cell.movementOneWayBlockedMask);
            appendInt(hash, cell.lineOfSightOneWayBlockedMask);
            appendBool(hash, cell.fogged);
        }

        appendUint64(hash, static_cast<std::uint64_t>(map->layers.size()));
        for (const MapLayerModel &layer : map->layers)
        {
            appendString(hash, layer.metadata.id);
            appendBool(hash, layer.visible);
        }

        appendUint64(hash, static_cast<std::uint64_t>(map->generation.graph.features.size()));
        for (const MapGeneratedFeature &feature : map->generation.graph.features)
        {
            appendString(hash, feature.id);
            appendUint64(hash, static_cast<std::uint64_t>(feature.kind));
            appendString(hash, feature.name);
            appendString(hash, feature.materialId);
            appendFloat(hash, feature.width);
            appendBool(hash, feature.closed);
            appendBool(hash, feature.locked);
            appendBool(hash, feature.discovered);
            appendBool(hash, feature.gmOnly);
            appendString(hash, feature.lineage.sourceMapId);
            appendString(hash, feature.lineage.sourceFeatureId);
            appendUint64(hash, feature.lineage.sourceRevision);
            appendUint64(hash, static_cast<std::uint64_t>(feature.points.size()));
            for (const MapFeaturePoint &point : feature.points)
            {
                appendFloat(hash, point.x);
                appendFloat(hash, point.y);
            }
        }

        appendUint64(hash, static_cast<std::uint64_t>(map->features.size()));
        for (const MapFeatureModel &feature : map->features)
        {
            appendString(hash, feature.metadata.id);
            appendUint64(hash, static_cast<std::uint64_t>(feature.kind));
            appendString(hash, feature.name);
            appendString(hash, feature.description);
            appendInt(hash, feature.x);
            appendInt(hash, feature.y);
            appendBool(hash, feature.discovered);
            appendBool(hash, feature.gmOnly);
        }

        appendUint64(hash, static_cast<std::uint64_t>(map->regions.size()));
        for (const MapRegionModel &region : map->regions)
        {
            appendString(hash, region.metadata.id);
            appendString(hash, region.name);
            appendInt(hash, region.x);
            appendInt(hash, region.y);
            appendInt(hash, region.width);
            appendInt(hash, region.height);
            appendBool(hash, region.gmOnly);
        }

        appendUint64(hash, static_cast<std::uint64_t>(map->routes.size()));
        for (const MapRouteModel &route : map->routes)
        {
            appendString(hash, route.metadata.id);
            appendString(hash, route.name);
            appendInt(hash, route.startX);
            appendInt(hash, route.startY);
            appendInt(hash, route.endX);
            appendInt(hash, route.endY);
            appendBool(hash, route.discovered);
        }

        appendUint64(hash, static_cast<std::uint64_t>(map->tokens.size()));
        for (const MapTokenModel &token : map->tokens)
        {
            appendString(hash, token.metadata.id);
            appendString(hash, token.name);
            appendUint64(hash, static_cast<std::uint64_t>(token.kind));
            appendString(hash, token.assetId);
            appendString(hash, token.layerId);
            appendFloat(hash, token.x);
            appendFloat(hash, token.y);
            appendFloat(hash, token.rotation);
            appendFloat(hash, token.size);
            appendBool(hash, token.hiddenFromPlayers);
        }

        // World entities can further restrict generated feature visibility;
        // their public state is therefore part of the same review generation.
        appendUint64(hash, static_cast<std::uint64_t>(campaign.nations.size()));
        for (const NationModel &nation : campaign.nations)
        {
            appendString(hash, nation.metadata.id);
            appendString(hash, nation.name);
            appendUint64(hash, static_cast<std::uint64_t>(nation.visibility));
            appendUint64(hash, static_cast<std::uint64_t>(nation.territories.size()));
            for (const NationTerritoryModel &territory : nation.territories)
            {
                appendString(hash, territory.metadata.id);
                appendString(hash, territory.name);
            }
        }
        appendUint64(hash, static_cast<std::uint64_t>(campaign.worldPlaces.size()));
        for (const WorldPlaceModel &place : campaign.worldPlaces)
        {
            appendString(hash, place.metadata.id);
            appendString(hash, place.name);
            appendString(hash, place.linkedMapId);
            appendString(hash, place.linkedFeatureId);
            appendUint64(hash, static_cast<std::uint64_t>(place.visibility));
        }
    }

    const PlayerSafeStoryScene scene = buildPlayerSafeStoryScene(campaign);
    appendBool(hash, scene.available);
    appendString(hash, scene.title);
    appendString(hash, scene.playerSummary);
    appendString(hash, scene.readAloudText);

    const auto encounter = std::find_if(
        campaign.encounters.begin(), campaign.encounters.end(),
        [&](const EncounterModel &candidate) {
            return candidate.metadata.id == display.publicEncounterId;
        });
    appendBool(hash, encounter != campaign.encounters.end());
    if (encounter != campaign.encounters.end())
    {
        appendBool(hash, encounter->combatPhase == CombatPhase::WrapUp);
        appendString(hash, encounter->name);
        // This optional normalization is deliberately limited to values that
        // the explicit runtime refresh may replace in an already-approved
        // snapshot. Every row identity and all other public fields remain in
        // the fingerprint, so a reinforcement or changed initiative fails
        // closed instead of appearing on the player display.
        appendInt(hash, normalizeLiveCombatStats ? 0 : encounter->round);
        const std::vector<PlayerSafeInitiativeEntry> entries =
            buildPlayerSafeInitiative(*encounter);
        appendUint64(hash, static_cast<std::uint64_t>(entries.size()));
        int visibleEntryIndex = 0;
        for (int participantIndex = 0;
             participantIndex < static_cast<int>(encounter->participants.size()); ++participantIndex)
        {
            const EncounterParticipantModel &participant =
                encounter->participants[static_cast<std::size_t>(participantIndex)];
            if (participant.startsHidden ||
                (participant.reinforcementRound > 0 && participant.reinforcementRound > encounter->round))
                continue;
            // IDs are never rendered, but bind the reviewed visible rows to
            // their original roster. A replacement with the same display name
            // and initiative must still require a fresh review.
            appendString(hash, participant.metadata.id);
            const PlayerSafeInitiativeEntry &entry =
                entries[static_cast<std::size_t>(visibleEntryIndex++)];
            appendString(hash, entry.displayName);
            appendInt(hash, entry.initiative);
            appendBool(hash, normalizeLiveCombatStats ? false : entry.active);
        }
    }

    const std::vector<PlayerSafeNpcSummary> npcs =
        buildPlayerSafeNpcSummaries(campaign);
    appendUint64(hash, static_cast<std::uint64_t>(npcs.size()));
    for (const PlayerSafeNpcSummary &npc : npcs)
    {
        appendString(hash, npc.name);
        appendString(hash, npc.role);
    }

    const std::vector<PlayerSafeCharacterSummary> party =
        buildPlayerSafeCharacterSummaries(campaign);
    appendUint64(hash, static_cast<std::uint64_t>(party.size()));
    for (const PlayerSafeCharacterSummary &character : party)
    {
        appendString(hash, character.entityId);
        appendString(hash, character.name);
        appendString(hash, character.characterClass);
        appendInt(hash, character.level);
        appendInt(hash, normalizeLiveCombatStats ? 0 : character.currentHitPoints);
        appendInt(hash, character.maxHitPoints);
        appendInt(hash, normalizeLiveCombatStats ? 0 : character.temporaryHitPoints);
        appendInt(hash, character.speedFeet);
        appendUint64(hash, static_cast<std::uint64_t>(character.conditions.size()));
        for (const std::string &condition : character.conditions)
            appendString(hash, condition);
        appendUint64(hash, static_cast<std::uint64_t>(character.spellSlots.size()));
        for (const auto &slot : character.spellSlots)
        { appendInt(hash, slot.level); appendInt(hash, slot.available); appendInt(hash, slot.maximum); }
    }
    const std::vector<PlayerSafeInventoryItemSummary> inventory = buildPlayerSafeInventorySummaries(campaign);
    appendUint64(hash, static_cast<std::uint64_t>(inventory.size()));
    for (const PlayerSafeInventoryItemSummary &item : inventory)
    { appendString(hash, item.ownerName); appendString(hash, item.itemName); appendInt(hash, item.quantity); appendBool(hash, item.partyOwned); }

    const std::vector<PlayerSafeHandout> handouts = display.showPublicHandouts
                                                        ? buildPlayerSafeHandouts(campaign)
                                                        : std::vector<PlayerSafeHandout>{};
    appendUint64(hash, static_cast<std::uint64_t>(handouts.size()));
    for (PlayerSafeHandout handout : handouts)
    {
        const auto source = std::find_if(
            campaign.handouts.begin(), campaign.handouts.end(),
            [&](const HandoutModel &candidate) {
                return candidate.metadata.id == handout.assetId;
            });
        const bool bound = source != campaign.handouts.end() &&
                           bindManagedHandout(campaign, *source, handout, verifyAssetBytes);
        appendBool(hash, bound);
        appendString(hash, handout.assetId);
        appendString(hash, handout.title);
        appendString(hash, handout.body);
        appendString(hash, source == campaign.handouts.end() ? std::string{} : source->filePath);
        appendString(hash, handout.projectRelativePath);
        appendString(hash, handout.contentHash);
    }
    // Public journal and safety changes must invalidate the cached projection,
    // including revocation. The review hash alone cannot refresh a stale cache.
    const auto objectives = TabletopToolkit::playerSafeObjectives(campaign);
    appendUint64(hash, static_cast<std::uint64_t>(objectives.size()));
    for (const auto &objective : objectives)
    {
        appendString(hash, objective.id);
        appendString(hash, objective.title);
        appendString(hash, objective.summary);
        appendInt(hash, static_cast<int>(objective.status));
    }
    const auto clues = TabletopToolkit::playerSafeClues(campaign);
    appendUint64(hash, static_cast<std::uint64_t>(clues.size()));
    for (const auto &clue : clues)
    {
        appendString(hash, clue.id);
        appendString(hash, clue.title);
        appendString(hash, clue.text);
        appendString(hash, clue.discoveredAtUtc);
    }
    const auto boundaries = TabletopToolkit::playerSafeBoundaries(campaign);
    appendUint64(hash, static_cast<std::uint64_t>(boundaries.size()));
    for (const auto &boundary : boundaries)
    {
        appendInt(hash, static_cast<int>(boundary.kind));
        appendString(hash, boundary.topic);
        appendString(hash, boundary.guidance);
    }
    const auto warnings = TabletopToolkit::playerSafeWarnings(campaign);
    appendUint64(hash, static_cast<std::uint64_t>(warnings.size()));
    for (const auto &warning : warnings)
    {
        appendString(hash, warning.targetEntityId);
        appendString(hash, warning.targetEntityType);
        appendString(hash, warning.targetLabel);
        appendString(hash, warning.warning);
    }
    return hash == 0 ? 1 : hash;
}

} // namespace PlayerDisplayReview
