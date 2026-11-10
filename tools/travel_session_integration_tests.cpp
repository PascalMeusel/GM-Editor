#include "Models/Session/TravelSessionIntegration.hpp"
#include "Controllers/Editor/Widgets/MapViewer/MapViewerCampaignSaveTransaction.hpp"
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
const auto persist = [](Campaign &) { return true; };

Campaign fixture(EntityId &sessionId, EntityId &activeEntry)
{
    Campaign campaign;
    campaign.metadata.id = "travel_integration";
    const auto session = SessionPlan::createSession(campaign, "Live session", persist);
    require(session.applied(), "Session fixture creation failed");
    sessionId = session.sessionId;
    const auto scene = SessionPlan::createScene(campaign, sessionId, "Current scene", "Keep active", persist);
    require(scene.applied(), "Scene fixture creation failed");
    activeEntry = scene.entryId;
    require(SessionPlan::startWithEntry(campaign, sessionId, activeEntry, persist).applied(), "Session fixture start failed");
    MapModel geographic;
    geographic.metadata.id = "geographic"; geographic.atlasLevel = MapLevel::Region;
    MapModel tactical;
    tactical.metadata.id = "tactical"; tactical.atlasLevel = MapLevel::Tactical; tactical.parentMapId = "geographic";
    campaign.maps = {geographic, tactical};
    campaign.partyLocation.mapId = "tactical";
    campaign.partyTravel.currentMapId = "geographic";
    return campaign;
}

void acceptance()
{
    EntityId sessionId, activeEntry;
    auto campaign = fixture(sessionId, activeEntry);
    require(TravelSession::geographicSource(campaign, "tactical") == "geographic", "Tactical ancestor not resolved");
    require(TravelSession::geographicSource(campaign, "missing") == "geographic", "Active route fallback not resolved");
    campaign.maps[1].parentMapId = "tactical";
    campaign.partyTravel.currentMapId = "tactical";
    require(TravelSession::geographicSource(campaign, "tactical").empty(), "Cyclic parent map did not stop");
    campaign.maps[1].parentMapId = "geographic";
    campaign.partyTravel.currentMapId = "geographic";
    TravelSceneCandidate candidate;
    candidate.title = "Improvised clearing"; candidate.summary = "Public-looking but unreviewed summary";
    candidate.discovery = "A broken wagon"; candidate.complication = "Rain"; candidate.gmSecret = "GM_SECRET";
    candidate.localMap.metadata.id = "generated_map";
    std::vector<TravelSceneCandidate> candidates{candidate};
    int selectedMap = 0, selectedCandidate = 0;
    const auto originalMaps = campaign.maps.size();
    const auto originalThreads = campaign.storyThreads.size();
    std::string error;
    {
        MapViewerCampaignSaveTransaction<TravelSceneCandidate> transaction(campaign, selectedMap, candidates, selectedCandidate);
        campaign.maps.push_back(candidate.localMap);
        require(TravelSession::stageAcceptedScene(campaign, sessionId, candidate, "generated_map", {}, error), error.c_str());
        require(!transaction.persistOrRollback([] { return false; }), "Failed save was accepted");
    }
    require(campaign.maps.size() == originalMaps && campaign.storyThreads.size() == originalThreads &&
                SessionPlan::findSession(campaign, sessionId)->planEntries.size() == 1 && candidates.size() == 1 && selectedCandidate == 0,
            "Save failure did not preserve scene/map/candidate retry state");
    {
        MapViewerCampaignSaveTransaction<TravelSceneCandidate> transaction(campaign, selectedMap, candidates, selectedCandidate);
        campaign.maps.push_back(candidate.localMap);
        EncounterModel encounter; encounter.metadata.id = "prepared_encounter"; campaign.encounters.push_back(encounter);
        require(TravelSession::stageAcceptedScene(campaign, sessionId, candidate, "generated_map", "prepared_encounter", error), error.c_str());
        require(transaction.persistOrRollback([] { return true; }), "Successful acceptance failed");
    }
    const auto *session = SessionPlan::findSession(campaign, sessionId);
    require(session && session->planEntries.size() == 2 && session->liveState.activePlanEntryId == activeEntry,
            "Accepted improvisation changed active scene or failed to append plan entry");
    const auto *scene = SessionPlan::findScene(campaign, session->planEntries.back().storyBeatId);
    require(scene && scene->gmNotes == "GM_SECRET" && scene->playerSummary.empty() && scene->readAloudText.empty() &&
                scene->links.size() == 2 && scene->links[0].targetEntityId == "generated_map" &&
                scene->links[1].targetEntityId == "prepared_encounter", "Scene links/private projection incorrect");
    const auto count = session->planEntries.size();
    require(!TravelSession::stageAcceptedScene(campaign, "stale-session", candidate, "generated_map", {}, error) &&
                SessionPlan::findSession(campaign, sessionId)->planEntries.size() == count,
            "Changed active session accepted old candidate context");
    require(!TravelSession::stageAcceptedScene(campaign, sessionId, candidate, "missing_map", {}, error),
            "Missing map entered live session plan");
    Campaign noSession;
    require(TravelSession::stageAcceptedScene(noSession, {}, candidate, "atlas-only", {}, error) && noSession.storyThreads.empty(),
            "Atlas-only acceptance unexpectedly created a session");
}
}
int main()
{
    try { acceptance(); std::cout << "Travel/session integration checks passed\n"; return 0; }
    catch (const std::exception &exception) { std::cerr << exception.what() << '\n'; return 1; }
}
