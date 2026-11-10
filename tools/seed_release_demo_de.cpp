#include "sqlite3.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
constexpr const char *kCampaignId = "campaign_current_feature_tour";
constexpr const char *kCampaignName = "Das Tor unter dem Hafen";
constexpr const char *kRevision = "release-demo-de-0.9-v1";
constexpr char kFieldSeparator = '\x1f';
constexpr char kEntrySeparator = '\x1e';

struct FieldValue
{
    std::string column;
    std::string value;
};

struct RowTranslation
{
    std::string table;
    std::string stableId;
    std::vector<FieldValue> fields;
};

std::string quote(const std::string &value)
{
    std::string escaped = "'";
    for (const char c : value)
        escaped += c == '\'' ? "''" : std::string(1, c);
    escaped += "'";
    return escaped;
}

std::string identifier(const std::string &value)
{
    if (value.empty() || !std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_';
        }))
        throw std::runtime_error("Unsafe SQL identifier: " + value);
    return "\"" + value + "\"";
}

void exec(sqlite3 *db, const std::string &sql)
{
    char *error = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK)
    {
        const std::string message = error ? error : "unknown sqlite error";
        sqlite3_free(error);
        throw std::runtime_error(message + "\nSQL:\n" + sql);
    }
}

std::string packed(const std::vector<std::string> &fields)
{
    std::string result;
    for (std::size_t index = 0; index < fields.size(); ++index)
    {
        if (index > 0)
            result += kFieldSeparator;
        result += fields[index];
    }
    return result;
}

std::string packedEntries(const std::vector<std::string> &entries)
{
    std::string result;
    for (const std::string &entry : entries)
    {
        result += entry;
        result += kEntrySeparator;
    }
    return result;
}

std::string inventory(std::initializer_list<std::string> entries)
{
    std::string result;
    for (const std::string &entry : entries)
        result += entry + ";";
    return result;
}

std::string action(const std::string &name,
                   int reach,
                   int range,
                   const std::string &attack,
                   const std::string &damage,
                   const std::string &notes = {})
{
    return packed({name, std::to_string(reach), std::to_string(range), attack, damage, notes});
}

std::string actions(std::initializer_list<std::string> entries)
{
    return packedEntries(std::vector<std::string>(entries));
}

std::string spellbook(std::initializer_list<std::string> ids)
{
    std::vector<std::string> entries;
    for (const std::string &id : ids)
        entries.push_back(packed({id, "1", "Kampagne", "Für die deutsche Demo vorbereitet."}));
    return packedEntries(entries);
}

std::string mapRegion(const std::string &id,
                      const std::string &name,
                      const std::string &description,
                      int x,
                      int y,
                      int width,
                      int height,
                      int movementCost,
                      int lighting,
                      bool difficult,
                      bool blocksLineOfSight,
                      bool gmOnly)
{
    return packed({id, name, description, std::to_string(x), std::to_string(y),
                   std::to_string(width), std::to_string(height), std::to_string(movementCost),
                   std::to_string(lighting), difficult ? "1" : "0", blocksLineOfSight ? "1" : "0",
                   gmOnly ? "1" : "0", "datetime('now')", "datetime('now')"});
}

std::string mapFeature(const std::string &id,
                       int kind,
                       const std::string &name,
                       const std::string &description,
                       const std::string &gmNotes,
                       const std::string &linkedEntityId,
                       const std::string &targetMapId,
                       int x,
                       int y,
                       bool discovered,
                       bool gmOnly,
                       int difficultyClass)
{
    return packed({id, std::to_string(kind), name, description, gmNotes, linkedEntityId, targetMapId,
                   std::to_string(x), std::to_string(y), discovered ? "1" : "0", gmOnly ? "1" : "0",
                   std::to_string(difficultyClass), "datetime('now')", "datetime('now')"});
}

std::string mapRoute(const std::string &id,
                     const std::string &name,
                     const std::string &fromMapId,
                     const std::string &toMapId,
                     int startX,
                     int startY,
                     int endX,
                     int endY,
                     const std::string &distanceMiles,
                     const std::string &terrain,
                     const std::string &notes,
                     bool discovered)
{
    return packed({id, name, fromMapId, toMapId, std::to_string(startX), std::to_string(startY),
                   std::to_string(endX), std::to_string(endY), distanceMiles, terrain, notes,
                   discovered ? "1" : "0", "datetime('now')", "datetime('now')"});
}

std::string roomConnection(const std::string &id,
                           const std::string &targetRoomId,
                           const std::string &label,
                           bool locked,
                           bool secret,
                           bool discovered)
{
    return packed({id, targetRoomId, label, locked ? "1" : "0", secret ? "1" : "0",
                   discovered ? "1" : "0", "datetime('now')", "datetime('now')"});
}

std::string dicePreset(const std::string &id,
                       const std::string &name,
                       const std::string &expression,
                       const std::string &notes)
{
    return packed({id, name, expression, notes, "datetime('now')", "datetime('now')"});
}

std::string combatLog(const std::string &id,
                      int round,
                      const std::string &actor,
                      const std::string &eventType,
                      const std::string &summary)
{
    return packed({id, std::to_string(round), actor, eventType, summary,
                   "datetime('now')", "datetime('now')"});
}

std::string activeEffect(const std::string &id,
                         int kind,
                         const std::string &name,
                         const std::string &sourceName,
                         const std::string &sourceEntityId,
                         const std::string &sourceSpellId,
                         const std::string &notes,
                         int rounds,
                         bool concentration)
{
    return packed({id, std::to_string(kind), name, sourceName, sourceEntityId, sourceSpellId, notes,
                   std::to_string(rounds), concentration ? "1" : "0", "1",
                   "datetime('now')", "datetime('now')"});
}

void add(std::vector<RowTranslation> &rows,
         const std::string &table,
         const std::string &stableId,
         std::initializer_list<std::pair<std::string, std::string>> fields)
{
    RowTranslation row;
    row.table = table;
    row.stableId = stableId;
    for (const auto &field : fields)
        row.fields.push_back({field.first, field.second});
    rows.push_back(std::move(row));
}

