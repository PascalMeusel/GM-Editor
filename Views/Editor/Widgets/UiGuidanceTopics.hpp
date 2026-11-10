#pragma once

#include <array>

namespace UiGuidance
{
enum class Topic
{
    PartyMarker, TravelWatch, CampaignParticipants, SessionPhases,
    SessionOutcomes, PlayerDisplay, DataSafety, Terms
};

// Static, generic guidance only. Campaign names, drafts and file paths belong
// in their existing private task panels, never in this shared catalog.
struct TopicText
{
    const char *title;
    const char *purpose;
    const char *prerequisites;
    const char *effects;
    const char *example;
};

const TopicText &text(Topic topic);

namespace Detail
{
inline constexpr std::array<TopicText, 8> english{{
    {
        "Party marker",
        "The party marker records one campaign location. It does not move character or combat tokens.",
        "Select a map and Place party marker. Choose a destination cell, review it, then use Save party location.",
        "The action changes the saved map location of the fictional group and requests a campaign save. "
        "Matching region audio triggers may play. Route progress and the game clock do not advance.",
        "Choose the bridge cell and confirm it. The group location changes to the bridge; a character's combat position stays unchanged."
    },
    {
        "Travel watch",
        "A watch is one travel step. It updates distance and travel tallies; the party marker and game clock stay where they are.",
        "Start a route with a positive distance and choose a pace. Illustrated roads alone are not travel routes. "
        "The active route must have distance remaining.",
        "Slow, normal and fast add up to 6, 8 and 10 miles, capped at the route distance. Each watch adds one watch, "
        "one encounter-check tally and one ration per campaign character, with a minimum of one ration. "
        "Inventory is not deducted and no encounter is rolled automatically. Restarting a route resets its distance progress, "
        "watches and encounter checks; the ration tally is retained.",
        "Three campaign characters at normal pace: up to 8 miles, 3 rations and 1 encounter check. "
        "If only 2 miles remain, distance increases by 2 miles and the same tallies apply. Place the marker separately."
    },
    {
        "Campaign characters and encounter participants",
        "Add encounter participants from the campaign character list. This is not a separate party roster.",
        "The campaign needs character records and a ruleset. Choose a character or add all campaign characters "
        "to the current encounter setup.",
        "Each press adds another participant, including characters already in the encounter. "
        "A participant has combat health, actions and initiative, and may also have a battlefield position. "
        "Review the participant list and save the encounter setup.",
        "With three campaign characters, Add all campaign characters adds three participants. "
        "Pressing it again adds three more; it does not update or deduplicate the first three."
    },
    {
        "Session phases",
        "Prepare a session, start it to play, then record outcomes and finish. A phase selects tools for the current activity.",
        "Check the session title and selected scene. Run session in preparation saves its draft and starts the selected prepared session. "
        "The Live start control has a separate start review. An active session is needed for saved phase changes.",
        "During an active session, phase changes are saved. Break pauses session timing; leaving Break resumes it. "
        "This is session timing, not the in-world clock. Wrap-up does not end a session: use End session. "
        "Viewing a completed recap is read-only.",
        "Choose Break during a table break, then return to Scene. Later choose Wrap-up to record the recap, "
        "and use End session when the session is finished."
    },
    {
        "Session outcomes",
        "Record what happened privately. Proposed campaign changes are applied only after a separate review.",
        "Choose the source session. For a proposed change, choose its linked entry and enter the desired change. "
        "For the next session, choose an unstarted destination or create one.",
        "Record saves the outcome; Review compares and applies selected proposed changes. "
        "Next session carries selected follow-up text and scene links without starting that session or applying campaign changes. "
        "Player recap uses separate player text and chosen public handouts; private outcome text is never a fallback.",
        "Record 'Mara agreed to help', propose an NPC note, then review and apply it. "
        "Carry 'Ask Mara about the ruins' into the next session. Write a separate player sentence if you want to share it."
    },
    {
        "Player display",
        "Open creates a black player window. Show to players displays the reviewed selection; editing the GM view does not publish it.",
        "Choose the public scene, map, encounter and other content in the player display controls. "
        "Wait for review preparation, inspect the player preview, then confirm the requested action.",
        "Open player window keeps output black. Show to players publishes the approved selection. "
        "Blackout hides content immediately. Changed content requires a new review. "
        "The separate health/turn update changes only current and temporary HP, active turn and round for the already reviewed roster. "
        "Presentation-safe GM preview hides supported GM sections; it is not a desktop-wide privacy mode.",
        "Review a public scene, open the player window, then use Show to players after review. "
        "Use Blackout before selecting another scene; opening that scene in the GM editor alone does not share it."
    },
    {
        "Save, recovery, backups and transfer",
        "Save campaign data, recover unfinished edits, restore an earlier state or transfer a campaign: these are different tasks.",
        "Check the selected campaign and the exact scope shown before restoring or exporting. "
        "For backups, open App Settings in the campaign launcher, then Browse & Restore Backups. "
        "Use Export selected campaign in the launcher to create a transfer package.",
        "Save writes campaign data; some editors have a separate Save action for their draft. Supported draft recovery "
        "reopens unfinished edits for review and does not apply them automatically. Managed database backups may contain "
        "the whole campaign library; the backup browser restores only the selected campaign and the displayed package scope "
        "(keep, replace or remove). A campaign transfer package contains only the selected campaign and its managed assets. "
        "GM Markdown contains private readable notes and can include editable story-thread GM text; it is not a campaign backup. "
        "Update exported notes reviews those edits against this campaign and saves only selected compatible changes. Conflicts stay unchanged. "
        "Import Markdown creates a new private note. Neither operation imports linked entities or attachments. "
        "Player recap export contains separately reviewed player text and chosen public handouts.",
        "To move one campaign, export its campaign package. To reverse a mistaken restore, review its Before restore safety entry "
        "and exact scope. Do not send a whole-library database backup as though it contained only that campaign."
    },
    {
        "Terms and actions",
        "Use these terms to distinguish campaign records, map objects and deliberate sharing actions.",
        "Check the named campaign, session, map or encounter next to an action. Similar names do not make two records the same target.",
        "Character: a campaign character record. Party: the fictional group; these actions have no separate membership selector. "
        "Party marker: one saved map location. Token: a positioned map object. Participant: a combat entry, including in theater-of-mind play. "
        "Scene: reusable story content linked into a session. Route: a distance-bearing travel record, not decorative road art. "
        "Watch: one travel increment, not a guaranteed number of hours. Outcome: a record of what happened that may propose a later change. "
        "Publication: a reviewed player-facing selection.",
        "A scene can be linked into two sessions. Placing the party marker near it does not start either session, "
        "move combat participants or publish the scene. Use each task's explicit action."
    }
}};

inline constexpr std::array<TopicText, 8> german{{
    {
        "Gruppenmarker",
        "Der Gruppenmarker speichert einen Kampagnenstandort. Figuren und Kampfmarker werden dadurch nicht bewegt.",
        "Wähle eine Karte und Gruppenmarker setzen. Wähle ein Zielfeld, prüfe es und nutze Gruppenposition speichern.",
        "Die Aktion ändert den gespeicherten Kartenstandort der fiktiven Gruppe und fordert eine Kampagnenspeicherung an. "
        "Passende Audioauslöser der Region können starten. Reisefortschritt und Spieluhr ändern sich nicht.",
        "Wähle das Brückenfeld und bestätige es. Die Gruppenposition liegt danach an der Brücke; "
        "die Kampfposition einer Figur bleibt unverändert."
    },
    {
        "Reiseabschnitt",
        "Ein Reiseabschnitt ist ein Reiseschritt. Er erhöht Entfernung und Reisezähler; Gruppenmarker und Spieluhr bleiben unverändert.",
        "Starte eine Route mit positiver Entfernung und wähle ein Tempo. Gezeichnete Straßen allein sind keine Reiserouten. "
        "Auf der aktiven Route muss noch Strecke übrig sein.",
        "Langsam, normal und schnell erhöhen die Strecke um bis zu 6, 8 und 10 Meilen, höchstens bis zur Routenlänge. "
        "Jeder Schritt zählt einen Abschnitt, eine Begegnungsprüfung und eine Ration pro Kampagnenfigur, mindestens eine Ration. "
        "Inventar wird nicht abgezogen und keine Begegnung automatisch ausgewürfelt. Beim Neustart einer Route werden "
        "Streckenfortschritt, Abschnitte und Begegnungsprüfungen zurückgesetzt; der Rationszähler bleibt erhalten.",
        "Drei Kampagnenfiguren bei normalem Tempo: bis zu 8 Meilen, 3 Rationen und 1 Begegnungsprüfung. "
        "Sind nur 2 Meilen übrig, kommen 2 Meilen und dieselben Zähler hinzu. Den Marker setzt du separat."
    },
    {
        "Kampagnenfiguren und Kampfteilnehmende",
        "Füge Kampfteilnehmende aus der Figurenliste der Kampagne hinzu. Eine eigene Gruppenauswahl gibt es hier nicht.",
        "Die Kampagne benötigt Figuren und ein Regelwerk. Wähle eine Figur oder füge alle Kampagnenfiguren "
        "zur aktuellen Begegnungsvorbereitung hinzu.",
        "Jeder Klick fügt weitere Teilnehmende hinzu, auch wenn die Figuren bereits im Kampf sind. "
        "Ein Kampfteilnehmender hat TP, Aktionen und Initiative und kann eine Position auf dem Schlachtfeld haben. "
        "Prüfe die Teilnehmenden und speichere die Begegnungsvorbereitung.",
        "Bei drei Kampagnenfiguren fügt Alle Kampagnenfiguren hinzufügen drei Teilnehmende hinzu. "
        "Ein weiterer Klick fügt drei weitere hinzu; die ersten drei werden weder aktualisiert noch zusammengeführt."
    },
    {
        "Sitzungsphasen",
        "Bereite eine Sitzung vor, starte sie zum Spielen, halte Ergebnisse fest und beende sie. Die Phase zeigt Werkzeuge für die aktuelle Tätigkeit.",
        "Prüfe Sitzungstitel und ausgewählte Szene. Sitzung starten in der Vorbereitung speichert den Entwurf und startet die ausgewählte vorbereitete Sitzung. "
        "Der Start in Live hat eine eigene Startprüfung. Phasenwechsel werden bei einer aktiven Sitzung gespeichert.",
        "Während einer aktiven Sitzung werden Phasenwechsel gespeichert. Pause hält die Sitzungszeit an; beim Verlassen läuft sie weiter. "
        "Dies betrifft die Sitzungszeit, nicht die Spielweltuhr. Abschluss beendet die Sitzung nicht: Nutze Sitzung beenden. "
        "Das Betrachten eines abgeschlossenen Rückblicks ist schreibgeschützt.",
        "Wähle Pause für eine Tischpause und kehre danach zu Szene zurück. Wähle später Abschluss für den Rückblick "
        "und nutze Sitzung beenden, wenn die Sitzung fertig ist."
    },
    {
        "Sitzungsergebnisse",
        "Halte privat fest, was geschehen ist. Vorgeschlagene Kampagnenänderungen werden erst nach einer eigenen Prüfung übernommen.",
        "Wähle die Ursprungssitzung. Wähle für eine vorgeschlagene Änderung den verknüpften Eintrag und gib die Änderung ein. "
        "Wähle als nächste Sitzung ein noch nicht gestartetes Ziel oder erstelle eines.",
        "Festhalten speichert das Ergebnis; Prüfen vergleicht und übernimmt ausgewählte Änderungsvorschläge. "
        "Nächste Sitzung übernimmt ausgewählte Folgetexte und Szenenverknüpfungen, ohne die Sitzung zu starten oder Kampagnenänderungen anzuwenden. "
        "Spieler-Rückblick nutzt eigenen Spielertext und gewählte öffentliche Handouts; privater Ergebnistext wird niemals ersatzweise verwendet.",
        "Halte 'Mara hat Hilfe zugesagt' fest, schlage eine NSC-Notiz vor, prüfe und übernimm sie. "
        "Übernimm 'Mara nach den Ruinen fragen' in die nächste Sitzung. Schreibe bei Bedarf einen eigenen Satz für Spieler."
    },
    {
        "Spieleranzeige",
        "Öffnen erzeugt ein schwarzes Spielerfenster. Spielern zeigen zeigt die geprüfte Auswahl; Änderungen in der SL-Ansicht werden nicht automatisch veröffentlicht.",
        "Wähle öffentliche Szene, Karte, Begegnung und weitere Inhalte in den Steuerelementen der Spieleranzeige. "
        "Warte auf die Vorbereitung, prüfe die Spielervorschau und bestätige dann die gewünschte Aktion.",
        "Spielerfenster öffnen lässt die Ausgabe schwarz. Spielern zeigen veröffentlicht die geprüfte Auswahl. "
        "Schwarzbild blendet Inhalte sofort aus. Geänderte Inhalte benötigen eine neue Prüfung. "
        "Die eigene TP-/Zug-Aktualisierung ändert nur aktuelle und temporäre TP, aktiven Zug und Runde der bereits geprüften Teilnehmenden. "
        "Die präsentationssichere SL-Vorschau verbirgt unterstützte SL-Abschnitte; sie ist kein Schutzmodus für den gesamten Desktop.",
        "Prüfe eine öffentliche Szene, öffne das Spielerfenster und nutze nach der Prüfung Spielern zeigen. "
        "Nutze Schwarzbild vor der Auswahl einer anderen Szene; ihr Öffnen im SL-Editor allein gibt sie nicht frei."
    },
    {
        "Speichern, Wiederherstellung, Sicherung und Übertragung",
        "Kampagnendaten speichern, unfertige Änderungen retten, einen früheren Stand wiederherstellen oder eine Kampagne übertragen sind verschiedene Aufgaben.",
        "Prüfe die ausgewählte Kampagne und den angezeigten Umfang vor einer Wiederherstellung oder einem Export. "
        "Öffne für Sicherungen die App-Einstellungen im Kampagnenstartfenster und dann Sicherungen durchsuchen & wiederherstellen. "
        "Ausgewählte Kampagne exportieren im Startfenster erstellt ein Übertragungspaket.",
        "Speichern schreibt Kampagnendaten; manche Editoren haben eine eigene Speicheraktion für ihren Entwurf. "
        "Unterstützte Entwurfswiederherstellung öffnet unfertige Änderungen zur Prüfung und übernimmt sie nicht automatisch. "
        "Verwaltete Datenbanksicherungen können die gesamte Kampagnenbibliothek enthalten; die Sicherungsübersicht stellt nur "
        "die gewählte Kampagne und den angezeigten Paketumfang wieder her (beibehalten, ersetzen oder entfernen). "
        "Ein Kampagnenpaket enthält nur die gewählte Kampagne und ihre verwalteten Anlagen. "
        "SL-Markdown enthält lesbare private Notizen und optional bearbeitbaren SL-Text aus Handlungssträngen; es ist keine Kampagnensicherung. "
        "Exportierte Notizen aktualisieren vergleicht diese Änderungen mit der Kampagne und speichert nur ausgewählte passende Änderungen. "
        "Konflikte bleiben unverändert. Markdown importieren erstellt eine neue private Notiz. Beide Vorgänge importieren weder verknüpfte Einträge noch Anhänge. "
        "Spieler-Rückblick exportiert getrennt geprüften Spielertext und gewählte öffentliche Handouts.",
        "Exportiere zum Übertragen einer Kampagne ihr Kampagnenpaket. Prüfe zum Rückgängigmachen einer Wiederherstellung "
        "deren Sicherheitseintrag Vor der Wiederherstellung und seinen genauen Umfang. Versende keine Sicherung der ganzen Bibliothek, "
        "als enthielte sie nur diese Kampagne."
    },
    {
        "Begriffe und Aktionen",
        "Unterscheide mit diesen Begriffen Kampagneneinträge, Kartenobjekte und bewusste Freigaben.",
        "Prüfe Kampagne, Sitzung, Karte oder Begegnung, die neben einer Aktion genannt wird. Ähnliche Namen machen zwei Einträge nicht zum selben Ziel.",
        "Figur: ein Figureneintrag der Kampagne. Gruppe: die fiktive Gruppe; diese Aktionen haben keine eigene Mitgliederauswahl. "
        "Gruppenmarker: ein gespeicherter Kartenstandort. Marker: ein platziertes Kartenobjekt. Kampfteilnehmender: ein Kampfeintrag, auch im erzählerischen Kampf. "
        "Szene: wiederverwendbarer, mit einer Sitzung verknüpfter Erzählstoff. Reiseroute: ein Reiseeintrag mit Entfernung, keine gezeichnete Straße. "
        "Reiseabschnitt: ein Reiseschritt, keine garantierte Stundenzahl. Ergebnis: ein Geschehnis, das eine spätere Änderung vorschlagen kann. "
        "Freigabe: eine geprüfte Auswahl für Spieler.",
        "Eine Szene kann mit zwei Sitzungen verknüpft sein. Ein Gruppenmarker in ihrer Nähe startet keine Sitzung, "
        "bewegt keine Kampfteilnehmenden und gibt die Szene nicht frei. Nutze die jeweilige ausdrückliche Aktion."
    }
}};
} // namespace Detail
} // namespace UiGuidance
