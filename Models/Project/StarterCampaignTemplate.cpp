#include "StarterCampaignTemplate.hpp"
#include "Models/Session/SessionOutcomeService.hpp"

#include <utility>

namespace StarterCampaignTemplate
{
namespace
{
StoryBeatModel beat(const char *title,
                    const char *summary,
                    const char *gmNotes,
                    StoryStatus status = StoryStatus::Idea)
{
    StoryBeatModel result;
    result.title = title;
    result.summary = summary;
    result.gmNotes = gmNotes;
    result.status = status;
    result.metadata.tags = {"starter", "guide"};
    return result;
}
} // namespace

void apply(Campaign &campaign, Language language)
{
    const bool german = language == Language::German;
    campaign.metadata.tags.push_back("guided-starter-v1");
    campaign.metadata.customProperties["starter_template"] = "guided_v1";
    // The opening scene is a conversation. Start with the journal layout so
    // first-time GMs can share its text before the first map is introduced.
    campaign.localPlayerDisplay.mapOnly = false;
    campaign.localPlayerDisplay.fullscreenMapOnly = false;

    if (campaign.description.empty())
    {
        campaign.description = german
                                   ? "Ein geführter Startbereich zum Vorbereiten der ersten Spielrunde. Alle Beispielinhalte können frei geändert oder gelöscht werden."
                                   : "A guided workspace for preparing the first session. Every example can be changed or deleted.";
    }

    StoryThreadModel guide;
    guide.title = german ? "Hier anfangen: Erste Sitzung" : "Start here: First session";
    guide.type = StoryThreadType::SessionThread;
    guide.status = StoryStatus::Planned;
    guide.summary = german
                        ? "Drei kurze Schritte führen von der Gruppe über die Welt zur ersten spielbaren Szene."
                        : "Three short steps lead from the party and world to a playable opening scene.";
    guide.stakes = german
                       ? "Am Ende steht eine kleine, spielbereite Ausgangslage."
                       : "Finish with a small situation that is ready to play.";
    guide.gmNotes = german
                        ? "Öffne links Geschichte, Figuren und Welt. Passe die Beispiele an deine Runde an und lösche diesen Leitfaden, sobald du ihn nicht mehr brauchst."
                        : "Open Story, Cast, and World from the left navigation. Adapt the examples to your table and delete this guide when it is no longer useful.";
    guide.metadata.tags = {"starter", "guide"};

    if (german)
    {
        guide.beats.push_back(beat("1. Gruppe anlegen",
                                   "Lege unter Figuren die ersten Spielercharaktere an.",
                                   "Namen und ein Satz zum Ziel jeder Figur reichen für den Anfang.",
                                   StoryStatus::Planned));
        guide.beats.push_back(beat("2. Ausgangsort festlegen",
                                   "Passe den Beispielort Lichtermarkt unter Welt an oder ersetze ihn.",
                                   "Ein Ort, ein Problem und eine auffällige Person genügen."));
        guide.beats.push_back(beat("3. Eröffnungsszene vorbereiten",
                                   "Schreibe eine konkrete Situation und stelle sie für Live bereit.",
                                   "Was sehen die Figuren zuerst? Wer will etwas von ihnen? Was passiert, wenn sie nichts tun?"));
    }
    else
    {
        guide.beats.push_back(beat("1. Add the party",
                                   "Create the first player characters in Cast.",
                                   "A name and one sentence about each character's goal is enough to begin.",
                                   StoryStatus::Planned));
        guide.beats.push_back(beat("2. Choose a starting place",
                                   "Adapt the example place Lantern Fair in World or replace it.",
                                   "One place, one problem, and one memorable person is enough."));
        guide.beats.push_back(beat("3. Prepare the opening scene",
                                   "Write one concrete situation and stage it for Live.",
                                   "What do the characters see first? Who wants something from them? What happens if they do nothing?"));
    }
    campaign.storyThreads.push_back(std::move(guide));

    NPCModel guideNpc;
    guideNpc.name = "Mara Vell";
    guideNpc.category = NPCCategory::Person;
    guideNpc.role = german ? "Gastgeberin und Gerüchtequelle" : "Host and source of rumors";
    guideNpc.description = german
                               ? "Mara organisiert den Lichtermarkt. Sie kennt jeden Stand, aber verschweigt, warum die alten Laternen zuletzt von selbst erloschen."
                               : "Mara organizes the Lantern Fair. She knows every stall, but hides why the old lanterns recently went dark on their own.";
    guideNpc.attitudeTowardParty = german ? "Freundlich, aber abwägend" : "Friendly but cautious";
    guideNpc.goals = german ? "Den Markt sicher eröffnen" : "Open the fair safely";
    guideNpc.secrets = german
                           ? "Unter dem Marktplatz befindet sich ein versiegelter Zugang."
                           : "A sealed passage lies beneath the market square.";
    guideNpc.dialogueNotes = german
                                 ? "Direkt, warmherzig, wird bei Fragen zu den Laternen knapp."
                                 : "Direct and warm; becomes terse when asked about the lanterns.";
    guideNpc.metadata.tags = {"starter", "example"};
    campaign.npcs.push_back(std::move(guideNpc));

    WorldPlaceModel guidePlace;
    guidePlace.type = WorldPlaceType::Town;
    guidePlace.name = german ? "Lichtermarkt" : "Lantern Fair";
    guidePlace.description = german
                                 ? "Ein kleiner Handelsort, der sich um einen alten, von Laternen umstandenen Platz gebildet hat."
                                 : "A small trading settlement built around an old square ringed by lanterns.";
    guidePlace.notes = german
                           ? "Beispielort: verändern, vertiefen oder löschen. Eine Karte kann später direkt aus dem Ort erstellt werden."
                           : "Example place: change, expand, or delete it. A map can be created from this place later.";
    guidePlace.visibility = WorldObjectVisibility::Discovered;
    guidePlace.metadata.tags = {"starter", "example"};
    campaign.worldPlaces.push_back(std::move(guidePlace));

    MapModel chamber;
    chamber.name = german ? "Unter dem Lichtermarkt" : "Beneath Lantern Fair";
    chamber.description = german ? "Ein kleiner Gewölberaum mit Zugang im Süden, zwei Pfeilern und einer Laterne im Norden." : "A small vaulted room with a southern entrance, two pillars and a lantern to the north.";
    chamber.scope = MapScope::Dungeon;
    chamber.atlasLevel = MapLevel::Tactical;
    chamber.mapKind = MapKind::Encounter;
    chamber.creationIntent = MapCreationIntent::TacticalEncounter;
    chamber.width = 18; chamber.height = 12; chamber.cellSize = 5.0f;
    chamber.spatialReference.unit = MapDistanceUnit::Feet;
    chamber.spatialReference.calibrated = true;
    chamber.spatialReference.extentWidth = 90.0f;
    chamber.spatialReference.extentHeight = 60.0f;
    chamber.cells.resize(static_cast<std::size_t>(chamber.width * chamber.height));
    for (int y = 0; y < chamber.height; ++y) for (int x = 0; x < chamber.width; ++x)
    {
        auto &cell = chamber.cells[static_cast<std::size_t>(y * chamber.width + x)];
        const bool wall = x == 0 || y == 0 || x == chamber.width - 1 || y == chamber.height - 1;
        cell.kind = wall ? MapTileKind::Wall : MapTileKind::Room;
        cell.lighting = MapLightingMode::Dim;
        cell.blocksLineOfSight = wall;
        cell.movementBlockedMask = wall ? 255 : 0;
        cell.lineOfSightBlockedMask = wall ? 255 : 0;
    }
    for (int x : {8, 9})
    {
        auto &door = chamber.cells[static_cast<std::size_t>((chamber.height - 1) * chamber.width + x)];
        door.kind = MapTileKind::Door; door.blocksLineOfSight = false;
        door.movementBlockedMask = 0; door.lineOfSightBlockedMask = 0;
    }
    for (int x : {5, 12})
    {
        auto &pillar = chamber.cells[static_cast<std::size_t>(5 * chamber.width + x)];
        pillar.kind = MapTileKind::Cover; pillar.difficultTerrain = true; pillar.movementCost = 2;
    }
    const EntityId chamberId = chamber.metadata.id;
    campaign.worldPlaces.back().linkedMapId = chamberId;
    campaign.maps.push_back(std::move(chamber));

    EncounterModel encounter;
    encounter.name = german ? "Das Lichtwesen beschützen" : "Protect the light spirit";
    encounter.description = german ? "Verhandelt eine Zuflucht für das verletzte Wesen. Wenn die Gruppe es bedroht, wehrt es sich und flieht. Ergänze vor einem Kampf die Spielerfiguren über Alle Spieler hinzufügen." : "Negotiate shelter for the injured spirit. If threatened, it defends itself and flees. Before combat, add the party with Add all players.";
    encounter.mapId = chamberId; encounter.tacticalMapId = chamberId;
    encounter.battlefieldWidth = 18; encounter.battlefieldHeight = 12; encounter.cellSizeFeet = 5;
    encounter.useMapCells = true;
    encounter.battlefieldCells = campaign.maps.back().cells;
    EncounterParticipantModel spirit;
    spirit.displayName = german ? "Verletztes Lichtwesen" : "Injured light spirit";
    spirit.currentHitPoints = 6; spirit.maxHitPoints = 10; spirit.armorClass = 11; spirit.speedFeet = 20;
    spirit.startingX = spirit.x = spirit.turnStartX = 9;
    spirit.startingY = spirit.y = spirit.turnStartY = 3;
    spirit.friendly = true;
    MonsterActionModel spark; spark.name = german ? "Warnfunke" : "Warning spark";
    spark.attackBonus = "+2"; spark.damage = "1d4"; spark.reachFeet = 5;
    spark.notes = german ? "Nur zur Selbstverteidigung; nach einem Treffer sucht das Wesen Schutz." : "Self-defense only; after a hit, the spirit seeks shelter.";
    spirit.actions.push_back(std::move(spark)); encounter.participants.push_back(std::move(spirit));
    const EntityId encounterId = encounter.metadata.id;
    campaign.encounters.push_back(std::move(encounter));

    SessionModel session;
    session.title = german ? "Erste Sitzung: Die erloschenen Laternen" : "First session: The darkened lanterns";
    session.roughNotes = german
        ? "Der Markt soll heute öffnen. Die Laternen erlöschen. Mara kennt einen versiegelten Zugang. Die Gruppe entscheidet, was darunter geweckt werden darf."
        : "The fair opens today. Its lanterns go dark. Mara knows a sealed entrance. The party must decide what may be awakened beneath it.";
    StoryThreadModel &opening = campaign.storyThreads.back();
    opening.title = session.title;
    opening.summary = german ? "Drei spielbare Szenen für eine kurze erste Sitzung." : "Three playable scenes for a short first session.";
    opening.gmNotes = german ? "Nutze Sitzungsplan zum Umordnen und Live zum Leiten. Verknüpfte Personen und Orte werden gemeinsam bearbeitet." : "Reorder scenes in the session plan and run them in Live. Linked people and places remain shared campaign content.";
    opening.beats.clear();
    opening.beats.push_back(beat(german ? "Ankunft im Dunkeln" : "Arrival in darkness",
        german ? "Am Markttor bittet Mara die Gruppe, eine vermisste Laternenhüterin zu finden." : "At the fair gate, Mara asks the party to find a missing lamplighter.",
        german ? "Mara verschweigt den Zugang unter dem Platz. Bei einer konkreten Hilfszusage zeigt sie den Schlüssel." : "Mara hides the passage beneath the square. When the party offers concrete help, she shows its key.", StoryStatus::Planned));
    opening.beats.push_back(beat(german ? "Spuren unter dem Platz" : "Traces beneath the square",
        german ? "Rußspuren und eine warme Bodenplatte führen zum versiegelten Zugang." : "Soot and a warm paving stone lead to the sealed passage.",
        german ? "Jede sinnvolle Untersuchung findet einen Hinweis. Druck entsteht durch die näherkommende Marktöffnung, nicht durch eine Sackgasse." : "Any reasonable investigation finds a clue. The approaching opening creates pressure, not a dead end.", StoryStatus::Planned));
    opening.beats.push_back(beat(german ? "Eine Laterne für morgen" : "A lantern for tomorrow",
        german ? "Die vermisste Hüterin schützt unten ein verletztes Lichtwesen. Die Gruppe handelt eine Lösung aus." : "Below, the missing lamplighter shelters an injured light spirit. The party negotiates a solution.",
        german ? "Ein Versprechen, eine Reparatur oder ein sicherer Zufluchtsort kann helfen. Halte eine Folge und einen offenen Aufhänger im Abschluss fest." : "A promise, a repair or safe shelter can help. Record a consequence and an open hook in wrap-up.", StoryStatus::Planned));
    for (auto &scene : opening.beats)
    {
        StoryLinkModel person; person.targetEntityType = "NPC"; person.targetEntityId = campaign.npcs.back().metadata.id;
        StoryLinkModel place; place.targetEntityType = "world_place"; place.targetEntityId = campaign.worldPlaces.back().metadata.id;
        scene.links = {person, place};
        scene.playerSummary = scene.summary;
        SessionPlanEntry entry; entry.storyBeatId = scene.metadata.id; session.planEntries.push_back(std::move(entry));
    }
    opening.beats.front().readAloudText = german
        ? "Zwischen den Marktständen hängen dunkle Laternen. Eine Frau hält euch einen leeren Laternenhaken entgegen: 'Sie hätte längst zurück sein sollen.'"
        : "Dark lanterns hang between the stalls. A woman holds out an empty lantern hook: 'She should have been back by now.'";
    StoryLinkModel clueMapLink; clueMapLink.targetEntityType = "Map"; clueMapLink.targetEntityId = chamberId;
    opening.beats[1].links.push_back(std::move(clueMapLink));
    StoryLinkModel finaleMapLink; finaleMapLink.targetEntityType = "Map"; finaleMapLink.targetEntityId = chamberId;
    opening.beats[2].links.push_back(std::move(finaleMapLink));
    StoryLinkModel encounterLink; encounterLink.targetEntityType = "Encounter"; encounterLink.targetEntityId = encounterId;
    opening.beats[2].links.push_back(std::move(encounterLink));
    StoryLinkModel ownership; ownership.targetEntityType = "Session"; ownership.targetEntityId = session.metadata.id;
    opening.links.push_back(std::move(ownership));
    // These are proposals to adapt after play, not facts imposed on the campaign.
    // No attitude, player visibility or next-session state changes until reviewed.
    SessionOutcomes::Ledger outcomeExamples;
    SessionOutcomes::Outcome trust;
    trust.privateText = german
        ? "Beispiel für den Abschluss: Hat die Gruppe dem Lichtwesen geholfen, vertraut Mara ihr. Passe dies an die tatsächliche Entscheidung an oder lehne den Vorschlag ab."
        : "Wrap-up example: if the party helped the light spirit, Mara trusts them. Adapt this to the actual decision or reject the proposal.";
    trust.publicText = german ? "Mara dankt euch für den Schutz des Lichtwesens." : "Mara thanks you for sheltering the light spirit.";
    trust.sourceSceneId = opening.beats.back().metadata.id;
    trust.target = {SessionOutcomes::TargetType::Npc, campaign.npcs.back().metadata.id};
    trust.action = SessionOutcomes::Action::NpcAttitude;
    trust.changeText = german ? "Vertraut der Gruppe nach ihrer Hilfe" : "Trusts the party after their help";
    outcomeExamples.outcomes.push_back(std::move(trust));
    SessionOutcomes::Outcome nextHook;
    nextHook.kind = SessionOutcomes::Kind::OpenHook;
    nextHook.privateText = german
        ? "Beispiel für Sitzung zwei: Falls die Gruppe eine sichere Zuflucht verspricht, bittet Mara sie beim nächsten Markttag um Hilfe mit den alten Laternen. Übernimm diesen Aufhänger nur, wenn er zur Entscheidung passt."
        : "Second-session example: if the party promises safe shelter, Mara asks for help with the old lanterns on the next market day. Carry this hook forward only if it fits their decision.";
    nextHook.sourceSceneId = opening.beats.back().metadata.id;
    nextHook.target = {SessionOutcomes::TargetType::Place, campaign.worldPlaces.back().metadata.id};
    outcomeExamples.outcomes.push_back(std::move(nextHook));
    SessionOutcomes::write(session, outcomeExamples);
    session.roughNotes += german
        ? "\n\nNach dem Spiel: Öffne die Ergebnisse, passe die beiden Beispiele an und prüfe die vorgeschlagene Änderung. Übernimm den gewählten Aufhänger in eine neue zweite Sitzung. Spielertexte bleiben bis zu deiner ausdrücklichen Auswahl privat."
        : "\n\nAfter play: open Outcomes, adapt the two examples and review the proposed change. Carry the chosen hook into a new second session. Player text remains private until you explicitly select it.";
    campaign.sessionIds.push_back(session.metadata.id);
    campaign.sessions.push_back(std::move(session));
}
} // namespace StarterCampaignTemplate