std::vector<RowTranslation> germanTranslations()
{
    std::vector<RowTranslation> rows;

    add(rows, "campaigns", kCampaignId, {
        {"name", kCampaignName},
        {"description", "Unter den regennassen Kais antwortet ein uraltes Tor auf eine gestohlene Obsidianklinge. Vier Gefährten müssen zwischen Hafenwache, Schmugglern und dem Gläsernen Chor entscheiden, wem die Wahrheit über den Riss anvertraut werden darf."},
        {"tags", "deutsche-demo;release-0.9;hafen;fantasy;"},
        {"party_vehicles", "Schoner Seekarte;Signalboot am Kai;"},
        {"party_strongholds", "Wachtturm im Hafenviertel;Leuchtturm-Anbau;"},
        {"party_reputation", "Bekannt als die Gruppe, die das Tor unter dem Hafen geschlossen hat."},
        {"travel_route_name", "Vom Hafenviertel zum Archivgewölbe"},
        {"travel_pace", "Vorsichtig"},
        {"travel_notes", "Diese Route dient der Reiseverwaltung, bevor die Begegnung im Archiv beginnt."},
    });

    add(rows, "inventories", "inv_party_current_feature_tour", {{"name", "Gruppeninventar"}});
    add(rows, "inventory_entries", "party_inv_entry_1", {{"slot", "Bandolier"}, {"state_notes", "Gemeinsam am Hafenkai gekauft."}});
    add(rows, "inventory_entries", "party_inv_entry_2", {{"slot", "Gürtel"}, {"state_notes", "In einer verborgenen Tasche verwahrt."}});
    add(rows, "inventory_entries", "party_inv_entry_3", {{"slot", "Tasche"}, {"state_notes", "Gemeinsame Kampagnennotizen."}});

    add(rows, "custom_items", "item_starforged_rapier", {
        {"name", "Sternengeschmiedetes Rapier"}, {"description", "Eine schlanke Duellwaffe, deren Klinge nahe planarer Schwellen glimmt."},
        {"category", "Waffe"}, {"item_type", "Rapier"}, {"weapon_damage", "1d8 Stichschaden"},
        {"effects", "Leuchtet in der Nähe planarer Tore;"}});
    add(rows, "custom_items", "item_moonlit_shield", {
        {"name", "Mondlichtschild"}, {"description", "Silberne Runen ziehen sich über diesen leichten Schild und sammeln das Licht des Mondes."},
        {"category", "Rüstung"}, {"item_type", "Schild"}, {"effects", "Spendet auf Befehl schwaches Licht;"}});
    add(rows, "custom_items", "item_field_journal", {
        {"name", "Cassians Feldtagebuch"}, {"description", "Ein mit persönlichen Handlungsbögen verknüpfter Gegenstand."},
        {"category", "Handlungsgegenstand"}, {"item_type", "Buch"}, {"effects", "Enthält verschlüsselte Expeditionsnotizen;"}});
    add(rows, "custom_items", "item_healing_tonic", {
        {"name", "Karmesinroter Heiltrank"}, {"description", "Ein kräftiger Trank aus Rotblatt, Salz und einem Tropfen geweihten Wassers."},
        {"category", "Verbrauchsgegenstand"}, {"item_type", "Trank"}, {"effects", "Stellt nach dem Trinken manuell Trefferpunkte wieder her;"}});
    add(rows, "custom_items", "item_gate_key", {
        {"name", "Obsidianschlüssel"}, {"description", "Der Schlüssel zum versiegelten Tor unter dem Hafen."},
        {"category", "Handlungsgegenstand"}, {"item_type", "Schlüssel"}, {"effects", "Öffnet eine versiegelte Schwelle unter dem Hafen;"}});
    add(rows, "custom_items", "item_charting_chalk", {
        {"name", "Kartografenkreide"}, {"description", "Verzauberte Kreide für Wegmarkierungen und Bodenpläne."},
        {"category", "Verbrauchsgegenstand"}, {"item_type", "Werkzeug"}, {"effects", "Markiert Wege und Geometrie deutlich;"}});
    add(rows, "custom_items", "item_tideglass_focus", {
        {"name", "Flutglas-Fokus"}, {"description", "Eine Mondsteinscherbe für rituelle Zauberei."},
        {"category", "Magischer Fokus"}, {"item_type", "Wundersamer Gegenstand"}, {"effects", "Gewährt Einblicke in planare Störungen;"}});

    add(rows, "custom_spells", "spell_ember_lance", {
        {"name", "Glutlanze"}, {"casting_time", "1 Aktion"}, {"spell_range", "90 Fuß"}, {"duration", "Sofort"},
        {"classes", "Magier, Zauberer"}, {"damage", "1d10 Feuerschaden"}, {"area", "Eine Kreatur"},
        {"effect_summary", "Eine glühende Lanze trifft eine Kreatur in Reichweite."},
        {"scaling", "+1d10 Feuerschaden je höherem Zauberplatz"}, {"description", "Der Zauber bündelt Herdenglut zu einem einzigen geraden Geschoss."}});
    add(rows, "custom_spells", "spell_warding_chorus", {
        {"name", "Schutzchor"}, {"casting_time", "1 Aktion"}, {"spell_range", "30 Fuß"}, {"duration", "1 Minute"},
        {"material_description", "eine silberne Stimmgabel"}, {"classes", "Barde, Kleriker"},
        {"saving_throw_ability", "Weisheit"}, {"area", "Aura mit 15 Fuß Radius"},
        {"effect_summary", "Kreaturen im Bereich müssen einen Rettungswurf bestehen oder verlieren Schwung."},
        {"scaling", "Der Radius steigt je höherem Zauberplatz um 5 Fuß."},
        {"description", "Mehrstimmige Schutzverse brechen den Schwung naher Feinde."}});
    add(rows, "custom_spells", "spell_menders_light", {
        {"name", "Licht der Heilerin"}, {"casting_time", "1 Aktion"}, {"spell_range", "Berührung"}, {"duration", "Sofort"},
        {"classes", "Kleriker, Druide"}, {"healing", "1d8 + Attribut"}, {"area", "Eine Kreatur"},
        {"effect_summary", "Stellt Trefferpunkte wieder her."}, {"scaling", "+1d8 Heilung je höherem Zauberplatz"},
        {"description", "Warmes Licht schließt Wunden und lindert den Schmerz einer berührten Kreatur."}});
    add(rows, "custom_spells", "spell_echo_step", {
        {"name", "Echoschritt"}, {"casting_time", "1 Bonusaktion"}, {"spell_range", "Selbst"}, {"duration", "Sofort"},
        {"classes", "Barde, Magier"}, {"area", "Selbst"},
        {"effect_summary", "Bewege dich durch ein nahes Echo oder Spiegelbild."},
        {"description", "Für einen Herzschlag tauscht die wirkende Person ihren Platz mit einem nahen Widerhall."}});
    add(rows, "custom_spells", "spell_gate_sense", {
        {"name", "Torgespür"}, {"casting_time", "10 Minuten"}, {"spell_range", "Selbst"}, {"duration", "1 Stunde"},
        {"material_description", "gemahlener Obsidian"}, {"classes", "Magier, Hexenmeister"}, {"area", "Selbst"},
        {"effect_summary", "Spürt nahe planare Schwellen auf."}, {"scaling", "Als Ritual wird kein Zauberplatz verbraucht."},
        {"description", "Ritualmagie, die direkt mit der Hafengeschichte verknüpft ist."}});

    add(rows, "characters", "char_mira", {
        {"class", "Magierin"}, {"race", "Mensch"},
        {"backstory", "Mira kartiert instabile Tore und führt die Nachforschungen im Hafen an."},
        {"portrait_asset_id", "Resources/Campaigns/campaign_release_demo_de/Assets/Portraits/mira.png"},
        {"inventory_entries", inventory({"item_starforged_rapier:1:1:Haupthand", "item_field_journal:1:0:Rucksack", "item_healing_tonic:2:0:Beutel"})},
        {"spellbook_entries", spellbook({"spell_ember_lance", "spell_echo_step", "spell_gate_sense"})}});
    add(rows, "characters", "char_thorne", {
        {"class", "Paladin"}, {"race", "Zwerg"},
        {"backstory", "Thorne schützt die Gruppe mit Schild, Heilmagie und ruhiger Entschlossenheit."},
        {"portrait_asset_id", "Resources/Campaigns/campaign_release_demo_de/Assets/Portraits/thorne.png"},
        {"inventory_entries", inventory({"item_moonlit_shield:1:1:Nebenhand", "item_healing_tonic:1:0:Gürtel"})},
        {"spellbook_entries", spellbook({"spell_menders_light", "spell_warding_chorus"})}});
    add(rows, "characters", "char_lio", {
        {"class", "Schurke"}, {"race", "Halbling"},
        {"backstory", "Lio trägt den Obsidianschlüssel und kennt verborgene Wege durch das Hafenviertel."},
        {"portrait_asset_id", "Resources/Campaigns/campaign_release_demo_de/Assets/Portraits/lio.png"},
        {"inventory_entries", inventory({"item_gate_key:1:0:Verborgene Tasche", "item_healing_tonic:3:0:Beutel"})}});
    add(rows, "characters", "char_seren", {
        {"class", "Bardin"}, {"race", "Elf"},
        {"backstory", "Seren verbindet unterstützende Magie mit den Liedern und Gerüchten der Stadt."},
        {"portrait_asset_id", "Resources/Campaigns/campaign_release_demo_de/Assets/Portraits/seren.png"},
        {"inventory_entries", inventory({"item_healing_tonic:1:0:Umhängetasche"})},
        {"spellbook_entries", spellbook({"spell_warding_chorus", "spell_echo_step", "spell_menders_light"})}});

    add(rows, "npcs", "npc_captain_sera", {
        {"name", "Kapitänin Sera Voss"}, {"description", "Die Hafenkapitänin kann zur Verbündeten oder zum Hindernis werden."},
        {"faction", "Hafenwache"}, {"role", "Kontaktperson der Obrigkeit"},
        {"dialogue_notes", "Antwortet knapp und hasst Überraschungen."},
        {"inventory_entries", inventory({"item_healing_tonic:1:0:Schreibtisch"})}});
    add(rows, "npcs", "npc_oracle_nyme", {
        {"name", "Orakel Nyme"}, {"description", "Eine maskierte Seherin, die mit dem Geheimnis des Tores verbunden ist."},
        {"faction", "Gläserner Chor"}, {"role", "Mystische Auftraggeberin"},
        {"dialogue_notes", "Spricht in Handelspreisen und Wetterzeichen."}, {"conditions", "Konzentriert;"},
        {"inventory_entries", inventory({"item_field_journal:1:0:Lesepult"})},
        {"spellbook_entries", spellbook({"spell_gate_sense", "spell_warding_chorus", "spell_menders_light"})}});
    add(rows, "npcs", "npc_broker_cass", {
        {"description", "Miras Bruder und ein charmanter Schmuggler-Vermittler."},
        {"faction", "Laternensyndikat"}, {"role", "Rivalisierender Kontakt"},
        {"dialogue_notes", "Überspielt Angst mit Witzen und schlechten Karten."},
        {"inventory_entries", inventory({"item_gate_key:1:0:Mantel"})}});
    add(rows, "npcs", "npc_golem", {
        {"name", "Archivgolem"}, {"description", "Ein uralter Hüter des versiegelten Archivs."},
        {"faction", "Altes Archiv"}, {"role", "Wächter"},
        {"dialogue_notes", "Wiederholt alte Katalognummern, bevor er angreift."}, {"conditions", "Bewacht;"},
        {"inventory_entries", inventory({"item_moonlit_shield:1:1:Armplatte"})}});
    add(rows, "npcs", "npc_watcher_ell", {
        {"name", "Quartiermeister Elias Ell"}, {"description", "Koordiniert die Nachtwachen im Hafen."},
        {"faction", "Hafenwache"}, {"role", "Logistikoffizier"}, {"attitude_toward_party", "Freundlich, aber sachlich"},
        {"goals", "Die Patrouillenrouten lückenlos halten."}, {"resources", "Seerationen, Gezeitenkarten und Signalflaggen."},
        {"threat_location", "Wachtturm am Kai 3"}, {"relationships", "Mit Sera befreundet, Cassian gegenüber neutral."},
        {"dialogue_notes", "Wenn sich das Tor regt, lässt er noch vor Morgengrauen das Horn blasen."},
        {"inventory_entries", inventory({"item_charting_chalk:2:0:Handgelenk"})}});

    add(rows, "factions", "faction_harbor_watch", {
        {"name", "Hafenwache"}, {"faction_type", "Stadtwache"},
        {"summary", "Seeverteidiger, die Hafeneinfahrten und Zoll kontrollieren."}, {"headquarters", "Wachhaus im Hafenviertel"},
        {"attitude_toward_party", "Beschützend, Fremden gegenüber skeptisch"},
        {"goals", "Den Torbruch eindämmen und die Handelswege offen halten."},
        {"resources", "Patrouillenboote, Signaltürme und Rechtsgewalt"},
        {"gm_notes", "Verfolgt Schulden und Gefälligkeiten der Gruppe genau."}});
    add(rows, "factions", "faction_lantern_syndicate", {
        {"name", "Laternensyndikat"}, {"faction_type", "Schmugglergilde"},
        {"summary", "Ein beweglicher Verbund für den Schwarzmarktverkehr auf dem Fluss."}, {"headquarters", "Nachtmarktviertel"},
        {"attitude_toward_party", "Geschäftlich und verhandlungsbereit"},
        {"goals", "Schmuggelware ohne Blutvergießen an den Kontrollen vorbeibringen."},
        {"resources", "Sichere Routen, Dietriche und Botenketten"}, {"gm_notes", "Oft nützlich, gelegentlich unberechenbar."}});
    add(rows, "factions", "faction_archive_conclave", {
        {"name", "Archivkonklave"}, {"faction_type", "Gelehrtenzirkel"},
        {"summary", "Hüter verborgener Aufzeichnungen über planare Phänomene."}, {"headquarters", "Stapel unter dem Archiv"},
        {"attitude_toward_party", "Besessen von Ordnung, Gefühlen gegenüber gleichgültig"},
        {"goals", "Anomale Funde sichern und einordnen."}, {"resources", "Kartografische Geräte, Gewölbe und Chiffrenarchive"},
        {"gm_notes", "Unterstützt Expeditionen, die saubere Daten teilen."}});

    add(rows, "deities", "deity_aquora", {
        {"power_type", "Gottheit"}, {"pantheon", "Gezeiten-Pantheon"}, {"domains", "Wasser, Schicksal, Eide"},
        {"symbol", "Silberne Welle über schwarzem Stein"}, {"alignment", "Neutral gut"},
        {"tenets", "Schütze Küstenbünde und alle, die ihr Wort halten."},
        {"worshipers", "Seeleute, Kartografen und vereidigte Hauptleute"},
        {"relationships", "Hält mit dem Gläsernen Chor ein angespanntes Abkommen über Sturmzeichen."},
        {"gm_notes", "Zeigt sich durch ruhige Wellen und verlässliche Gezeiten."}});
    add(rows, "deities", "deity_noctur", {
        {"power_type", "Aufgestiegener Geist"}, {"pantheon", "Aufstieg des Gläsernen Chors"},
        {"domains", "Wissen, Geheimnisse, Schleier"}, {"symbol", "Geschlossenes Auge unter gebrochenem Heiligenschein"},
        {"alignment", "Rechtschaffen neutral"}, {"tenets", "Wissen wird erst durch Prüfung zur Wahrheit."},
        {"worshipers", "Seher, Archivare und Spione"}, {"relationships", "Ringt mit Aquora um rituelle Autorität."},
        {"gm_notes", "Erscheint meist als Schweigen in einer Spiegelung."}});

    add(rows, "custom_monsters", "monster_tide_wraith", {
        {"name", "Flutgespenst"}, {"source", "Kampagne"}, {"size", "Mittelgroß"}, {"creature_type", "Untoter"},
        {"actions", actions({action("Ertränkende Berührung", 5, 5, "+5", "2d8 nekrotisch", "Das Ziel hört die Hafenglocke."),
                              action("Brackiger Strahl", 5, 60, "+5", "1d10 Kälteschaden")})}});

    add(rows, "maps", "map_harbor_ward", {
        {"name", "Hafenviertel"}, {"description", "Regennasse Kais, enge Märkte und die bewachten Zugänge zum alten Archiv."},
        {"regions", packedEntries({
            mapRegion("region_fog_quay", "Nebelkai", "Regennasser Marktzugang mit schwierigem Gelände.", 0, 0, 4, 3, 2, 1, true, false, false),
            mapRegion("region_watch_line", "Wachlinie", "Nur für die Spielleitung sichtbarer Patrouillendruck am Zoll.", 4, 2, 3, 3, 1, 1, false, false, true)})},
        {"features", packedEntries({
            mapFeature("feature_gate_clue", 0, "Salzrunen-Spur", "Rückstände weisen zum Archivgewölbe.", "Nach einer Untersuchung gegen SG 13 aufdecken.", "story_gate_below", "map_archive_vault", 3, 2, true, false, 13),
            mapFeature("feature_supply_house", 0, "Hafenausrüster", "Hier gibt es Vorräte und Kartografenkreide.", "Von diesem Ort aus den Laden öffnen.", "shop_harbor_supply", "", 6, 4, true, false, 0)})},
        {"routes", packedEntries({
            mapRoute("route_harbor_to_archive", "Schmugglertreppe zum Archiv", "map_harbor_ward", "map_archive_vault", 5, 5, 1, 6, "3.5", "Städtische Tunnel", "Reiseroute für Sitzung und Kartenansicht.", true)})}});
    add(rows, "maps", "map_archive_vault", {
        {"name", "Archivgewölbe"}, {"description", "Versunkene Regale umgeben das schwarze Schloss einer planaren Schwelle."},
        {"regions", packedEntries({
            mapRegion("region_sealed_stacks", "Versiegelte Stapel", "Eingestürzte Regale blockieren die Sicht.", 1, 1, 3, 5, 1, 2, false, true, false),
            mapRegion("region_wraith_tide", "Geisterflut", "Kaltes Wasser erschwert die Bewegung.", 5, 3, 4, 3, 2, 1, true, false, false)})},
        {"features", packedEntries({
            mapFeature("feature_obsidian_lock", 1, "Obsidianschloss", "Der Schlüssel öffnet diese Schwelle.", "Beim Öffnen den Audiohinweis Torenthüllung starten.", "item_gate_key", "", 4, 3, false, false, 15),
            mapFeature("feature_hidden_ledger", 2, "Verborgene Buchnische", "Cassians Feldnotizen liegen hinter gesprungenem Stein.", "Verknüpft die Figurengeschichte mit dem Gewölbe.", "item_field_journal", "", 6, 5, false, true, 14)})}});

    add(rows, "map_layers", "layer_harbor_gm", {{"name", "SL-Markierungen"}});
    add(rows, "map_layers", "layer_harbor_party", {{"name", "Gruppenmarker"}});
    add(rows, "map_layers", "layer_archive_tokens", {{"name", "Begegnungsmarker"}});
    add(rows, "map_tokens", "token_party_harbor", {{"name", "Position der Gruppe"}, {"linked_entity_type", "Gruppe"}});
    add(rows, "map_tokens", "token_captain_sera", {{"name", "Kapitänin Sera"}});
    add(rows, "map_tokens", "token_tide_wraith", {{"name", "Startposition des Flutgespensts"}});
    add(rows, "map_tokens", "token_oracle_archive", {{"name", "Orakel Nyme"}});

    add(rows, "audio_assets", "audio_harbor_rain", {
        {"name", "Regen am Hafen"}, {"original_file_name", "regen_am_hafen.mp3"},
        {"file_path", "Resources/Campaigns/campaign_release_demo_de/Assets/Audio/regen_am_hafen.mp3"}});
    add(rows, "audio_assets", "audio_gate_reveal", {
        {"name", "Klang der Torenthüllung"}, {"original_file_name", "torenthuellung.wav"},
        {"file_path", "Resources/Campaigns/campaign_release_demo_de/Assets/Audio/torenthuellung.wav"}});
    add(rows, "audio_playlists", "playlist_session_one", {{"name", "Audiohinweise für Sitzung eins"}});
    add(rows, "asset_metadata", "assetmeta_harbor_rain", {
        {"display_name", "Regen am Hafen"}, {"project_path", "Resources/Campaigns/campaign_release_demo_de/Assets/Audio/regen_am_hafen.mp3"}});
    add(rows, "asset_metadata", "assetmeta_gate_reveal", {
        {"display_name", "Klang der Torenthüllung"}, {"project_path", "Resources/Campaigns/campaign_release_demo_de/Assets/Audio/torenthuellung.wav"}});
    add(rows, "asset_metadata", "assetmeta_mira_portrait", {
        {"display_name", "Porträt von Mira Vale"}, {"project_path", "Resources/Campaigns/campaign_release_demo_de/Assets/Portraits/mira.png"}});

    add(rows, "shops", "shop_harbor_supply", {
        {"name", "Hafenausrüster"}, {"merchant_name", "Hauptmann Rook"},
        {"description", "Praktische Ausrüstung und ritualtaugliche Werkzeuge nahe der Zollstation."},
        {"faction_reputation_notes", "Guter Ruf bei der Hafenwache gewährt 20 Prozent Rabatt."}});
    add(rows, "shops", "shop_glass_market", {
        {"name", "Basar des Gläsernen Chors"}, {"description", "Ausgewählte Reliquien, Gebetsstücke und Gezeitenamulette."},
        {"faction_reputation_notes", "Anhänger Nocturs erhalten bei guter Beziehung bessere Preise."}});
    add(rows, "loot_parcels", "parcel_gate_cache", {
        {"name", "Vorrat am Tor"}, {"source", "Durchbruch im Archiv"},
        {"notes", "Am Ort des Zusammenbruchs des Flutgespensts geborgen."}});

    add(rows, "encounters", "enc_archive_breach", {
        {"name", "Durchbruch im Archiv"}, {"description", "Während Orakel Nyme den Schutzchor hält, steigt ein Flutgespenst zwischen den versunkenen Regalen empor."},
        {"map_context_name", "Archivgewölbe"},
        {"dice_roll_presets", packedEntries({
            dicePreset("preset_initiative", "Gruppeninitiative", "1d20+2", "Schneller Einstieg in die vorbereitete Begegnung."),
            dicePreset("preset_wraith_damage", "Schaden des Flutgespensts", "2d8", "Für Ertränkende Berührung verwenden."),
            dicePreset("preset_gate_surge", "Torpuls", "1d6+2", "Schaden durch die Umgebung.")})},
        {"combat_log", packedEntries({
            combatLog("log_setup", 1, "SL", "Notiz", "Die Begegnung ist mit Gruppenmarkern, verborgenem Flutgespenst und Audiohinweis vorbereitet."),
            combatLog("log_round_hint", 1, "Orakel Nyme", "Zauber", "Der Schutzchor benötigt Konzentration; bei Schaden daran erinnern.")})},
        {"after_combat_summary", "Nach dem Sieg stabilisiert sich das Tor lange genug für eine wichtige Enthüllung."},
        {"after_combat_loot_notes", "Vorrat am Tor vergeben und 450 EP oder einen Meilenstein gewähren."},
        {"after_combat_story_consequences", "Die Szene Der Obsidianschlüssel dreht sich abschließen und Cassians Schuld voranbringen."},
        {"after_combat_session_notes", "Guter Abschluss für Sitzung eins oder Übergang zur Audienz beim Gläsernen Chor."}});

    add(rows, "encounter_participants", "part_mira", {
        {"actions", actions({action("Glutlanze", 5, 90, "+7", "1d10 Feuerschaden", "Zauberangriff aus dem Zauberbuch."),
                              action("Sternengeschmiedetes Rapier", 5, 5, "+8", "1d8 Stichschaden", "Ausgerüstete Waffe.")})},
        {"group_name", "Gruppe"}});
    add(rows, "encounter_participants", "part_thorne", {
        {"actions", actions({action("Licht der Heilerin", 5, 5, "+6", "1d8 + Attribut", "Heilzauber."),
                              action("Schildstoß", 5, 5, "+5", "1d6 Wuchtschaden")})},
        {"group_name", "Gruppe"}});
    add(rows, "encounter_participants", "part_oracle", {
        {"display_name", "Orakel Nyme"}, {"conditions", "Konzentriert;"},
        {"actions", actions({action("Schutzchor", 5, 30, "+0", "manuelles Ergebnis", "Rettungswurf auf Weisheit gegen SG 16.")})},
        {"group_name", "Verbündete"},
        {"active_effects", packedEntries({activeEffect("effect_oracle_chorus", 2, "Schutzchor", "Orakel Nyme", "npc_oracle_nyme", "spell_warding_chorus", "Konzentration im Kampftracker beachten.", 10, true)})}});
    add(rows, "encounter_participants", "part_wraith", {
        {"display_name", "Flutgespenst"},
        {"actions", actions({action("Ertränkende Berührung", 5, 5, "+5", "2d8 nekrotisch"),
                              action("Brackiger Strahl", 5, 60, "+5", "1d10 Kälteschaden")})},
        {"group_name", "Welle 2"}, {"damage_resistances", "Kälte;nekrotisch;"}, {"damage_vulnerabilities", "gleißend;"}});

    add(rows, "story_threads", "story_gate_below", {
        {"title", "Das Tor unter dem Hafen"}, {"summary", "Haupthandlung mit Szenen, Karte, Audiohinweisen, Zaubern und Gegenständen."},
        {"stakes", "Scheitert die Gruppe, wird der Hafen zu einem planaren Riss."},
        {"gm_notes", "Beim Drehen des Schlüssels den Hinweis zur Torenthüllung starten."}});
    add(rows, "story_threads", "story_mira_cass", {
        {"title", "Mira und Cassians Schuld"}, {"summary", "Figurengeschichte um Mira, Cassian und das Feldtagebuch."},
        {"stakes", "Mira kann Cassian retten oder das Syndikat entlarven."},
        {"gm_notes", "Verbindet Charakterbogen und persönliche Geschichte."}});
    add(rows, "story_threads", "story_glass_choir", {
        {"title", "Der Gläserne Chor"}, {"summary", "Welthandlung um Orakel Nyme und rituelle Magie."},
        {"stakes", "Der Chor kann Auftraggeber, Gegner oder Warnsystem werden."},
        {"gm_notes", "Zauber- und Figurenverknüpfungen sollen im Inspektor sichtbar sein."}});

    add(rows, "story_beats", "beat_gate_key", {
        {"title", "Der Obsidianschlüssel dreht sich"}, {"summary", "Die Gruppe setzt den Schlüssel im Archivgewölbe ein; das Tor antwortet."},
        {"gm_notes", "Torenthüllung starten und die Archivszene öffnen."}, {"in_world_date", "Nacht 3"}});
    add(rows, "story_beats", "beat_harbor_rain", {
        {"title", "Regen über dem Hafenviertel"}, {"summary", "Ermittlungsszene mit Sera, Cassian und der ersten Spur."},
        {"gm_notes", "Regen leise als Schleife abspielen."}, {"in_world_date", "Nacht 1"}});
    add(rows, "story_beats", "beat_cassian_debt", {
        {"title", "Cassians Schuld"}, {"summary", "Cassian bittet wegen alter Schulden beim Laternensyndikat um Hilfe."},
        {"gm_notes", "Mit dem Laden und Cassian als rivalisierendem Kontakt verknüpfen."}, {"in_world_date", "Abend 2"}});
    add(rows, "story_beats", "beat_glass_choir_council", {
        {"title", "Rat über dem Glas"}, {"summary", "Die Gruppe erhält eine förmliche Audienz bei Orakel Nyme."},
        {"gm_notes", "Hier wird die Rolle des Chors in der Welt enthüllt."}, {"in_world_date", "Morgengrauen 1"}});

    const std::vector<std::pair<std::string, std::string>> linkLabels = {
        {"link_gate_mira", "leitende Forscherin"}, {"link_gate_map", "Ort der Begegnung"},
        {"link_gate_spell", "rituelle Spur"}, {"link_cass_lio", "Träger des Schlüssels"},
        {"link_cass_npc", "rivalisierender Bruder"}, {"link_choir_oracle", "Stimme des Chors"},
        {"link_beat_key_item", "öffnet das Tor"}, {"link_beat_key_audio", "Enthüllungsmusik abspielen"},
        {"link_beat_harbor_audio", "während der Ermittlung abspielen"}, {"link_beat_debt", "Kontakt für Cassians Schuld"},
        {"link_beat_debt_shop", "Zugang zum Schuldgeschäft"}, {"link_beat_choir", "rituelle Audienz"},
    };
    for (const auto &entry : linkLabels)
        add(rows, "story_links", entry.first, {{"label", entry.second}});

    add(rows, "story_documents", "doc_session_one_runbook", {
        {"title", "Ablaufplan für Sitzung 1"},
        {"body", "Mit Regen am Hafen beginnen. Jede Figur führt eine Spur, eine Ressource und ein Risiko ein. Danach vom Hafenviertel zum Archivgewölbe wechseln und Durchbruch im Archiv starten, sobald Lio den Obsidianschlüssel benutzt."}});
    add(rows, "story_documents", "doc_table_safety_and_tone", {
        {"title", "Ton und Sicherheit am Tisch"},
        {"body", "Küstenfantasy, Geheimnisse und okkulter Druck mit bodenständigem Heldentum. Bedrohungen unheimlich, aber verständlich halten; drastische Gewalt vermeiden und soziale Konflikte deeskalierbar gestalten."}});

    add(rows, "timeline_events", "timeline_harbor_hook", {
        {"title", "Regen über dem Hafen"}, {"description", "Sera warnt die Gruppe, während sich über dem Tor ein Sturm sammelt."}, {"in_world_date", "Nacht 1"}});
    add(rows, "timeline_events", "timeline_archive_breach", {
        {"title", "Durchbruch im Archiv"}, {"description", "Der Obsidianschlüssel dreht sich und das Flutgespenst erscheint."}, {"in_world_date", "Nacht 1 - Dritte Glocke"}});
    add(rows, "timeline_events", "timeline_choir_audience", {
        {"title", "Audienz beim Gläsernen Chor"}, {"description", "Orakel Nyme bietet eine rituelle Erklärung und nennt den Preis für das Schließen des Tores."}, {"in_world_date", "Morgengrauen 2"}});

    add(rows, "dungeons", "dungeon_archive_lower", {
        {"name", "Unteres Archivgewölbe"}, {"description", "Drei spielbare Räume, direkt mit Karte und Begegnung verknüpft."},
        {"gm_notes", "Die Buchnische bleibt verborgen, bis Cassians Kreidemarkierung erkannt oder das Flutglas an das Schloss gehalten wird."}});
    add(rows, "dungeon_rooms", "room_archive_entry", {
        {"name", "Versalzene Eingangsstapel"}, {"description", "Eingestürzter Eingang mit Wasserflecken und Wachkreide."},
        {"keyed_description", "Raum A: sicherer Ausgangspunkt mit der ersten Torspur."},
        {"read_aloud_text", "Regenwasser tropft an den Regalen nach oben und fällt dann, als erinnere es sich an die Schwerkraft."},
        {"secrets", "Eine Untersuchung gegen SG 13 erkennt Cassians Kreidemarkierung."},
        {"exploration_notes", "Beim ersten Aufdecken der Karte sichtbar."},
        {"connections", packedEntries({roomConnection("conn_entry_lock", "room_obsidian_lock", "Gang mit Obsidianschloss", true, false, true)})}});
    add(rows, "dungeon_rooms", "room_obsidian_lock", {
        {"name", "Gang am Obsidianschloss"}, {"description", "Torschwelle und rituelles Schloss."},
        {"keyed_description", "Raum B: Torszene mit Audiohinweis."},
        {"read_aloud_text", "Das Schloss blüht im Mondlicht wie schwarzes Glas auf."},
        {"secrets", "Der Flutglas-Fokus summt, wenn er hierher gebracht wird."},
        {"traps", "Öffnen ohne Schlüssel löst einen kalten Puls aus."},
        {"exploration_notes", "Aktiver Raum für die deutsche Demo."},
        {"connections", packedEntries({
            roomConnection("conn_lock_entry", "room_archive_entry", "Zurück zu den Eingangsstapeln", false, false, true),
            roomConnection("conn_lock_ledger", "room_ledger_niche", "Geheime Buchnische", false, true, false)})}});
    add(rows, "dungeon_rooms", "room_ledger_niche", {
        {"name", "Buchnische"}, {"description", "Verborgener Nebenraum mit Cassians älteren Notizen."},
        {"keyed_description", "Raum C: optionale Belohnung für die Figurengeschichte."},
        {"read_aloud_text", "Hinter einem längst verrotteten Regal wartet eine trockene Nische."},
        {"secrets", "Das Tagebuch nennt einen Förderer des Gläsernen Chors."},
        {"exploration_notes", "Als abgeschlossen markieren, sobald die Gruppe das Tagebuch birgt."},
        {"connections", packedEntries({roomConnection("conn_ledger_lock", "room_obsidian_lock", "Verborgene Rückwand", false, true, false)})}});

    add(rows, "sessions", "session_feature_tour_one", {
        {"title", "Sitzung 1: Regen, Schuld und das Tor"},
        {"summary", "Eine sofort spielbare Sitzung für vier Personen mit Szenen, Audiohinweisen, Karten, Räumen, Laden, Inventar und einer Begegnung."},
        {"recap", "Die Gruppe trifft im Regen auf Kapitänin Sera und folgt der Salzrunen-Spur zum Archiv."},
        {"unresolved_threads", "Wer finanziert Cassian? Was weiß Orakel Nyme?"},
        {"loot_awarded", "Der Vorrat am Tor steht noch aus."}, {"xp_milestones", "Meilenstein Durchbruch im Archiv oder 450 EP."},
        {"npcs_met", "Kapitänin Sera Voss;Cassian Vale;Orakel Nyme;"},
        {"locations_visited", "Hafenviertel;Unteres Archivgewölbe;"},
        {"open_hooks", "Audienz beim Gläsernen Chor;Ursprung des Tores;"}});
    add(rows, "session_notes", "note_opening_cue", {
        {"body", "Vor dem Lesetext der Hafenszene Regen am Hafen starten."}, {"linked_entity_type", "Audio"}, {"linked_entity_label", "Regen am Hafen"}});
    add(rows, "session_notes", "note_gate_reveal", {
        {"body", "Sobald Lio den Obsidianschlüssel benutzt, den Klang der Torenthüllung starten und Durchbruch im Archiv laden."},
        {"linked_entity_type", "Begegnung"}, {"linked_entity_label", "Durchbruch im Archiv"}});

    const std::string rulesetJson =
        R"json({"schemaVersion":1,"rulesetId":"hafenfantasy-demo","abilityModifierFormula":"score","proficiencyFormula":"0","checkDiceFormula":"1d20","passiveScoreBase":10,"published":0,"abilityScores":[{"id":"strength","name":"Stärke","abbreviation":"STÄ","description":"Körperkraft und Athletik."},{"id":"dexterity","name":"Geschicklichkeit","abbreviation":"GES","description":"Beweglichkeit und Präzision."},{"id":"wisdom","name":"Weisheit","abbreviation":"WEI","description":"Wahrnehmung und Intuition."}],"skills":[{"id":"athletics","name":"Athletik","abilityScoreId":"strength","description":"Klettern, Springen und Kraftakte."},{"id":"investigation","name":"Nachforschung","abilityScoreId":"wisdom","description":"Spuren verbinden und Geheimnisse aufdecken."}],"resources":[{"id":"momentum","name":"Schwung","maximumFormula":"3","recoveryFormula":"3","recoveryTrigger":"Sitzungsbeginn","description":"Gemeinsamer Vorteil der Gruppe."}],"conditions":[{"id":"soaked","name":"Durchnässt","description":"Nasse Kleidung erschwert vorsichtige Bewegung.","mechanicalEffects":["Nachteil bei leisem Klettern."],"endCondition":"Trockene Rast."}]})json";
    add(rows, "ruleset_definitions", "ruleset_feature_tour_overlay", {
        {"name", "Hafenfantasy - Deutscher Entwurf"}, {"installed", "1"},
        {"definition_json", rulesetJson}});
    add(rows, "homebrew_definitions", "homebrew_feature_tour_pack", {
        {"name", "Hausregeln der Hafenkampagne"}, {"author", "GM Editor Demo"}, {"source", "Lokale deutsche Demo"},
        {"definition_json", R"json({"items":["Sternengeschmiedetes Rapier","Mondlichtschild"],"spells":["Glutlanze","Schutzchor"],"session":"Durchbruch im Archiv"})json"}});
    add(rows, "house_rules", "house_rule_spell_fatigue", {
        {"label", "Rituelle Ausdauer"}, {"description", "Zauberwirkende erhalten nach einer kurzen Rast einen verbrauchten Ritualplatz zurück, wenn sie mindestens 30 Minuten kontrolliert meditieren."}});
    add(rows, "house_rules", "house_rule_shops_open", {
        {"label", "Öffnungszeiten am Hafenmarkt"}, {"description", "Ladenvorräte werden nur durch ausdrückliches Auffüllen erneuert."},
        {"change_history", packedEntries({packed({"2026-05-22T00:00:00", "Besonderen Marktrhythmus nach langen Seereisen ergänzt."})})}});

    return rows;
}

