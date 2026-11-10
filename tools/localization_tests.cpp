#include "Models/Localization/LocaleFormatting.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Localization/LocalizationSettings.hpp"
#include "Models/Localization/TranslationCatalog.hpp"
#include "Models/Rulesets/RulesetDefinitionModel.hpp"
#include "Models/Rulesets/RulesetDefinitionValidator.hpp"
#include "Models/Rulesets/RulesetFormulaEvaluator.hpp"
#include "Models/Rulesets/RulesetPresentationFormatter.hpp"
#include "Models/Settings/AccessibilitySettings.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace
{
static_assert(U'ü' == U'\u00fc', "Compile source text as UTF-8, independently of the Windows code page");
static_assert(sizeof(u8"Größe") == 8, "UTF-8 literals must encode German text exactly once");
bool require(bool condition, const std::string &message)
{
    if (condition)
        return true;
    std::cerr << message << '\n';
    return false;
}

bool isWellFormedUtf8(std::string_view text)
{
    for (std::size_t index = 0; index < text.size();)
    {
        const unsigned char lead = static_cast<unsigned char>(text[index]);
        std::size_t continuationCount = 0;
        if (lead <= 0x7f)
            continuationCount = 0;
        else if (lead >= 0xc2 && lead <= 0xdf)
            continuationCount = 1;
        else if (lead >= 0xe0 && lead <= 0xef)
            continuationCount = 2;
        else if (lead >= 0xf0 && lead <= 0xf4)
            continuationCount = 3;
        else
            return false;

        if (index + continuationCount >= text.size())
            return false;
        for (std::size_t offset = 1; offset <= continuationCount; ++offset)
        {
            const unsigned char continuation = static_cast<unsigned char>(text[index + offset]);
            if ((continuation & 0xc0) != 0x80)
                return false;
        }
        index += continuationCount + 1;
    }
    return true;
}
} // namespace