bool tableExists(sqlite3 *db, const std::string &name)
{
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=? LIMIT 1;", -1, &statement, nullptr) != SQLITE_OK)
        throw std::runtime_error("Could not inspect sqlite schema.");
    sqlite3_bind_text(statement, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    const bool found = sqlite3_step(statement) == SQLITE_ROW;
    sqlite3_finalize(statement);
    return found;
}

std::string valueFor(sqlite3 *db, const RowTranslation &row, const FieldValue &field)
{
    const std::string sql = "SELECT " + identifier(field.column) + " FROM " + identifier(row.table) +
                            " WHERE stable_id=? LIMIT 1;";
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
        throw std::runtime_error("Could not read " + row.table + "." + field.column + ".");
    sqlite3_bind_text(statement, 1, row.stableId.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement) != SQLITE_ROW)
    {
        sqlite3_finalize(statement);
        throw std::runtime_error("Required demo row is missing: " + row.table + "/" + row.stableId);
    }
    const unsigned char *text = sqlite3_column_text(statement, 0);
    const std::string value = text ? reinterpret_cast<const char *>(text) : "";
    sqlite3_finalize(statement);
    return value;
}

void applyTranslations(sqlite3 *db, bool isolateCampaign)
{
    exec(db, "PRAGMA foreign_keys = ON;");
    exec(db, "BEGIN IMMEDIATE TRANSACTION;");
    try
    {
        if (isolateCampaign)
            exec(db, "DELETE FROM campaigns WHERE stable_id <> " + quote(kCampaignId) + ";");

        for (const RowTranslation &row : germanTranslations())
        {
            if (!tableExists(db, row.table))
                throw std::runtime_error("Required demo table is missing: " + row.table);
            std::string sql = "UPDATE " + identifier(row.table) + " SET ";
            for (std::size_t index = 0; index < row.fields.size(); ++index)
            {
                if (index > 0)
                    sql += ", ";
                sql += identifier(row.fields[index].column) + "=" + quote(row.fields[index].value);
            }
            sql += " WHERE stable_id=" + quote(row.stableId) + ";";
            exec(db, sql);
            if (sqlite3_changes(db) != 1)
                throw std::runtime_error("Required demo row is missing or ambiguous: " + row.table + "/" + row.stableId);
        }

        exec(db,
             "UPDATE shop_stock SET availability='Verfügbar', haggle_notes='Bei der ersten Bestellung nach einem Zusatzbeutel fragen.' "
             "WHERE shop_id=(SELECT id FROM shops WHERE stable_id='shop_harbor_supply') AND item_id='item_charting_chalk';");
        exec(db,
             "UPDATE shop_stock SET availability='Begrenzt', haggle_notes='Außerhalb des Wachplans nicht günstig.' "
             "WHERE shop_id=(SELECT id FROM shops WHERE stable_id='shop_harbor_supply') AND item_id='item_moonlit_shield';");
        exec(db,
             "UPDATE shop_stock SET availability='Selten', haggle_notes='Nur mit einem Zeichen des Gläsernen Chors zugänglich.' "
             "WHERE shop_id=(SELECT id FROM shops WHERE stable_id='shop_glass_market') AND item_id='item_tideglass_focus';");
        exec(db,
             "UPDATE loot_parcel_items SET notes='Bekommt bei Tageslicht feine Risse.' "
             "WHERE loot_parcel_id=(SELECT id FROM loot_parcels WHERE stable_id='parcel_gate_cache') AND item_id='item_tideglass_focus';");
        exec(db,
             "UPDATE loot_parcel_items SET notes='Persönliche Beobachtungen zu harmonischen Siegeln.' "
             "WHERE loot_parcel_id=(SELECT id FROM loot_parcels WHERE stable_id='parcel_gate_cache') AND item_id='item_field_journal';");
        // Promote the localized draft into the application-level Ruleset
        // Studio library. It remains a draft (published=0 in definition_json)
        // and is deliberately detached from campaign cascade deletion.
        exec(db, "UPDATE ruleset_definitions SET campaign_id=NULL, installed=1 WHERE stable_id='ruleset_feature_tour_overlay';");
        exec(db, "COMMIT;");
    }
    catch (...)
    {
        exec(db, "ROLLBACK;");
        throw;
    }
}

std::uint64_t fixtureFingerprint()
{
    std::uint64_t value = 1469598103934665603ULL;
    auto mix = [&](const std::string &text) {
        for (const unsigned char byte : text)
        {
            value ^= byte;
            value *= 1099511628211ULL;
        }
        value ^= 0xff;
        value *= 1099511628211ULL;
    };
    for (const RowTranslation &row : germanTranslations())
    {
        mix(row.table);
        mix(row.stableId);
        for (const FieldValue &field : row.fields)
        {
            mix(field.column);
            mix(field.value);
        }
    }
    mix(kRevision);
    return value;
}

std::size_t validateTranslations(sqlite3 *db)
{
    std::size_t checked = 0;
    for (const RowTranslation &row : germanTranslations())
    {
        for (const FieldValue &field : row.fields)
        {
            const std::string actual = valueFor(db, row, field);
            if (actual != field.value)
                throw std::runtime_error("German demo validation failed at " + row.table + "/" + row.stableId + "/" + field.column);
            ++checked;
        }
    }

    sqlite3_stmt *statement = nullptr;
    const char *sql = "SELECT COUNT(*) FROM campaigns WHERE stable_id=? AND name=? AND tags LIKE '%deutsche-demo%';";
    if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK)
        throw std::runtime_error("Could not validate German campaign identity.");
    sqlite3_bind_text(statement, 1, kCampaignId, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, kCampaignName, -1, SQLITE_STATIC);
    const bool validIdentity = sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_int(statement, 0) == 1;
    sqlite3_finalize(statement);
    if (!validIdentity)
        throw std::runtime_error("German release-demo identity or revision tag is missing.");

    sqlite3_stmt *rulesetStatement = nullptr;
    if (sqlite3_prepare_v2(db,
                           "SELECT COUNT(*) FROM ruleset_definitions WHERE stable_id='ruleset_feature_tour_overlay' AND campaign_id IS NULL AND installed=1;",
                           -1, &rulesetStatement, nullptr) != SQLITE_OK)
        throw std::runtime_error("Could not validate German Ruleset Studio draft.");
    const bool validRulesetDraft = sqlite3_step(rulesetStatement) == SQLITE_ROW && sqlite3_column_int(rulesetStatement, 0) == 1;
    sqlite3_finalize(rulesetStatement);
    if (!validRulesetDraft)
        throw std::runtime_error("German Ruleset Studio draft is not installed at application level.");

    const std::vector<std::pair<std::string, std::string>> scopedValues = {
        {"SELECT availability FROM shop_stock WHERE shop_id=(SELECT id FROM shops WHERE stable_id='shop_harbor_supply') AND item_id='item_charting_chalk';", "Verfügbar"},
        {"SELECT availability FROM shop_stock WHERE shop_id=(SELECT id FROM shops WHERE stable_id='shop_harbor_supply') AND item_id='item_moonlit_shield';", "Begrenzt"},
        {"SELECT availability FROM shop_stock WHERE shop_id=(SELECT id FROM shops WHERE stable_id='shop_glass_market') AND item_id='item_tideglass_focus';", "Selten"},
        {"SELECT notes FROM loot_parcel_items WHERE loot_parcel_id=(SELECT id FROM loot_parcels WHERE stable_id='parcel_gate_cache') AND item_id='item_tideglass_focus';", "Bekommt bei Tageslicht feine Risse."},
        {"SELECT notes FROM loot_parcel_items WHERE loot_parcel_id=(SELECT id FROM loot_parcels WHERE stable_id='parcel_gate_cache') AND item_id='item_field_journal';", "Persönliche Beobachtungen zu harmonischen Siegeln."},
    };
    for (const auto &expected : scopedValues)
    {
        sqlite3_stmt *scopedStatement = nullptr;
        if (sqlite3_prepare_v2(db, expected.first.c_str(), -1, &scopedStatement, nullptr) != SQLITE_OK)
            throw std::runtime_error("Could not validate scoped German demo content.");
        const bool matches = sqlite3_step(scopedStatement) == SQLITE_ROW &&
                             sqlite3_column_text(scopedStatement, 0) != nullptr &&
                             expected.second == reinterpret_cast<const char *>(sqlite3_column_text(scopedStatement, 0));
        sqlite3_finalize(scopedStatement);
        if (!matches)
            throw std::runtime_error("German demo validation failed for scoped shop or loot content.");
        ++checked;
    }

    const std::set<std::string> technicalColumns = {
        "stable_id", "tags", "created_at", "updated_at", "last_modified", "attributes",
        "weapon_ability", "spellcasting_ability", "saving_throw_ability", "ruleset_name",
        "cells", "battlefield_cells", "spell_slots_max", "spell_slots_used", "inventory_entries",
        "spellbook_entries", "actions", "regions", "features", "routes", "connections",
        "dice_roll_presets", "combat_log", "active_effects", "definition_json", "change_history"};
    const std::set<std::string> englishTokens = {
        "action", "allies", "archive", "campaign", "captain", "careful", "common",
        "concentrating", "current", "dwarf", "encounter", "feature", "gate", "guarding",
        "halfling", "harbor", "human", "limited", "map", "medium", "monster", "party",
        "player", "rare", "rogue", "self", "session", "spell", "story", "tests", "undead",
        "vault", "wizard"};
    const std::set<std::string> englishConnectors = {
        "after", "before", "from", "into", "should", "that", "their", "this", "through",
        "used", "when", "while", "with"};

    std::set<std::string> scannedRows;
    for (const RowTranslation &row : germanTranslations())
    {
        if (!scannedRows.insert(row.table + "\n" + row.stableId).second)
            continue;
        const std::string sql = "SELECT * FROM " + identifier(row.table) + " WHERE stable_id=? LIMIT 1;";
        sqlite3_stmt *rowStatement = nullptr;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &rowStatement, nullptr) != SQLITE_OK)
            throw std::runtime_error("Could not scan German demo row: " + row.table + "/" + row.stableId);
        sqlite3_bind_text(rowStatement, 1, row.stableId.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(rowStatement) != SQLITE_ROW)
        {
            sqlite3_finalize(rowStatement);
            throw std::runtime_error("German demo row vanished during language scan: " + row.table + "/" + row.stableId);
        }
        const int columnCount = sqlite3_column_count(rowStatement);
        for (int columnIndex = 0; columnIndex < columnCount; ++columnIndex)
        {
            if (sqlite3_column_type(rowStatement, columnIndex) != SQLITE_TEXT)
                continue;
            const std::string column = sqlite3_column_name(rowStatement, columnIndex);
            if (technicalColumns.count(column) ||
                (column.size() >= 3 && column.compare(column.size() - 3, 3, "_id") == 0) ||
                (column.size() >= 4 && column.compare(column.size() - 4, 4, "_ids") == 0) ||
                (column.size() >= 5 && column.compare(column.size() - 5, 5, "_type") == 0) ||
                column.find("path") != std::string::npos || column.find("file_name") != std::string::npos ||
                column.find("hash") != std::string::npos || column.find("mime") != std::string::npos)
                continue;

            const unsigned char *raw = sqlite3_column_text(rowStatement, columnIndex);
            std::string normalized = raw ? reinterpret_cast<const char *>(raw) : "";
            for (char &character : normalized)
            {
                const unsigned char byte = static_cast<unsigned char>(character);
                character = std::isalpha(byte) ? static_cast<char>(std::tolower(byte)) : ' ';
            }
            std::istringstream words(normalized);
            std::string word;
            while (words >> word)
            {
                if (englishTokens.count(word) || englishConnectors.count(word))
                {
                    sqlite3_finalize(rowStatement);
                    throw std::runtime_error("English authored token '" + word + "' remains in German demo at " +
                                             row.table + "/" + row.stableId + "/" + column);
                }
            }
        }
        sqlite3_finalize(rowStatement);
    }

    return checked;
}