int main()
{
    using namespace gm::localization;
    bool ok = true;

    ok &= require(catalogIsComplete(Locale::English), "English translation catalog is incomplete");
    ok &= require(catalogIsComplete(Locale::German), "German translation catalog is incomplete");
    ok &= require(translation(Locale::German, TextId::ProductName) == "GM Editor",
                  "The German UI must preserve the canonical product name");
    ok &= require(translation(Locale::German, TextId::LauncherWelcome) == "Willkommen bei GM Editor",
                  "The German launcher must use the canonical product name");
    ok &= require(translation(Locale::English, TextId::WorkspaceEncounters) == "Encounters",
                  "English workspace translation is incorrect");
    ok &= require(translation(Locale::German, TextId::WorkspaceEncounters) == "Begegnungen",
                  "German workspace translation is incorrect");
    ok &= require(translation(Locale::German, TextId::MenuBackToCampaigns) == "Zurück zu Kampagnen",
                  "German application menu translation is incorrect");
    ok &= require(translationKey(TextId::LiveWrapUp) == "live.wrap_up", "Stable translation key changed");
    ok &= require(translationKey(TextId::LiveStartSession) == "live.start_session",
                  "Stable live-session action key changed");
    ok &= require(translation(Locale::German, TextId::LiveDashboardTitle) == "Live-Sitzungsübersicht",
                  "German live dashboard title is incorrect");
    ok &= require(translation(Locale::German, TextId::LiveBreakPauseAudio) == "Pause starten + Audio anhalten",
                  "German break-with-audio action is incorrect");
    ok &= require(translation(Locale::German, TextId::LiveRemoveSessionNoteQuestion) ==
                      "Diese Notiz mit Zeitstempel entfernen?",
                  "German live-session removal prompt is incorrect");
    ok &= require(translation(Locale::German, TextId::LiveUnresolvedThreads) == "Offene Handlungsfäden",
                  "German recap field translation is incorrect");
    ok &= require(translation(Locale::German, TextId::LiveClockSystemStatus) ==
                      "Die Spieluhr folgt der lokalen Systemzeit.",
                  "German live clock status is incorrect");
    ok &= require(uiLiteralCatalogIsComplete(), "Specialist UI literal catalog is incomplete");
    ok &= require(uiLiteralCatalogSize() >= 1200, "Specialist UI literal coverage regressed unexpectedly");
    ok &= require(uiLiteralTranslation(Locale::German, "Next Action") == "Nächste Aktion",
                  "German responsive status-strip heading is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "Months / Calendar Shape") ==
                      "Monate / Kalenderstruktur",
                  "German calendar heading is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "Fix required fields before saving.") ==
                      "Behebe vor dem Speichern die Pflichtfelder.",
                  "German specialist validation copy is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "Map Creator") == "Karteneditor",
                  "German legacy workspace title is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "At the table") == "Am Spieltisch" &&
                      uiLiteralTranslation(Locale::German, "Player characters") == "Spielfiguren" &&
                      uiLiteralTranslation(Locale::German, "Creatures") == "Kreaturen" &&
                      uiLiteralTranslation(Locale::German, "Atlas context") == "Atlaskontext" &&
                      uiLiteralTranslation(Locale::German, "Prepared encounters") == "Vorbereitete Begegnungen" &&
                      uiLiteralTranslation(Locale::German, "Eligible maps") == "Geeignete Karten",
                  "German workspace context rail is incomplete");
    ok &= require(uiLiteralTranslation(Locale::German, "Sheet") == "Bogen" &&
                      uiLiteralTranslation(Locale::German, "Encounter map") == "Begegnungskarte" &&
                      uiLiteralTranslation(Locale::German, "Wilderness scout") == "Wildniskundschafter" &&
                      uiLiteralTranslation(Locale::German, "Retired sailor") == "Seemann im Ruhestand",
                  "German generated-character and encounter labels are incomplete");
    ok &= require(uiLiteralTranslation(Locale::German,
                      "Finish creating this player or discard the draft before saving the campaign.") !=
                      "Finish creating this player or discard the draft before saving the campaign.",
                  "German unfinished-player save guidance is missing");
    ok &= require(uiLiteralTranslation(Locale::German, "Review parent update") ==
                      "Aktualisierung der Oberkarte prüfen" &&
                      uiLiteralTranslation(Locale::German, "Please choose") == "Bitte wählen" &&
                      uiLiteralTranslation(Locale::German, "Use parent") == "Oberkarte übernehmen" &&
                      uiLiteralTranslation(Locale::German, "Keep local") == "Lokale Version behalten" &&
                      uiLiteralTranslation(Locale::German,
                          "Choose what should happen to every changed inherited map item. Nothing is applied until you confirm.") ==
                          "Lege für jedes geänderte geerbte Kartenelement fest, was geschehen soll. Bis zur Bestätigung wird nichts übernommen." &&
                      uiLiteralTranslation(Locale::English,
                          "Choose what should happen to every changed inherited map item. Nothing is applied until you confirm.") ==
                          "Choose what should happen to every changed inherited map item. Nothing is applied until you confirm.",
                  "German atlas rebase flow is incomplete");
    ok &= require(uiLiteralTranslation(Locale::German, "Reveal Fog") == "Nebel aufdecken" &&
                      uiLiteralTranslation(Locale::German, "Hide with Fog") == "Mit Nebel verbergen" &&
                      uiLiteralTranslation(Locale::German, "Ping Players") == "Spieler-Ping" &&
                      uiLiteralTranslation(Locale::German,
                          "Player ping shown for three seconds.") ==
                          "Der Spieler-Ping wird drei Sekunden lang angezeigt." &&
                      uiLiteralTranslation(Locale::German,
                          "Fog stroke cancelled. The map was restored; the player display remains blacked out.") ==
                          "Der Nebelstrich wurde verworfen und die Karte wiederhergestellt; die Spieleranzeige bleibt abgedunkelt.",
                  "German live map fog and ping workflow is incomplete");
    ok &= require(uiLiteralTranslation(Locale::German, "GM Publish Player Display") ==
                      "Spieleranzeige veröffentlichen" &&
                      uiLiteralTranslation(Locale::German, "GM view (private)") == "SL-Ansicht (privat)" &&
                      uiLiteralTranslation(Locale::German, "Player view (published)") ==
                          "Spieleransicht (veröffentlicht)" &&
                      uiLiteralTranslation(Locale::German, "Visibility check") == "Sichtbarkeitsprüfung",
                  "German GM/player publication review is incomplete");
    ok &= require(uiLiteralTranslation(Locale::German, "Presentation Publish Review") ==
                      "Veröffentlichung der Spieleranzeige prüfen" &&
                      uiLiteralTranslation(Locale::German, "Presentation Arm Review") ==
                          "Aktivierung der Spieleranzeige prüfen" &&
                      uiLiteralTranslation(Locale::German,
                          "Close this review without changing the player display.") ==
                          "Schließe diese Prüfung, ohne die Spieleranzeige zu verändern.",
                  "German Presentation Panel review is incomplete");
    ok &= require(uiLiteralTranslation(Locale::German, "Player display monitor") ==
                      "Bildschirm für Spieleranzeige" &&
                      uiLiteralTranslation(Locale::German, "Full-screen map on selected display") ==
                          "Karte im Vollbild auf ausgewähltem Bildschirm" &&
                      uiLiteralTranslation(Locale::German, "Follow GM map camera") ==
                          "SL-Kartenkamera folgen" &&
                      uiLiteralTranslation(Locale::German, "Reveal entire public map") ==
                          "Gesamte öffentliche Karte aufdecken" &&
                      uiLiteralTranslation(Locale::German, "Hide entire public map") ==
                          "Gesamte öffentliche Karte verbergen",
                  "German local player display window and fog controls are incomplete");
    ok &= require(uiLiteralTranslation(Locale::German,
                      "The selected public scene no longer exists. Choose another scene and review again.") !=
                      "The selected public scene no longer exists. Choose another scene and review again." &&
                      uiLiteralTranslation(Locale::German,
                      "The selected initiative source no longer exists. Choose another encounter and review again.") !=
                      "The selected initiative source no longer exists. Choose another encounter and review again." &&
                      uiLiteralTranslation(Locale::German,
                      "A revealed NPC no longer exists. Update the NPC reveals and review again.") !=
                      "A revealed NPC no longer exists. Update the NPC reveals and review again." &&
                      uiLiteralTranslation(Locale::German,
                      "A public handout no longer exists. Update the handout reveals and review again.") !=
                      "A public handout no longer exists. Update the handout reveals and review again.",
                  "German stale player-publication diagnostics are incomplete");
    ok &= require(uiLiteralTranslation(Locale::German, "Threat") == "Bedrohung",
                  "German specialist field label is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "Tree Trunk") == "Baumstamm",
                  "German map authoring choice is incorrect");
    const std::string_view mapCreatorDynamicTerms[] = {
        "Not calibrated",
        "Kilometers",
        "Meters",
        "Feet",
        "Last edit",
        "Restore unsaved changes",
        "Restore the recovery copy into this draft.",
        "Discard recovery",
        "Permanently discard this recovery copy.",
        "Discard the unsaved map recovery copy?",
        "Restore Map Recovery",
        "Import existing map image",
        "Copy a PNG or JPEG into this campaign and use it as an editable, traceable map reference.",
        "Imported images start GM-only. You decide explicitly when the original image is safe for players.",
        "Import editable map package",
        "Choose a .ttrpgmappkg file and review it before replacing this draft.",
        "Invalidate this request. A worker already running may finish, but its result will never be applied.",
        "Export map package",
        "Choose where to save a portable .ttrpgmappkg file.",
        "Import map package",
        "Choose a package and review its contents before replacing this draft.",
        "Legacy .ttrpgmap files omit parts of the V5 map model. Use them only for older projects; .ttrpgmappkg is the normal format.",
        "Export legacy .ttrpgmap",
        "Export an incomplete legacy-compatible draft.",
        "Import legacy .ttrpgmap",
        "Import an old map draft from the configured path. The current draft will be replaced.",
        "Import independent copy",
        "Package overview",
        "Package size",
        "Embedded asset size",
        "Editable content",
        "Package format",
        "Map generator",
        "Generation recipe",
        "Art style",
        "Player visibility",
        "Map visibility",
        "Atlas import effect",
        "Source atlas state",
        "Safety checks",
        "Ready to import. Package contents and embedded assets passed validation.",
        "Import blocked because the package contains conflicting internal identities.",
        "This package references a parent map. The imported independent copy will be detached from that parent.",
        "No conflicting internal identities were found.",
        "Conflicting internal identities must be repaired before this package can be imported.",
        "Player view review",
        "Review player view",
        "Prepare this saved map in blackout and open the fail-closed GM/player review.",
        "Open in World",
        "Save this exact version before preparing a player-safe review.",
        "Map saved. Review the player view before showing anything at the table.",
        "Save the current map before reviewing the player view.",
        "The player view could not be prepared. Nothing was shown to players.",
        "Player view prepared in blackout. Review it before arming the display.",
        "Save the current map before leaving this workflow.",
        "The saved map could not be opened in World.",
        "Finish or cancel tracing before continuing.",
        "Restore the previous map state.",
        "Undo a map edit before using redo.",
        "Reapply the map edit that was undone.",
        "Remove Generated Control Point",
        "Insert Generated Control Point",
        "No tokens, features, generated features, regions, or routes yet.",
        "Import Map Image",
        "Import map image",
        "Source size",
        "Review player publishing",
        "Review the original image before allowing it in player output.",
        "Hide original image from players",
        "Change Map Image Visibility",
        "Allow in player output",
        "Adopt Legacy Map Image",
        "Calibrate Map Image",
        "Apply image calibration",
        "Preview the crop, orientation, and optional distance scale here. The saved map changes only when you apply the settings.",
        "1. Choose the visible area",
        "Set how much of the displayed image to hide from each edge.",
        "Whole image",
        "Restore all four image edges in this preview.",
        "Left margin",
        "Right margin",
        "Top margin",
        "Bottom margin",
        "Crop preview",
        "Visible area: %.0f%% wide, %.0f%% high",
        "2. Orient the image",
        "Turn left 90 degrees",
        "Rotate this preview 90 degrees counter-clockwise.",
        "Turn right 90 degrees",
        "Rotate this preview 90 degrees clockwise.",
        "Reset rotation",
        "The image is already upright in this preview.",
        "Return the preview to zero degrees.",
        "No rotation",
        "Rotation preview: %+.0f degrees",
        "3. Optional distance scale",
        "Add a real-world size only when distances on this map should be measurable. Atlas links do not require it.",
        "Use real-world size",
        "Map width",
        "Map height",
        "The visible area must be at least 1% wide and high.",
        "Enter a positive width, height, and unit for the distance scale.",
        "No preview changes to apply.",
        "Preview only: Apply the settings to change the saved map.",
        "These settings match the saved map.",
        "Discard preview changes",
        "Restore the crop, rotation, and distance scale stored on the map.",
        "Apply crop and scale",
        "Apply this reviewed crop, orientation, and optional distance scale as one undoable edit.",
        "The latest recovery snapshot will be retried automatically.",
        "Map recovery snapshot saved after retry.",
        "Battle maps are created in Encounters so objectives, participants, start zones, and terrain stay together.",
        "Prepare battle map in Encounters",
        "Standalone tactical map (advanced)",
        "Create standalone tactical map",
        "Encounter preparation could not be opened.",
        "Enter or paste a complete .ttrpgmappkg path. This replaces the unavailable native file browser on this platform.",
        "Map package path",
        "Choose or enter a .ttrpgmappkg file and review it before replacing this draft.",
        "Enter a map package path before exporting.",
        "A file already exists at this manual path. Choose a new path before exporting.",
        "Enter a map package path before importing.",
        "Enter or paste the complete PNG destination. Existing files are never replaced without a native confirmation.",
        "PNG destination path",
        "Tiled PNG manifest path",
        "Enter a PNG destination path before exporting.",
        "A PNG already exists at this manual path. Choose a new path before exporting.",
        "Enter a tiled PNG manifest path before exporting.",
        "Remove image reference",
        "Remove the managed image from this map. Traced semantic features remain.",
        "Remove the image reference but keep traced features?"};
    for (const std::string_view term : mapCreatorDynamicTerms)
    {
        const std::string_view translated = uiLiteralTranslation(Locale::German, term);
        ok &= require(translated != term && isWellFormedUtf8(translated),
                      "German MapCreator dynamic copy is missing or malformed: " + std::string(term));
    }
    ok &= require(uiLiteralTranslation(Locale::German, "Must include") == "Muss enthalten" &&
                      uiLiteralTranslation(Locale::German, "Must avoid") == "Darf nicht enthalten" &&
                      uiLiteralTranslation(Locale::German, "Options") == "Optionen",
                  "German travel-map requirement labels are incomplete");
    ok &= require(uiLiteralTranslation(Locale::German, "Run the table") == "Spieltisch leiten" &&
                      uiLiteralTranslation(Locale::German, "Player display") == "Spieleranzeige" &&
                      uiLiteralTranslation(Locale::German, "Table audio") == "Audio am Spieltisch",
                  "German live-workspace cards are incomplete");
    ok &= require(uiLiteralTranslation(Locale::German, "Review staged material, player-safe output, audio, and session readiness.") ==
                      "Prüfe bereitgestelltes Material, die spielersichere Ausgabe, Audio und Sitzungsbereitschaft." &&
                      uiLiteralTranslation(Locale::German, "Player character") == "Spielfigur" &&
                      uiLiteralTranslation(Locale::German, "Story arc") == "Handlungsbogen",
                  "German stable-workspace copy is incomplete");
    ok &= require(uiLiteralTranslation(Locale::German, "Ruleset: %s") == "Regelwerk: %s" &&
                      uiLiteralTranslation(Locale::German, "Players") == "Spielfiguren" &&
                      uiLiteralTranslation(Locale::German, "Sessions") == "Sitzungen",
                  "German campaign-launcher summary copy is incomplete");
    ok &= require(uiLiteralTranslation(Locale::German, "Unsaved Campaign Changes") ==
                      "Ungespeicherte Kampagnenänderungen" &&
                      uiLiteralTranslation(Locale::German, "Save and Continue") == "Speichern und fortfahren" &&
                      uiLiteralTranslation(Locale::German, "Discard Changes") == "Änderungen verwerfen",
                  "German unsaved-changes dialog actions are incomplete");
    ok &= require(uiLiteralTranslation(Locale::German, "Exit GM Editor? Save this campaign first.") ==
                      "Möchtest du den GM Editor beenden? Speichere zuerst diese Kampagne." &&
                      uiLiteralTranslation(Locale::German, "Last save error: %s") == "Letzter Speicherfehler: %s" &&
                      uiLiteralTranslation(Locale::German, "Unknown save error.") == "Unbekannter Speicherfehler." &&
                      uiLiteralTranslation(Locale::German, "The campaign could not be saved. Open Settings > Project Health for details.") ==
                          "Die Kampagne konnte nicht gespeichert werden. Öffne Einstellungen > Projektstatus für Details.",
                  "German unsaved-changes dialog copy is incomplete");
    ok &= require(uiLiteralTranslation(Locale::German, "No project events yet") == "Noch keine Projektereignisse" &&
                      uiLiteralTranslation(Locale::German, "Saving...") == "Wird gespeichert …",
                  "German project-health status copy is incomplete");
    const std::pair<std::string_view, std::string_view> releaseCoreWorkflowTerms[] = {
        {"Apply Generated Design", "Erzeugten Entwurf anwenden"},
        {"Polished (best of 12)", "Poliert (bester aus 12)"},
        {"Tile:", "Feld:"},
        {"Way:", "Weg:"},
        {"cave network", "Höhlennetz"},
        {"The carved cave or dungeon floor is too small for the encounter.",
         "Die begehbare Höhlen- oder Dungeonfläche ist für die Begegnung zu klein."},
        {"Actor Template", "Figurenvorlage"},
        {"Generate NPC Draft", "NSC-Entwurf erzeugen"},
        {"Generate Monster Draft", "Monsterentwurf erzeugen"},
        {"Create NPC", "NSC erstellen"},
        {"Create Monster", "Monster erstellen"},
        {"<Missing Spell>", "<Fehlender Zauber>"},
        {"1 Identity", "1 Identität"},
        {"2 Species/Class", "2 Spezies/Klasse"},
        {"New Player Flow", "Neue Spielfigur"},
        {"Next: Abilities/HP", "Weiter: Attribute/TP"},
        {"Use Suggested HP", "Vorgeschlagene TP verwenden"},
        {"Create Player", "Spielfigur erstellen"},
        {"Unnamed player draft", "Unbenannter Spielfigurenentwurf"},
        {"Party Scan", "Gruppenüberblick"},
        {"Party Health", "Gruppengesundheit"},
        {"Carried Weight", "Getragenes Gewicht"},
        {"spell slots left", "Zauberplätze übrig"},
        {"Magic, no slots", "Magie, keine Zauberplätze"},
        {"<Missing Item>", "<Fehlender Gegenstand>"},
        {"No party map set", "Keine Gruppenkarte festgelegt"},
        {"World Actors", "Figuren der Welt"},
        {"<Unnamed Faction>", "<Unbenannte Fraktion>"},
        {"No live hooks", "Keine Live-Aufhänger"},
        {"Learn it before it can appear in combat.",
         "Erlerne den Zauber, bevor er im Kampf verfügbar ist."},
        {"NPC Directory", "NSC-Verzeichnis"},
        {"Filters the NPC directory without changing campaign data.",
         "Filtert das NSC-Verzeichnis, ohne Kampagnendaten zu verändern."},
        {"Add NPC Resource", "NSC-Ressource hinzufügen"},
        {"Cantrip", "Zaubertrick"},
        {"<No matching spell>", "<Kein passender Zauber>"},
        {"This NPC already knows the selected spell.", "Dieser NSC kennt den ausgewählten Zauber bereits."},
        {"%d of %d NPCs shown", "%d von %d NSC angezeigt"},
        {"%d of %d monsters shown", "%d von %d Monstern angezeigt"},
        {"/ %d max, %d left", "/ %d max., %d übrig"},
        {" – defeated", " – besiegt"},
        {"%d broken references", "%d fehlerhafte Verweise"},
        {"%d results", "%d Ergebnisse"},
        {"Player Directory", "Figurenverzeichnis"},
        {"Start with Summary; use focused tabs for editing, play, magic, inventory, story, and notes.",
         "Beginne mit der Zusammenfassung; nutze die Reiter für Bearbeitung, Spiel, Magie, Inventar, Handlung und Notizen."},
        {"No matching spells", "Keine passenden Zauber"},
        {"Save Notes", "Notizen speichern"},
        {"Character name is empty.", "Der Figurenname ist leer."},
        {"This spell no longer exists in the ruleset or campaign spell library.",
         "Dieser Zauber ist weder im Regelwerk noch in der Zauberbibliothek der Kampagne vorhanden."},
        {"Armor", "Rüstung"},
        {"Carried", "Getragen"},
        {"Saves", "Rettungswürfe"},
        {"Related", "Verknüpft"},
        {"Main Story", "Haupthandlung"},
        {"Planned", "Geplant"},
        {"Depends On", "Hängt ab von"},
        {"No story arcs yet. Create one to track a main plot, character arc, world event, mystery, or side quest.",
         "Noch keine Handlungsbögen. Erstelle einen, um Haupthandlung, Figurenbogen, Weltereignis, Mysterium oder Nebenquest zu verfolgen."},
        {"Delete this story arc and all of its scenes?", "Diesen Handlungsbogen mit allen zugehörigen Szenen löschen?"},
        {"Launch this scene as the active table moment?", "Diese Szene als aktiven Spielmoment starten?"},
        {"No %s targets match this search.", "Keine Ziele vom Typ %s entsprechen dieser Suche."},
        {"Links: maps %d, encounters %d, NPCs %d, items %d, audio %d",
         "Verknüpfungen: Karten %d, Begegnungen %d, NSC %d, Gegenstände %d, Audio %d"},
        {"%d of %d notes shown", "%d von %d Notizen angezeigt"},
        {"No rulesets are available.", "Es sind keine Regelwerke verfügbar."},
        {"Ability rows define formula symbols such as strength and strength.mod.",
         "Attributzeilen definieren Formelsymbole wie strength und strength.mod."},
        {"Selected audio is already playing.", "Das ausgewählte Audio wird bereits wiedergegeben."},
        {"No active audio channels to stop.", "Es gibt keine aktiven Audiokanäle zum Stoppen."},
        {"Selected file is missing. Play will ask you to relink it from the Audio Library.",
         "Die ausgewählte Datei fehlt. Beim Abspielen wirst du aufgefordert, sie über die Audiobibliothek neu zu verknüpfen."},
        {"Persistent campaign layout loaded.", "Dauerhaftes Kampagnenlayout geladen."},
        {"Campaign Saved", "Kampagne gespeichert"},
        {"Warning", "Warnung"},
    };
    for (const auto &[english, german] : releaseCoreWorkflowTerms)
        ok &= require(uiLiteralTranslation(Locale::German, english) == german,
                      "German release-core workflow terminology is incomplete for: " + std::string(english));
    const std::array<std::pair<std::string_view, std::string_view>, 12> releaseWorkspaceDetailTerms = {{
        {"Create Campaign", "Kampagne erstellen"},
        {"No arc selected", "Kein Handlungsbogen ausgewählt"},
        {"Add Scene", "Szene hinzufügen"},
        {"Duplicate Room", "Raum duplizieren"},
        {"Add Bookmark", "Lesezeichen hinzufügen"},
        {"Create Playlist", "Wiedergabeliste erstellen"},
        {"Pause All for Break", "Alles für Pause anhalten"},
        {"Selected Item: %s", "Ausgewählter Gegenstand: %s"},
        {"Make Change", "Wechselgeld bilden"},
        {"Award to Party", "An Gruppe vergeben"},
        {"Select or create a shop.", "Wähle oder erstelle einen Laden."},
        {"Use 0 for a cantrip, 1-9 for leveled spells.", "Verwende 0 für einen Zaubertrick und 1–9 für Zaubergrade."},
    }};
    for (const auto &[english, german] : releaseWorkspaceDetailTerms)
        ok &= require(uiLiteralTranslation(Locale::German, english) == german,
                      "German release workspace detail terminology is incomplete for: " + std::string(english));
    const std::array<std::pair<std::string_view, std::string_view>, 16> runtimeComposedUiTerms = {{
        {"Playing %d / paused %d", "%d läuft / %d pausiert"},
        {"Selected: %s", "Ausgewählt: %s"},
        {"No selected audio", "Kein Audio ausgewählt"},
        {"1 audio file missing; %d can be restored from original source.", "Eine Audiodatei fehlt; %d kann aus der Originalquelle wiederhergestellt werden."},
        {"%d audio files missing; %d can be restored from original source.", "%d Audiodateien fehlen; %d können aus der Originalquelle wiederhergestellt werden."},
        {"%d active / %d queued", "%d aktiv / %d in Warteschlange"},
        {"Playlist item is missing from campaign audio.", "Der Wiedergabelisteneintrag fehlt in den Kampagnen-Audiodateien."},
        {"Owner: %s | Selected: %s", "Besitzer: %s | Ausgewählt: %s"},
        {"Add %d %s to %s.", "%d× %s zu %s hinzufügen."},
        {"Source: %s | Target: %s | Item: %s", "Quelle: %s | Ziel: %s | Gegenstand: %s"},
        {"Owner: %s | Item: %s | Qty: %d", "Besitzer: %s | Gegenstand: %s | Menge: %d"},
        {"Move %d %s from %s to %s. Stackable items merge when possible.", "%d× %s von %s nach %s verschieben. Stapelbare Gegenstände werden nach Möglichkeit zusammengeführt."},
        {"Destination: Party Inventory | Selected item: %s", "Ziel: Gruppeninventar | Ausgewählter Gegenstand: %s"},
        {"%d pending, %d awarded. Non-empty pending parcels can be awarded.", "%d ausstehend, %d vergeben. Gefüllte ausstehende Pakete können vergeben werden."},
        {"%d items across %d entries", "%d Gegenstände in %d Einträgen"},
        {"Charges %d/%d", "Ladungen %d/%d"},
    }};
    for (const auto &[english, german] : runtimeComposedUiTerms)
        ok &= require(uiLiteralTranslation(Locale::German, english) == german,
                      "German runtime-composed UI terminology is incomplete for: " + std::string(english));
    const std::array<std::pair<std::string_view, std::string_view>, 18> atlasTerms = {{
        {"Atlas", "Atlas"},
        {"World", "Welt"},
        {"Region", "Region"},
        {"Site", "Schauplatz"},
        {"Tactical", "Taktisch"},
        {"Detail map", "Detailkarte"},
        {"Create detail map", "Detailkarte erstellen"},
        {"Open detail map", "Detailkarte öffnen"},
        {"One level up", "Eine Ebene nach oben"},
        {"Geography", "Geografie"},
        {"Nations", "Nationen"},
        {"Places", "Orte"},
        {"Routes", "Routen"},
        {"Detail maps", "Detailkarten"},
        {"Secrets", "Geheimnisse"},
        {"Current", "Aktuell"},
        {"Update available", "Aktualisierung verfügbar"},
        {"Conflict", "Konflikt"},
    }};
    for (const auto &[english, german] : atlasTerms)
        ok &= require(uiLiteralTranslation(Locale::German, english) == german,
                      "German atlas terminology is incomplete for: " + std::string(english));
    ok &= require(uiLiteralTranslation(Locale::German, "Unpositioned") == "Nicht positioniert",
                  "German atlas positioning status is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "Promote to atlas") == "In den Atlas übernehmen",
                  "German atlas promotion action is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "Parent update available") ==
                      "Aktualisierung der übergeordneten Karte verfügbar",
                  "German parent-map update status is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "Atlas synchronization") == "Atlas-Synchronisierung",
                  "German atlas synchronization heading is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "Detail level") == "Detailebene",
                  "German atlas detail-level label is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "Use tactical atlas map") == "Taktische Atlaskarte verwenden",
                  "German tactical-atlas encounter action is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "Players know it") == "Spielern bekannt" &&
                      uiLiteralTranslation(Locale::German, "Visible to players") == "Für Spieler sichtbar",
                  "German atlas visibility choices are incomplete");
    ok &= require(uiLiteralTranslation(Locale::German, "Map Hierarchy") == "Kartenhierarchie",
                  "German map hierarchy heading is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "Review Generation") == "Erzeugung prüfen",
                  "German generator review heading is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "Use automatic repair to shorten secondary roads.") ==
                      "Kürze Nebenstraßen mit der automatischen Reparatur.",
                  "German map quality repair action is incorrect");
    const std::array<std::pair<std::string_view, std::string_view>, 15> mapV5WorkflowTerms = {{
        {"1. Idea", "1. Idee"},
        {"2. Facts", "2. Fakten"},
        {"3. Designs", "3. Entwürfe"},
        {"5. Play & Export", "5. Spielen & Ausgeben"},
        {"Map size", "Kartengröße"},
        {"Review facts", "Fakten prüfen"},
        {"Valid and ready to edit", "Gültig und bereit zur Bearbeitung"},
        {"No valid design yet", "Noch kein gültiger Entwurf"},
        {"Required check failed", "Pflichtprüfung fehlgeschlagen"},
        {"Try new designs", "Neue Entwürfe versuchen"},
        {"The required great river does not yet connect all named nations to a valid outlet.",
         "Der geforderte große Fluss verbindet noch nicht alle benannten Nationen mit einem gültigen Abfluss."},
        {"Recommended for most campaign maps.",
         "Für die meisten Kampagnenkarten empfohlen."},
        {"A required fact is not satisfied.",
         "Ein verbindlicher Fakt ist nicht erfüllt."},
        {"The same element is both required and excluded.",
         "Dasselbe Element ist gleichzeitig vorgeschrieben und ausgeschlossen."},
        {"Regenerate the shared river and keep nation and capital facts locked.",
         "Erzeuge den gemeinsamen Fluss neu und behalte Nationen und Hauptstadt als feste Vorgaben bei."},
    }};
    for (const auto &[english, german] : mapV5WorkflowTerms)
        ok &= require(uiLiteralTranslation(Locale::German, english) == german,
                      "German map V5 workflow copy is incomplete for: " + std::string(english));
    const std::array<std::pair<std::string_view, std::string_view>, 8> mapV5EditSafetyTerms = {{
        {"Regenerate part of map", "Kartenteil neu erzeugen"},
        {"Feature family", "Elementfamilie"},
        {"Only inside a selected rectangle", "Nur innerhalb eines ausgewählten Rechtecks"},
        {"Change preview ready", "Änderungsvorschau bereit"},
        {"Map checkpoints", "Kartenstände"},
        {"Create checkpoint", "Kartenstand anlegen"},
        {"Restore map checkpoint", "Kartenstand wiederherstellen"},
        {"A preferred fact was adjusted to keep the map valid.",
         "Ein Wunsch wurde angepasst, damit die Karte gültig bleibt."},
    }};
    for (const auto &[english, german] : mapV5EditSafetyTerms)
        ok &= require(uiLiteralTranslation(Locale::German, english) == german,
                      "German map V5 safe-edit terminology is incomplete for: " + std::string(english));
    const std::array<std::pair<std::string_view, std::string_view>, 23> mapV5FeatureProfileTerms = {{
        {"Classification", "Klassifikation"},
        {"Display profile", "Darstellungsprofil"},
        {"Current map style", "Aktueller Kartenstil"},
        {"Dirt road", "Erdstraße"},
        {"Gravel road", "Schotterstraße"},
        {"Narrow trail", "Schmaler Pfad"},
        {"Creek", "Bach"},
        {"Deep river", "Tiefer Fluss"},
        {"Light wooden bridge", "Leichte Holzbrücke"},
        {"Heavy wooden bridge", "Massive Holzbrücke"},
        {"Rough stone bridge", "Grobe Steinbrücke"},
        {"Dressed stone bridge", "Behauene Steinbrücke"},
        {"Ford", "Furt"},
        {"Pier", "Steg"},
        {"Town", "Kleinstadt"},
        {"Village", "Dorf"},
        {"Fortress", "Festung"},
        {"Ruin", "Ruine"},
        {"Barricade", "Barrikade"},
        {"Style default", "Stilvorgabe"},
        {"Change Generated Feature Classification", "Klassifikation eines erzeugten Elements ändern"},
        {"The feature classification was updated.", "Die Klassifikation des Elements wurde geändert."},
        {"The feature display profile was updated.", "Das Darstellungsprofil des Elements wurde geändert."},
    }};
    for (const auto &[english, german] : mapV5FeatureProfileTerms)
        ok &= require(uiLiteralTranslation(Locale::German, english) == german,
                      "German generated-feature profile terminology is incomplete for: " + std::string(english));
    const std::array<std::pair<std::string_view, std::string_view>, 29> mapV5PlacePromotionTerms = {{
        {"Campaign world", "Kampagnenwelt"},
        {"Generated settlements and landmarks remain map features until you add the ones you want to the campaign world.",
         "Erzeugte Siedlungen und Landmarken bleiben Kartenelemente, bis du die gewünschten Orte in die Kampagnenwelt übernimmst."},
        {"Unnamed settlement", "Unbenannte Siedlung"},
        {"Unnamed landmark", "Unbenannte Landmarke"},
        {"Add to campaign world", "Zur Kampagnenwelt hinzufügen"},
        {"Added to campaign world", "Zur Kampagnenwelt hinzugefügt"},
        {"Create a linked campaign place and save it with this campaign.",
         "Erstellt einen verknüpften Kampagnenort und speichert ihn mit dieser Kampagne."},
        {"Add Generated Place to Campaign", "Erzeugten Ort zur Kampagne hinzufügen"},
        {"The generated place was added to the campaign world and saved.",
         "Der erzeugte Ort wurde zur Kampagnenwelt hinzugefügt und gespeichert."},
        {"Could not add the generated place to the campaign world:",
         "Der erzeugte Ort konnte nicht zur Kampagnenwelt hinzugefügt werden:"},
        {"The campaign place was saved but could not be found in the current campaign.",
         "Der Kampagnenort wurde gespeichert, konnte aber in der aktuellen Kampagne nicht gefunden werden."},
        {"Could not undo the campaign-world transfer:",
         "Die Übernahme in die Kampagnenwelt konnte nicht rückgängig gemacht werden:"},
        {"Could not redo the campaign-world transfer:",
         "Die Übernahme in die Kampagnenwelt konnte nicht wiederholt werden:"},
        {"The generated place no longer exists on this map.",
         "Der erzeugte Ort ist auf dieser Karte nicht mehr vorhanden."},
        {"Only generated settlements and landmarks can be added to the campaign world.",
         "Nur erzeugte Siedlungen und Landmarken können zur Kampagnenwelt hinzugefügt werden."},
        {"Name this generated place before adding it to the campaign world.",
         "Benenne diesen erzeugten Ort, bevor du ihn zur Kampagnenwelt hinzufügst."},
        {"This generated place has no usable map position.",
         "Dieser erzeugte Ort besitzt keine verwendbare Kartenposition."},
        {"Already in the campaign world.", "Bereits in der Kampagnenwelt vorhanden."},
        {"Save this map before adding its generated places to the campaign world.",
         "Speichere diese Karte, bevor du ihre erzeugten Orte zur Kampagnenwelt hinzufügst."},
        {"Save the latest map changes before adding this generated place.",
         "Speichere die letzten Kartenänderungen, bevor du diesen erzeugten Ort hinzufügst."},
        {"Ready to add to the campaign world.", "Bereit zum Hinzufügen zur Kampagnenwelt."},
        {"Generated place added to the campaign world.",
         "Der erzeugte Ort wurde zur Kampagnenwelt hinzugefügt."},
        {"The transferred campaign place no longer exists.",
         "Der übernommene Kampagnenort ist nicht mehr vorhanden."},
        {"The transferred campaign place was edited and cannot be removed by map undo.",
         "Der übernommene Kampagnenort wurde bearbeitet und kann nicht über Karten-Rückgängig entfernt werden."},
        {"The generated place transfer was undone.",
         "Die Übernahme des erzeugten Orts wurde rückgängig gemacht."},
        {"The generated place transfer has incomplete identity data.",
         "Die Übernahme des erzeugten Orts besitzt unvollständige Identitätsdaten."},
        {"A campaign place already uses this generated map feature.",
         "Ein Kampagnenort verwendet dieses erzeugte Kartenelement bereits."},
        {"The generated map place changed and cannot be restored by redo.",
         "Der erzeugte Kartenort wurde geändert und kann beim Wiederholen nicht wiederhergestellt werden."},
        {"The generated place transfer was restored.",
         "Die Übernahme des erzeugten Orts wurde wiederhergestellt."},
    }};
    for (const auto &[english, german] : mapV5PlacePromotionTerms)
    {
        ok &= require(uiLiteralTranslation(Locale::German, english) == german,
                      "German generated-place promotion copy is incomplete for: " + std::string(english));
        ok &= require(uiSystemMessageTranslation(Locale::German, english) == german,
                      "German generated-place promotion system copy is incomplete for: " + std::string(english));
    }
    const std::array<std::pair<std::string_view, std::string_view>, 8> playerMapReleaseTerms = {{
        {"Player map content", "Karteninhalte für Spieler"},
        {"Political borders", "Politische Grenzen"},
        {"Roads and routes", "Straßen und Routen"},
        {"Places and markers", "Orte und Markierungen"},
        {"Names and labels", "Namen und Beschriftungen"},
        {"Tokens and annotations", "Token und Anmerkungen"},
        {"Terrain, coasts, water, approved background art, and fog remain part of the base player map. Optional campaign knowledge stays hidden until enabled here.",
         "Gelände, Küsten, Wasser, freigegebene Hintergrundbilder und Nebel bleiben Teil der grundlegenden Spielerkarte. Optionale Kampagneninformationen bleiben verborgen, bis du sie hier freigibst."},
        {"Changing a category activates Blackout and requires a new player review.",
         "Das Ändern einer Kategorie aktiviert den Blackout und erfordert eine neue Spielerprüfung."},
    }};
    for (const auto &[english, german] : playerMapReleaseTerms)
        ok &= require(uiLiteralTranslation(Locale::German, english) == german,
                      "German player map release copy is incomplete for: " + std::string(english));
    const std::array<std::pair<std::string_view, std::string_view>, 12> mapV5PngExportTerms = {{
        {"PNG output", "PNG-Ausgabe"},
        {"Player-safe copy", "Spielersichere Ausgabe"},
        {"Game Master copy", "SL-Ausgabe"},
        {"Output size", "Ausgabegröße"},
        {"Screen size", "Bildschirmgröße"},
        {"100 px per tactical cell", "100 px pro Taktikfeld"},
        {"Visible layers", "Sichtbare Ebenen"},
        {"Political borders", "Politische Grenzen"},
        {"Imported image background", "Importierte Bildunterlage"},
        {"Apply fog", "Nebel anwenden"},
        {"PNG export detail must be between 8 and 128 pixels per cell.",
         "Der PNG-Detailgrad muss zwischen 8 und 128 Pixeln pro Feld liegen."},
        {"PNG export would exceed the maximum edge of 16384 pixels.",
         "Der PNG-Export würde die maximale Kantenlänge von 16384 Pixeln überschreiten."},
    }};
    for (const auto &[english, german] : mapV5PngExportTerms)
        ok &= require(uiLiteralTranslation(Locale::German, english) == german,
                      "German map V5 PNG export copy is incomplete for: " + std::string(english));
    const std::array<std::pair<std::string_view, std::string_view>, 48> mapWorkflowTerms = {{
        {"Empty", "Leer"},
        {"Bridges, planks, crossings, or elevated passages.",
         "Brücken, Planken, Übergänge oder erhöhte Passagen."},
        {"Export .ttrpgmap", "Als .ttrpgmap exportieren"},
        {"4 to 80 cells per side; applied when you continue.",
         "4 bis 80 Felder pro Seite; wird beim Fortfahren angewendet."},
        {"Tile: %s, brush %d", "Feld: %s, Pinsel %d"},
        {"Clear Area", "Bereichsauswahl aufheben"},
        {"%s / %s mi (%d%%)", "%s / %s Meilen (%d%%)"},
        {"%s pace, watch %d, %d rations, %d checks", "Tempo %s, Wache %d, %d Rationen, %d Prüfungen"},
        {"Party %s", "Gruppe %s"},
        {"%s - %s", "%s – %s"},
        {"%s - Note: %s", "%s – Notiz: %s"},
        {" (GM)", " (SL)"},
        {" (hidden)", " (ausgeblendet)"},
        {" hidden", " verborgen"},
        {"Generate three deterministic local scenes without changing the current map.",
         "Erzeuge drei reproduzierbare lokale Szenen, ohne die aktuelle Karte zu verändern."},
        {"Generated Cave Route", "Erzeugte Höhlenroute"},
        {"The child map dimensions must be positive.",
         "Die Abmessungen der Detailkarte müssen positiv sein."},
        {"Inspect Terrain", "Gelände untersuchen"},
        {"Battlefield Setup", "Schlachtfeldeinstellungen"},
        {"%s, brush %d", "%s, Pinsel %d"},
        {"Open the Atlas and create a Tactical detail from a Region or Site first.",
         "Öffne zuerst den Atlas und erstelle aus einer Region oder einem Schauplatz eine taktische Detailkarte."},
        {"Battle Map", "Kampfkarte"},
        {"Chokepoint", "Engstelle"},
        {"At least two meaningful routes between the teams are required.",
         "Zwischen den Teams sind mindestens zwei sinnvolle Routen erforderlich."},
        {"Both the upper and lower tactical routes must remain usable.",
         "Sowohl die obere als auch die untere taktische Route müssen nutzbar bleiben."},
        {"%s | wave %d", "%s | Welle %d"},
        {"%s | init %d", "%s | Ini %d"},
        {"%s | start %d,%d", "%s | Start %d,%d"},
        {"%s | hidden", "%s | verborgen"},
        {"%s | friendly", "%s | freundlich"},
        {"%s | round %d", "%s | Runde %d"},
        {"%s HP, AC %d", "%s TP, RK %d"},
        {"Table Display", "Spieltischanzeige"},
        {"Public now: %s | %s | %s | NPCs %d | Handouts %d",
         "Jetzt öffentlich: %s | %s | %s | NSC %d | Handouts %d"},
        {"Only selected public scene, map, initiative, NPCs, handouts, and shared art are sent to the local player display.",
         "Nur die ausgewählte öffentliche Szene, Karte, Initiative, NSC, Handouts und geteilten Bilder werden an die lokale Spieleranzeige gesendet."},
        {"Presentation-safe preview", "Spielersichere Vorschau"},
        {"Scene: %s | Map: %s | Initiative: %s", "Szene: %s | Karte: %s | Initiative: %s"},
        {"GM notes, secrets, hidden tokens, private stats, and unrevealed links remain filtered.",
         "SL-Notizen, Geheimnisse, verborgene Marker, private Werte und nicht freigegebene Verknüpfungen bleiben herausgefiltert."},
        {"Round %d", "Runde %d"},
        {"River branch", "Flussarm"},
        {"East-West Street", "Ost-West-Straße"},
        {"North-South Street", "Nord-Süd-Straße"},
        {"District Landmark", "Bezirkslandmarke"},
        {"Evaluating candidate", "Entwurf wird bewertet"},
        {"Queued", "Eingereiht"},
        {"Generating", "Wird erzeugt"},
        {"Map generation failed after", "Kartenerzeugung fehlgeschlagen nach"},
        {"Tactical Route", "Taktische Route"},
    }};
    for (const auto &[english, german] : mapWorkflowTerms)
        ok &= require(uiLiteralTranslation(Locale::German, english) == german,
                      "German map workflow terminology is incomplete for: " + std::string(english));
    const std::array<std::pair<std::string_view, std::string_view>, 29> liveDynamicTerms = {{
        {"Gregorian Calendar", "Gregorianischer Kalender"},
        {"Campaign Calendar", "Kampagnenkalender"},
        {"January", "Januar"},
        {"March", "März"},
        {"Monday", "Montag"},
        {"Thursday", "Donnerstag"},
        {"Day", "Tag"},
        {"Twilight", "Dämmerung"},
        {"Unnamed player", "Unbenannte Spielfigur"},
        {"Unnamed combatant", "Unbenannter Kampfteilnehmer"},
        {"Expires now", "Läuft jetzt ab"},
        {"rounds", "Runden"},
        {"Check concentration on damage", "Bei Schaden Konzentration prüfen"},
        {"[Session State] Break started.", "[Sitzungsstatus] Pause begonnen."},
        {"[Game Time] Advanced by ", "[Spielzeit] Vorgestellt um "},
        {" Previous: ", " Zuvor: "},
        {" Note: ", " Notiz: "},
        {"Session break resumed", "Sitzung nach Pause fortgesetzt"},
        {"Game time set", "Spielzeit gesetzt"},
        {"Session Report", "Sitzungsbericht"},
        {"Started:", "Begonnen:"},
        {"Unresolved Threads", "Offene Handlungsfäden"},
        {"Timestamped Notes", "Notizen mit Zeitstempel"},
        {"Prep / Wrap-up Tools", "Vorbereitungs- und Abschlusswerkzeuge"},
        {" Audio channels paused: ", " Angehaltene Audiokanäle: "},
        {" Audio resume requested; no paused channels changed.", " Audio-Fortsetzung angefordert; keine pausierten Kanäle wurden geändert."},
        {"character", "Spielfigur"},
        {"nation_territory", "Staatsgebiet"},
        {"story_beat", "Szene"},
    }};
    for (const auto &[english, german] : liveDynamicTerms)
        ok &= require(uiLiteralTranslation(Locale::German, english) == german,
                      "German live dynamic terminology is incomplete for: " + std::string(english));
    const std::pair<std::string_view, std::string_view> battleTrackerTerms[] = {
        {"Combat command center", "Kampfzentrale"},
        {"Continue Combat", "Kampf fortsetzen"},
        {"Set Up Combat", "Kampf einrichten"},
        {"Edit Encounter Preparation", "Begegnungsvorbereitung bearbeiten"},
        {"Combat running", "Kampf läuft"},
        {"Compact layout: combat controls and battlefield are stacked.", "Kompakte Ansicht: Kampfsteuerung und Schlachtfeld stehen untereinander."},
        {"Active Turn", "Aktiver Zug"},
        {"Action Resolution", "Aktionsabwicklung"},
        {"Load a battlefield, then add combatants.", "Lade ein Schlachtfeld und füge dann Kampfteilnehmer hinzu."},
        {"Active: {0} ({1}/{2} HP)", "Aktiv: {0} ({1}/{2} TP)"},
        {"movement {0}/{1} ft, {2} ft left", "Bewegung {0}/{1} Fuß, {2} Fuß verbleiben"},
        {"{0} conditions, {1} effects, {2} slots left, {3} resources left, {4} ft movement.",
         "{0} Zustände, {1} Effekte, {2} Plätze übrig, {3} Ressourcen übrig, {4} Fuß Bewegung."},
        {"{0} playing / {1} paused", "{0} laufen / {1} pausiert"},
        {"Action Step {0}", "Aktionsschritt {0}"},
        {"Reviewed: {0} modifiers", "Geprüft: {0} Modifikatoren"},
        {"One or more selected targets are out of range.", "Mindestens eines der gewählten Ziele ist außer Reichweite."},
        {"Combat phase set to {0}.", "Kampfphase auf {0} gesetzt."},
        {"{0} skipped their turn.", "{0} überspringt den Zug."},
        {"{0} is out of range for {1}.", "{0} ist für {1} außer Reichweite."},
        {"{0} resolves against {1} target(s) with {2}: {3} hit.",
         "{0} wird gegen {1} Ziel(e) mit {2} abgewickelt: {3} Treffer."},
        {"{0} takes {1} {2} damage and is defeated.", "{0} erleidet {1} {2}-Schaden und ist besiegt."},
        {"{0} is concentrating and should make a Constitution save DC {1}.",
         "{0} konzentriert sich und sollte einen Konstitutionsrettungswurf gegen SG {1} ablegen."},
        {"Level {0} slots left: {1}.", "Zauberplätze des {0}. Grades übrig: {1}."},
        {"{0} casts {1} as a ritual targeting {2}. {3}", "{0} wirkt {1} als Ritual auf {2}. {3}"},
        {"{0} succeeds on {1} against {2} DC {3} with {4}.", "{0} besteht den {1} gegen {2}, SG {3}, mit {4}."},
        {"Heal {0} HP ({1} -> {2})", "Heilung {0} TP ({1} -> {2})"},
        {"{0} reaction reviewed: {1}.", "Reaktion von {0} geprüft: {1}."},
        {"{0} uses left: {1}/{2}.", "{0} Anwendungen übrig: {1}/{2}."},
        {"{0} narrative position set to {1}.", "Narrative Position von {0} auf {1} gesetzt."},
        {"Engaged with {0}", "Im Nahkampf mit {0}"},
        {"Encounter: {0}", "Begegnung: {0}"},
        {"Start of turn effects for {0}: ", "Effekte zu Beginn des Zuges von {0}: "},
        {"{0} moved from ({1}, {2}) to ({3}, {4}).", "{0} wurde von ({1}, {2}) nach ({3}, {4}) bewegt."},
        {"{0} left {1}'s reach. Consider an opportunity attack or reaction.",
         "{0} hat die Reichweite von {1} verlassen. Erwäge einen Gelegenheitsangriff oder eine Reaktion."},
        {"Reaction's linked resource is missing.", "Die mit der Reaktion verknüpfte Ressource fehlt."},
        {"Damage resistance applied.", "Schadensresistenz angewendet."},
    };
    for (const auto &[english, german] : battleTrackerTerms)
        ok &= require(uiLiteralTranslation(Locale::German, english) == german,
                      "German battle-tracker dynamic terminology is incomplete for: " + std::string(english));
    ok &= require(uiLiteralTranslation(
                      Locale::German,
                      "Prep Checklist\n- Review previous recap\n- Confirm active hooks\n- Prepare opening scene\n- Check maps and encounters\n- Queue audio\n- Note player spotlight moments")
                      .find("Vorbereitungscheckliste") == 0,
                  "German live-session template body is not localized");
    ok &= require(uiLiteralTranslation(Locale::German, "Export the current draft map to the configured path.") ==
                      "Exportiere den aktuellen Kartenentwurf in den festgelegten Pfad.",
                  "German map export help is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "Paint walls, ledges, doors, cover, and other tactical edges.") ==
                      "Male Wände, Absätze, Türen, Deckung und andere taktische Kanten.",
                  "German tactical-tool help is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "No character is selected.") == "Kein Charakter ausgewählt.",
                  "German empty-state copy is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "Start a live session") == "Live-Sitzung starten",
                  "German campaign-home action title is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "Build the party first") == "Zuerst die Gruppe erstellen",
                  "German campaign-home empty state is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "Nothing is currently equipped.") ==
                      "Derzeit ist nichts ausgerüstet.",
                  "German inventory empty state is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "Unnamed encounter") == "Unbenannte Begegnung",
                  "German live encounter fallback is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "Remove this item from this inventory.") ==
                      "Entferne diesen Gegenstand aus diesem Inventar.",
                  "German inventory action help is incorrect");
    ok &= require(uiLiteralTranslation(Locale::German, "User-authored title") == "User-authored title",
                  "Unknown/user-authored content must pass through unchanged");
    ok &= require(uiLiteralTranslation(Locale::English, "Months") == "Months",
                  "English specialist literal should remain unchanged");
    for (std::size_t index = 0; index < static_cast<std::size_t>(TextId::Count); ++index)
    {
        ok &= require(isWellFormedUtf8(translation(Locale::English, static_cast<TextId>(index))),
                      "English catalog contains malformed UTF-8");
        ok &= require(isWellFormedUtf8(translation(Locale::German, static_cast<TextId>(index))),
                      "German catalog contains malformed UTF-8");
    }
    const std::string_view germanGlyphProbe = uiLiteralTranslation(Locale::German, "Next Action");
    const std::string_view germanValidationProbe = uiLiteralTranslation(Locale::German, "Fix required fields before saving.");
    const std::string_view germanAtlasGlyphProbe =
        uiLiteralTranslation(Locale::German, "Parent update available");
    ok &= require(isWellFormedUtf8(germanGlyphProbe) && germanGlyphProbe.find("ä") != std::string_view::npos,
                  "German specialist heading lost its umlaut glyph");
    ok &= require(isWellFormedUtf8(germanValidationProbe),
                  "German specialist validation contains malformed UTF-8");
    ok &= require(isWellFormedUtf8(germanAtlasGlyphProbe) &&
                      germanAtlasGlyphProbe.find("ü") != std::string_view::npos,
                  "German atlas terminology lost its umlaut glyph");

    // Ruleset services deliberately keep stable, English diagnostics for logs
    // and persistence. These regressions verify that every UI-facing branch
    // formats the structured output at the presentation boundary.
    using gm::rulesets::formatBackendError;
    using gm::rulesets::formatFormulaError;
    using gm::rulesets::formatDiagnosticCode;
    using gm::rulesets::formatUpgradeChange;
    using gm::rulesets::formatValidationDiagnostic;
    ok &= require(formatUpgradeChange(
                      {RulesetUpgradeChangeKind::Added, RulesetDefinitionKind::Ability, "strength"},
                      Locale::German) == "Attribut hinzugefügt: strength",
                  "German ruleset Added change was not localized");
    ok &= require(formatUpgradeChange(
                      {RulesetUpgradeChangeKind::Removed, RulesetDefinitionKind::Resource, "focus"},
                      Locale::German) == "Ressource entfernt: focus",
                  "German ruleset Removed change was not localized");
    ok &= require(formatUpgradeChange(
                      {RulesetUpgradeChangeKind::Changed, RulesetDefinitionKind::DerivedStat, "armor_class"},
                      Locale::German) == "Abgeleiteter Wert geändert: armor_class",
                  "German ruleset Changed change was not localized");
    ok &= require(formatUpgradeChange(
                      {RulesetUpgradeChangeKind::Added, RulesetDefinitionKind::Ability, "strength"},
                      Locale::English) == "Added ability: strength",
                  "English ruleset Added change changed unexpectedly");
    ok &= require(formatUpgradeChange(
                      {RulesetUpgradeChangeKind::Removed, RulesetDefinitionKind::Resource, "focus"},
                      Locale::English) == "Removed resource: focus" &&
                      formatUpgradeChange(
                          {RulesetUpgradeChangeKind::Changed, RulesetDefinitionKind::DerivedStat, "armor_class"},
                          Locale::English) == "Changed derived stat: armor_class",
                  "English ruleset Removed/Changed changes changed unexpectedly");
    ok &= require(gm::rulesets::formatLegacyUpgradeChange(
                      "Removed skill/check: stealth", Locale::German) ==
                      "Fertigkeit/Probe entfernt: stealth",
                  "Legacy ruleset diffs are not localized safely");

    const RulesetFormulaEvaluationResult dividedByZero =
        evaluateRulesetFormula("10 / zero", {{"zero", 0.0}});
    ok &= require(!dividedByZero.valid && !dividedByZero.errors.empty() &&
                      formatFormulaError(dividedByZero.errors.front(), Locale::German) == "Division durch null.",
                  "German ruleset formula evaluation error was not localized");
    ok &= require(formatFormulaError("Unknown formula variable: agility.", Locale::German) ==
                      "Unbekannte Formelvariable: agility.",
                  "German unknown formula variable was not localized");
    ok &= require(formatFormulaError("Unknown formula variable: agility.", Locale::English) ==
                      "Unknown formula variable: agility.",
                  "English formula error changed unexpectedly");

    RulesetDefinitionModel invalidRuleset;
    invalidRuleset.abilityScores.push_back({"strength", "Strength", "STR", {}});
    invalidRuleset.skills.push_back({"perception", "Perception", "wisdom", {}});
    invalidRuleset.derivedStats.push_back({"guard", "Guard", "level +", {}});
    const std::vector<RulesetValidationDiagnostic> rulesetDiagnostics =
        validateRulesetDefinition(invalidRuleset);
    bool localizedMissingReference = false;
    bool localizedBrokenFormula = false;
    for (const RulesetValidationDiagnostic &diagnostic : rulesetDiagnostics)
    {
        const std::string localized = formatValidationDiagnostic(diagnostic, Locale::German);
        if (diagnostic.messageKey == RulesetDiagnosticMessageKey::MissingReference)
            localizedMissingReference = localized == "Fehlende Referenz auf Attribut: wisdom.";
        if (diagnostic.messageKey == RulesetDiagnosticMessageKey::BrokenFormula)
            localizedBrokenFormula = localized.find("erwartet") != std::string::npos;
    }
    ok &= require(localizedMissingReference,
                  "German missing ruleset reference diagnostic was not localized from structured data");
    ok &= require(localizedBrokenFormula,
                  "German broken formula diagnostic was not localized from structured data");

    ok &= require(formatBackendError("Ruleset ID is required.", Locale::German) ==
                      "Eine Regelwerk-ID ist erforderlich." &&
                      formatBackendError("Version is required.", Locale::German) ==
                          "Eine Version ist erforderlich.",
                  "German ruleset publish errors were not localized");
    ok &= require(formatDiagnosticCode("missing_reference", Locale::German) == "Fehlende Referenz" &&
                      formatDiagnosticCode("missing_reference", Locale::English) == "Missing reference",
                  "Ruleset validator issue labels are not localized");
    ok &= require(formatBackendError("Ruleset ID is required.", Locale::English) ==
                      "Ruleset ID is required.",
                  "English ruleset publish error changed unexpectedly");
    ok &= require(formatBackendError(
                      "skills.perception: Missing ability score reference: wisdom.", Locale::German) ==
                      "Fertigkeiten › perception: Fehlende Referenz auf Attribut: wisdom.",
                  "German composite migration/validator error was not localized");
    ok &= require(formatBackendError(
                      "derivedStats.guard: Unknown formula variable: agility.", Locale::German) ==
                      "Abgeleitete Werte › guard: Unbekannte Formelvariable: agility.",
                  "German composite runtime formula error was not localized");
    ok &= require(formatBackendError(
                      "Campaigns can only pin published ruleset versions.", Locale::German) ==
                      "Kampagnen können nur veröffentlichte Regelwerk-Versionen verwenden.",
                  "German ruleset migration error was not localized");
    ok &= require(formatBackendError("Database detail from user plugin", Locale::German) ==
                      "Database detail from user plugin",
                  "Unknown ruleset backend details must pass through unchanged");

    ok &= require(parseLocalePreference("de-DE") == LocalePreference::German, "German locale parsing failed");
    ok &= require(parseLocalePreference("EN_us") == LocalePreference::English, "English locale parsing failed");
    ok &= require(parseLocalePreference("  Deutsch  ") == LocalePreference::German, "Trimmed locale parsing failed");
    ok &= require(parseLocalePreference("auto") == LocalePreference::System, "System locale parsing failed");
    ok &= require(!parseLocalePreference("klingon").has_value(), "Unsupported locale should not parse");

    ok &= require(formatInteger(1234567, Locale::English) == "1,234,567", "English integer formatting failed");
    ok &= require(formatInteger(-1234567, Locale::German) == "-1.234.567", "German integer formatting failed");
    ok &= require(formatNumber(1234.5, 2, Locale::English) == "1,234.50", "English decimal formatting failed");
    ok &= require(formatNumber(1234.5, 2, Locale::German) == "1.234,50", "German decimal formatting failed");
    ok &= require(formatPercent(0.125, 1, Locale::English) == "12.5%", "English percent formatting failed");
    ok &= require(formatPercent(0.125, 1, Locale::German) == "12,5 %", "German percent formatting failed");
    ok &= require(formatDuration(std::chrono::seconds(3725), Locale::English) == "1 hour, 2 minutes, 5 seconds",
                  "English duration formatting failed");
    ok &= require(formatDuration(std::chrono::seconds(3725), Locale::German) == "1 Stunde, 2 Minuten, 5 Sekunden",
                  "German duration formatting failed");
    ok &= require(formatDuration(std::chrono::seconds(-60), Locale::German, true) == "−1 Min",
                  "Compact negative duration formatting failed");

    const std::filesystem::path root = std::filesystem::current_path() / "localization_test_output";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);
    const std::filesystem::path settingsPath = root / "GMEditor.settings";

    {
        std::ofstream initial(settingsPath, std::ios::binary | std::ios::trunc);
        initial << "# Existing settings\ntext_scale=1.25\nlanguage=en\n";
    }

    std::string error;
    ok &= require(LocalizationSettingsStore::save(settingsPath, LocalePreference::German, &error),
                  "Could not persist German preference: " + error);
    const LocaleSettingsLoadResult loaded = LocalizationSettingsStore::load(settingsPath);
    ok &= require(loaded.succeeded() && loaded.fileFound && loaded.valueFound &&
                      loaded.preference == LocalePreference::German,
                  "Persisted German preference did not reload");
    {
        std::ifstream saved(settingsPath, std::ios::binary);
        const std::string contents((std::istreambuf_iterator<char>(saved)), std::istreambuf_iterator<char>());
        ok &= require(contents.find("text_scale=1.25") != std::string::npos,
                      "Saving language discarded unrelated settings");
        ok &= require(contents.find("language=de") != std::string::npos, "German preference was not written");
    }
    ok &= require(!std::filesystem::exists(settingsPath.string() + ".tmp"), "Temporary settings file was left behind");
    ok &= require(!std::filesystem::exists(settingsPath.string() + ".bak"), "Backup settings file was left behind");

    const std::filesystem::path accessibilityPath = root / "GMEditor-accessibility.settings";
    {
        std::ofstream initial(accessibilityPath, std::ios::binary | std::ios::trunc);
        initial << "# Shared application settings\n"
                   "language=de\n"
                   "custom_plugin_value=keep-me\n"
                   "\n"
                   "language=en\n"
                   "text_scale=1.25\n"
                   "high_contrast=yes\n"
                   "compact_tables=0\n"
                   "always_show_focus=off\n";
    }

    using gm::settings::AccessibilitySettings;
    using gm::settings::AccessibilitySettingsLoadResult;
    using gm::settings::AccessibilitySettingsStore;
    const AccessibilitySettingsLoadResult accessibilityLoaded =
        AccessibilitySettingsStore::load(accessibilityPath);
    ok &= require(accessibilityLoaded.succeeded() && accessibilityLoaded.fileFound &&
                      accessibilityLoaded.textScaleFound && accessibilityLoaded.highContrastFound &&
                      accessibilityLoaded.compactTablesFound && accessibilityLoaded.alwaysShowFocusFound,
                  "Accessibility settings were not fully loaded");
    ok &= require(std::abs(accessibilityLoaded.settings.textScale - 1.25f) < 0.001f &&
                       accessibilityLoaded.settings.highContrast &&
                       !accessibilityLoaded.settings.compactTables &&
                       !accessibilityLoaded.settings.alwaysShowFocus,
                  "Accessibility settings reloaded with incorrect values");

    AccessibilitySettings accessibilityChanged;
    accessibilityChanged.textScale = 0.92f;
    accessibilityChanged.highContrast = false;
    accessibilityChanged.compactTables = true;
    accessibilityChanged.alwaysShowFocus = true;
    ok &= require(AccessibilitySettingsStore::save(accessibilityPath, accessibilityChanged, &error),
                  "Could not persist accessibility settings: " + error);
    std::string accessibilityContents;
    {
        std::ifstream saved(accessibilityPath, std::ios::binary);
        accessibilityContents.assign(std::istreambuf_iterator<char>(saved), std::istreambuf_iterator<char>());
    }
    ok &= require(accessibilityContents.find("language=de") != std::string::npos &&
                      accessibilityContents.find("language=en") != std::string::npos,
                  "Saving accessibility settings discarded or rewrote a language line");
    ok &= require(accessibilityContents.find("custom_plugin_value=keep-me") != std::string::npos &&
                      accessibilityContents.find("\n\n") != std::string::npos,
                  "Saving accessibility settings discarded an unknown setting or blank line");
    ok &= require(accessibilityContents.find("text_scale=0.92") != std::string::npos &&
                      accessibilityContents.find("high_contrast=false") != std::string::npos &&
                      accessibilityContents.find("compact_tables=true") != std::string::npos &&
                      accessibilityContents.find("always_show_focus=true") != std::string::npos,
                  "Accessibility settings were not written in their canonical form");

    ok &= require(LocalizationSettingsStore::save(accessibilityPath, LocalePreference::German, &error),
                  "Language store could not update the shared settings file: " + error);
    const AccessibilitySettingsLoadResult afterLanguageSave =
        AccessibilitySettingsStore::load(accessibilityPath);
    ok &= require(afterLanguageSave.succeeded() &&
                       std::abs(afterLanguageSave.settings.textScale - 0.92f) < 0.001f &&
                       !afterLanguageSave.settings.highContrast && afterLanguageSave.settings.compactTables &&
                       afterLanguageSave.settings.alwaysShowFocus,
                  "Language persistence discarded accessibility settings");
    ok &= require(LocalizationSettingsStore::load(accessibilityPath).preference == LocalePreference::German,
                  "Accessibility persistence made the shared language setting unreadable");

    accessibilityChanged.textScale = 1.50f;
    ok &= require(AccessibilitySettingsStore::save(accessibilityPath, accessibilityChanged, &error),
                  "The supported 150 percent text scale must persist: " + error);
    const AccessibilitySettingsLoadResult maximumScaleReloaded =
        AccessibilitySettingsStore::load(accessibilityPath);
    ok &= require(maximumScaleReloaded.succeeded() && maximumScaleReloaded.textScaleFound &&
                      std::abs(maximumScaleReloaded.settings.textScale - 1.50f) < 0.001f,
                  "The 150 percent text scale must reload without reverting to the default");
    ok &= require(LocalizationSettingsStore::save(accessibilityPath, LocalePreference::English, &error),
                  "Language changes must remain writable at 150 percent text scale: " + error);
    const AccessibilitySettingsLoadResult maximumScaleAfterLanguageSave =
        AccessibilitySettingsStore::load(accessibilityPath);
    ok &= require(maximumScaleAfterLanguageSave.succeeded() &&
                      std::abs(maximumScaleAfterLanguageSave.settings.textScale - 1.50f) < 0.001f,
                  "Updating another preference must preserve the 150 percent text scale");

    std::string beforeRejectedSave;
    {
        std::ifstream saved(accessibilityPath, std::ios::binary);
        beforeRejectedSave.assign(std::istreambuf_iterator<char>(saved), std::istreambuf_iterator<char>());
    }
    AccessibilitySettings unsupportedAccessibility = accessibilityChanged;
    for (float unsupportedScale : {0.89f, 1.51f})
    {
        unsupportedAccessibility.textScale = unsupportedScale;
        ok &= require(!AccessibilitySettingsStore::save(accessibilityPath, unsupportedAccessibility, &error) &&
                          !error.empty(),
                      "Text scales immediately outside the supported bounds must be rejected");
    }
    std::string afterRejectedSave;
    {
        std::ifstream saved(accessibilityPath, std::ios::binary);
        afterRejectedSave.assign(std::istreambuf_iterator<char>(saved), std::istreambuf_iterator<char>());
    }
    ok &= require(afterRejectedSave == beforeRejectedSave,
                  "Rejected accessibility settings changed the shared settings file");
    ok &= require(!std::filesystem::exists(accessibilityPath.string() + ".tmp") &&
                      !std::filesystem::exists(accessibilityPath.string() + ".bak"),
                  "Accessibility persistence left a temporary or backup file behind");

    const auto invalidAccessibilityPath = root / "GMEditor-accessibility-invalid.settings";
    for (const char *unsupportedScale : {"0.89", "1.51"})
    {
        {
            std::ofstream invalidSettings(invalidAccessibilityPath, std::ios::binary | std::ios::trunc);
            invalidSettings << "text_scale=" << unsupportedScale << '\n';
        }
        const auto invalidAccessibility = AccessibilitySettingsStore::load(invalidAccessibilityPath);
        ok &= require(!invalidAccessibility.succeeded() && !invalidAccessibility.textScaleFound &&
                          invalidAccessibility.settings.textScale == 1.0f,
                      "Out-of-range persisted text scales must report an error and retain the safe default");
    }

    LocalizationService &service = LocalizationService::instance();
    ok &= require(service.initialize(settingsPath, &error), "Localization service initialization failed: " + error);
    const std::uint64_t germanRevision = service.revision();
    ok &= require(service.locale() == Locale::German, "Localization service did not activate German");
    ok &= require(std::string(service.text(TextId::CommonSave)) == "Speichern", "Service returned wrong German text");
    ok &= require(std::string(service.uiText("Delete Item")) == "Gegenstand löschen",
                  "Service returned wrong specialist UI text");
    ok &= require(std::string(service.uiText("A user-authored chronicle")) == "A user-authored chronicle",
                  "Service changed unknown/user-authored content");
    const std::pair<const char *, const char *> mapV5GermanTerms[] = {
        {"Choose what should happen to every changed inherited map item. Nothing is applied until you confirm.",
         "Lege für jedes geänderte geerbte Kartenelement fest, was geschehen soll. Bis zur Bestätigung wird nichts übernommen."},
        {"Compare inherited map content before changing this detail map.",
         "Vergleiche geerbte Karteninhalte, bevor diese Detailkarte geändert wird."},
        {"Unnamed region", "Unbenanntes Gebiet"},
        {"Unnamed route", "Unbenannte Route"},
        {"Preview legend", "Vorschaulegende"},
        {"The small previews hide labels so you can compare the overall shape. Select one to inspect its names and details below.",
         "Die kleinen Vorschauen blenden Beschriftungen aus, damit du die Gesamtform vergleichen kannst. Wähle einen Entwurf aus, um darunter Namen und Details zu prüfen."},
        {"Ready to edit", "Bereit zum Bearbeiten"},
        {"Ready · %d review notes", "Bereit · %d Prüfhinweise"},
        {"Best at: %s", "Besonders stark: %s"},
        {"Fit the complete design inside the preview.", "Passt den vollständigen Entwurf in die Vorschau ein."},
        {"Zoom out", "Verkleinern"},
        {"Zoom in", "Vergrößern"},
        {"Zoom: %d%%", "Zoom: %d%%"},
        {"Show or hide place and region names in this preview only.",
         "Blendet Orts- und Gebietsnamen nur in dieser Vorschau ein oder aus."},
        {"Show or hide nation borders in this preview only.",
         "Blendet Nationsgrenzen nur in dieser Vorschau ein oder aus."},
        {"Drag the map to move it. Use the mouse wheel or the zoom buttons to inspect labels and details.",
         "Ziehe die Karte, um sie zu verschieben. Nutze das Mausrad oder die Zoomtasten, um Beschriftungen und Details zu prüfen."},
        {"Labels are dense at this overview. Zoom in to read names, or hide labels while comparing the composition.",
         "Die Beschriftungen liegen in dieser Übersicht dicht beieinander. Vergrößere die Karte zum Lesen oder blende Beschriftungen beim Vergleich der Komposition aus."},
        {"Ready to edit with review notes", "Bereit zum Bearbeiten mit Prüfhinweisen"},
        {"Review notes", "Hinweise prüfen"},
        {"Cancel generation", "Erzeugung abbrechen"},
        {"Checking deterministic designs...", "Reproduzierbare Entwürfe werden geprüft …"},
        {"Detail area selected", "Detailbereich ausgewählt"},
        {"Editable map package", "Editierbares Kartenpaket"},
        {"Review Map Package", "Kartenpaket prüfen"},
        {"Map image reference", "Kartenbild-Referenz"},
        {"Crop, rotate, and calibrate", "Zuschneiden, drehen und kalibrieren"},
        {"Safe default: the original image is hidden from players.",
         "Sichere Voreinstellung: Das Originalbild ist für Spieler verborgen."},
        {"Unsaved map changes from the last editing session are available.",
         "Ungespeicherte Kartenänderungen aus der letzten Bearbeitungssitzung sind verfügbar."},
        {"Map image imported as a managed GM-only reference. Calibrate it and add map features in Edit.",
         "Das Kartenbild wurde als verwaltete, nur für die Spielleitung sichtbare Referenz importiert. Kalibriere es und zeichne Kartenelemente unter „Bearbeiten“ nach."},
        {"Repair map image", "Kartenbild reparieren"},
        {"Map image integrity verified.", "Die Integrität des Kartenbildes wurde bestätigt."},
        {"The original map image is missing. Choose a replacement to repair it.",
         "Das ursprüngliche Kartenbild fehlt. Wähle ein Ersatzbild, um die Referenz zu reparieren."},
        {"The managed map image is missing or cannot be read. Repair the image reference before exporting.",
         "Das verwaltete Kartenbild fehlt oder kann nicht gelesen werden. Repariere die Bildreferenz vor dem Export."},
        {"The published map image is unavailable. The GM can repair it in Map Edit.",
         "Das freigegebene Kartenbild ist nicht verfügbar. Die Spielleitung kann es in der Kartenbearbeitung reparieren."},
        {"map.import.unsupported_format", "Dieses Bildformat wird nicht unterstützt. Verwende PNG oder JPEG."},
        {"map.import.source_changed", "Die Kartenbilddatei wurde während des Imports verändert. Bitte versuche es erneut."},
        {"map.import.proxy_resize_failed", "Die Arbeitsvorschau des Kartenbilds konnte nicht verkleinert werden."},
        {"map.import.invalid_unit", "Wähle für eine kalibrierte Karte eine gültige Entfernungseinheit."},
        {"map.import.trace_degenerate", "Das nachgezeichnete Kartenelement besitzt keine gültige Linie oder Fläche."},
        {"map.import.trace_too_many_points", "Das nachgezeichnete Kartenelement besitzt zu viele Punkte."},
        {"map.import.legacy_reference_missing", "Diese Karte besitzt kein unverwaltetes altes Hintergrundbild."}};
    for (const auto &term : mapV5GermanTerms)
        ok &= require(std::string(service.uiText(term.first)) == term.second,
                      std::string("Missing German Map V5 UI translation: ") + term.first);
    ok &= require(service.systemMessage("Started live session: Harbour watch") == "Live-Sitzung gestartet: Harbour watch",
                  "A successful session start must preserve its title rather than become an unknown-error toast");
    ok &= require(gm::localization::uiSystemMessageTranslation(gm::localization::Locale::English,
                      "Started live session: Harbour watch") == "Started live session: Harbour watch",
                  "English successful session-start notification changed");
    ok &= require(service.systemMessage("").empty(), "Empty notification body became a technical failure");
    ok &= require(service.systemMessage("unmapped_backend_failure_for_regression") == "Unbekannter technischer Fehler.",
                  "Successful lifecycle translation suppressed an actual unknown-error fallback");
    ok &= require(service.systemMessage("Ended live session: Harbour watch") == "Live-Sitzung beendet: Harbour watch",
                  "A successful session completion must preserve the title and must not become an unknown-error toast");
    ok &= require(service.systemMessage("Could not read asset: background.png") ==
                      "Asset konnte nicht gelesen werden: background.png",
                  "German system-message translation did not preserve a safe dynamic suffix");
    const std::pair<const char *, const char *> atlasSystemMessages[] = {
        {"The rebased raster has an invalid cell count.",
         "Das abgeglichene Raster besitzt eine ungültige Feldanzahl."},
        {"The child raster was incomplete and will be initialized during rebase.",
         "Das Raster der Detailkarte war unvollständig und wird beim Abgleich neu aufgebaut."},
        {"The child contains duplicate inherited feature IDs: inherited-road",
         "Die Detailkarte enthält doppelte Kennungen geerbter Elemente: inherited-road"},
        {"The child contains duplicate inherited region IDs: inherited-region",
         "Die Detailkarte enthält doppelte Kennungen geerbter Gebiete: inherited-region"},
        {"The child contains duplicate inherited route IDs: inherited-route",
         "Die Detailkarte enthält doppelte Kennungen geerbter Wege: inherited-route"},
        {"The child contains duplicate local feature IDs: local-road",
         "Die Detailkarte enthält doppelte Kennungen lokaler Elemente: local-road"},
        {"The parent extraction produced a duplicate feature ID: river-main",
         "Der Ausschnitt der Oberkarte erzeugte eine doppelte Elementkennung: river-main"},
        {"The parent extraction produced a duplicate region ID: west-march",
         "Der Ausschnitt der Oberkarte erzeugte eine doppelte Gebietskennung: west-march"},
        {"The parent extraction produced a duplicate route ID: trade-road",
         "Der Ausschnitt der Oberkarte erzeugte eine doppelte Wegkennung: trade-road"},
        {"An inherited feature ID conflicts with local child content: city-gate",
         "Eine geerbte Elementkennung steht im Konflikt mit lokalem Inhalt der Detailkarte: city-gate"},
        {"An inherited region ID conflicts with local child content: west-march",
         "Eine geerbte Gebietskennung steht im Konflikt mit lokalem Inhalt der Detailkarte: west-march"},
        {"An inherited route ID conflicts with local child content: trade-road",
         "Eine geerbte Wegkennung steht im Konflikt mit lokalem Inhalt der Detailkarte: trade-road"},
        {"The child contains duplicate local region IDs: west-march",
         "Die Detailkarte enthält doppelte Kennungen lokaler Gebiete: west-march"},
        {"The child contains duplicate local route IDs: trade-road",
         "Die Detailkarte enthält doppelte Kennungen lokaler Wege: trade-road"},
        {"A child region has no stable ID.",
         "Ein Gebiet der Detailkarte besitzt keine stabile Kennung."},
        {"A child route has no stable ID.",
         "Ein Weg der Detailkarte besitzt keine stabile Kennung."},
        {"An inherited region has no stable source ID: west-march",
         "Ein geerbtes Gebiet besitzt keine stabile Quellenkennung: west-march"},
        {"An inherited route has no stable source ID: trade-road",
         "Ein geerbter Weg besitzt keine stabile Quellenkennung: trade-road"},
        {"An inherited region is absent from the rebase decision contract: west-march",
         "Ein geerbtes Gebiet fehlt im Entscheidungsvertrag des Atlasabgleichs: west-march"},
        {"An inherited route is absent from the rebase decision contract: trade-road",
         "Ein geerbter Weg fehlt im Entscheidungsvertrag des Atlasabgleichs: trade-road"},
        {"An inherited region conflicts with local child content: west-march",
         "Ein geerbtes Gebiet steht im Konflikt mit lokalem Inhalt der Detailkarte: west-march"},
        {"An inherited route conflicts with local child content: trade-road",
         "Ein geerbter Weg steht im Konflikt mit lokalem Inhalt der Detailkarte: trade-road"},
        {"The rebase contains a decision for an unknown or unchanged item: unknown-feature",
         "Der Atlasabgleich enthält eine Entscheidung für ein unbekanntes oder unverändertes Element: unknown-feature"},
        {"The rebase contains duplicate decisions for: river-main",
         "Der Atlasabgleich enthält doppelte Entscheidungen für: river-main"},
        {"A changed atlas feature has no rebase decision: river-main",
         "Für ein geändertes Atlaselement fehlt eine Abgleichentscheidung: river-main"},
        {"A changed atlas item has no rebase decision: region::west-march",
         "Für einen geänderten Atlasinhalt fehlt eine Abgleichentscheidung: region::west-march"},
        {"All extracted segments of one parent feature require the same rebase decision: river-main",
         "Alle übernommenen Abschnitte desselben Elements der Oberkarte benötigen dieselbe Abgleichentscheidung: river-main"}};
    for (const auto &message : atlasSystemMessages)
    {
        ok &= require(uiSystemMessageTranslation(Locale::German, message.first) == message.second,
                      std::string("German atlas service message was not localized: ") + message.first);
        ok &= require(uiSystemMessageTranslation(Locale::English, message.first) == message.first,
                      std::string("English atlas service message changed unexpectedly: ") + message.first);
    }
    ok &= require(service.systemMessage("Uncatalogued internal exception text") ==
                      "Unbekannter technischer Fehler.",
                  "Unknown English system diagnostics must fail closed in the German UI");
    ok &= require(service.format(TextId::LauncherResumeAvailable, {"Kampf"}) == "Fortsetzen verfügbar: Kampf",
                  "German placeholder formatting failed");
    ok &= require(service.format(TextId::LivePhaseTools, {"Kampf"}) == "Werkzeuge: Kampf",
                  "German live phase placeholder formatting failed");
    ok &= require(service.format(TextId::LiveSceneLaunched, {"Finale", "2"}) ==
                      "Finale gestartet. Gestartete Audios: 2.",
                  "German live scene status formatting failed");
    ok &= require(service.format(TextId::LiveAudioPausedForBreak, {"3"}) ==
                      "Für die Pause angehaltene Audiokanäle: 3.",
                  "German live audio status formatting failed");
    ok &= require(service.quantity(TextId::TimeMinuteSingular, TextId::TimeMinutePlural, 2) == "2 Minuten",
                  "German quantity formatting failed");
    ok &= require(service.setPreference(LocalePreference::English, true, &error),
                  "Could not switch to English: " + error);
    ok &= require(service.locale() == Locale::English && service.revision() > germanRevision,
                  "Live locale switch did not update service revision");
    ok &= require(std::string(service.text(TextId::CommonSave)) == "Save", "Service returned wrong English text");
    ok &= require(service.systemMessage("Uncatalogued internal exception text") ==
                      "Uncatalogued internal exception text",
                  "English system diagnostics should remain available for troubleshooting");
    ok &= require(LocalizationSettingsStore::load(settingsPath).preference == LocalePreference::English,
                  "Live locale switch was not persisted");

    {
        std::ofstream invalid(settingsPath, std::ios::binary | std::ios::trunc);
        invalid << "language=unsupported\n";
    }
    const LocaleSettingsLoadResult invalid = LocalizationSettingsStore::load(settingsPath);
    ok &= require(!invalid.succeeded(), "Invalid persisted language should produce a load error");

    std::filesystem::remove_all(root, cleanupError);
    if (!ok)
        return 1;

    std::cout << "Localization tests passed\n";
    return 0;
}