std::string fingerprintText()
{
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << fixtureFingerprint();
    return out.str();
}

void openAndRun(const std::filesystem::path &path, bool verifyOnly, bool isolateCampaign)
{
    sqlite3 *db = nullptr;
    if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK)
        throw std::runtime_error("Could not open database: " + path.string());
    try
    {
        if (!verifyOnly)
            applyTranslations(db, isolateCampaign);
        const std::size_t checked = validateTranslations(db);
        sqlite3_close(db);
        std::cout << (verifyOnly ? "Verified" : "Seeded") << " German release demo in " << path.string()
                  << " | revision=" << kRevision << " | fingerprint=" << fingerprintText()
                  << " | authored-fields=" << checked << "\n";
    }
    catch (...)
    {
        sqlite3_close(db);
        throw;
    }
}

void selfTest()
{
    sqlite3 *db = nullptr;
    if (sqlite3_open(":memory:", &db) != SQLITE_OK)
        throw std::runtime_error("Could not create in-memory database.");
    try
    {
        std::map<std::string, std::set<std::string>> schemas;
        for (const RowTranslation &row : germanTranslations())
            for (const FieldValue &field : row.fields)
                schemas[row.table].insert(field.column);

        for (const auto &schema : schemas)
        {
            std::string sql = "CREATE TABLE " + identifier(schema.first) + " (id INTEGER, stable_id TEXT PRIMARY KEY";
            for (const std::string &column : schema.second)
                sql += ", " + identifier(column) + " TEXT";
            if (schema.first == "ruleset_definitions")
                sql += ", campaign_id TEXT";
            sql += ");";
            exec(db, sql);
        }
        for (const RowTranslation &row : germanTranslations())
            exec(db, "INSERT INTO " + identifier(row.table) + " (stable_id) VALUES (" + quote(row.stableId) + ");");

        // Minimal scoped child rows cover shops and loot as well; all checks run
        // in memory and never touch a developer or user database.
        exec(db, "CREATE TABLE shop_stock (shop_id INTEGER, item_id TEXT, availability TEXT, haggle_notes TEXT);");
        exec(db, "CREATE TABLE loot_parcel_items (loot_parcel_id INTEGER, item_id TEXT, notes TEXT);");
        exec(db, "UPDATE shops SET id=1 WHERE stable_id='shop_harbor_supply';");
        exec(db, "UPDATE shops SET id=2 WHERE stable_id='shop_glass_market';");
        exec(db, "UPDATE loot_parcels SET id=1 WHERE stable_id='parcel_gate_cache';");
        exec(db, "INSERT INTO shop_stock (shop_id,item_id) VALUES (1,'item_charting_chalk'),(1,'item_moonlit_shield'),(2,'item_tideglass_focus');");
        exec(db, "INSERT INTO loot_parcel_items (loot_parcel_id,item_id) VALUES (1,'item_tideglass_focus'),(1,'item_field_journal');");
        applyTranslations(db, false);
        const std::size_t firstCount = validateTranslations(db);
        applyTranslations(db, false);
        const std::size_t secondCount = validateTranslations(db);
        if (firstCount == 0 || firstCount != secondCount)
            throw std::runtime_error("German demo manifest is empty or non-deterministic.");
        sqlite3_close(db);
        std::cout << "German release-demo self-test passed | revision=" << kRevision
                  << " | fingerprint=" << fingerprintText() << " | authored-fields=" << firstCount << "\n";
    }
    catch (...)
    {
        sqlite3_close(db);
        throw;
    }
}
}

int main(int argc, char **argv)
{
    try
    {
        bool verifyOnly = false;
        bool isolateCampaign = false;
        bool runSelfTest = false;
        std::vector<std::filesystem::path> databases;
        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (argument == "--verify-only")
                verifyOnly = true;
            else if (argument == "--isolate")
                isolateCampaign = true;
            else if (argument == "--self-test")
                runSelfTest = true;
            else if (!argument.empty() && argument.front() == '-')
                throw std::runtime_error("Unknown option: " + argument);
            else
                databases.emplace_back(argument);
        }

        if (runSelfTest)
        {
            if (!databases.empty() || verifyOnly || isolateCampaign)
                throw std::runtime_error("Usage: seed_release_demo_de --self-test");
            selfTest();
            return 0;
        }
        if (databases.empty())
            throw std::runtime_error("Usage: seed_release_demo_de [--verify-only] [--isolate] <db> [db...]");
        if (verifyOnly && isolateCampaign)
            throw std::runtime_error("--isolate cannot be combined with --verify-only");

        for (const std::filesystem::path &database : databases)
            openAndRun(database, verifyOnly, isolateCampaign);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
