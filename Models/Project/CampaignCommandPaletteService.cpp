#include "CampaignCommandPaletteService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/UX/LegacySurfaceRegistry.hpp"

#include <algorithm>
#include <cctype>

namespace
{
std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool containsText(const std::string &haystack, const std::string &needle)
{
    return needle.empty() || lowerCopy(haystack).find(lowerCopy(needle)) != std::string::npos;
}
}

namespace CampaignCommandPalette
{
std::vector<CampaignCommandPaletteItem> buildItems(const CampaignSearchIndex &index)
{
    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const auto text = [german](const char *english, const char *germanText) { return std::string(german ? germanText : english); };
    const std::string navigate = text("Navigate", "Navigieren");
    const std::string create = text("Create", "Erstellen");
    std::vector<CampaignCommandPaletteItem> items{
        {"workspace:home", text("Open Campaign Home", "Kampagnenstart öffnen"), navigate, {}, {}},
        {"workspace:story", text("Open Story", "Geschichte öffnen"), navigate, {}, {}},
        {"workspace:cast", text("Open Cast", "Figuren öffnen"), navigate, {}, {}},
        {"workspace:world", text("Open World", "Welt öffnen"), navigate, {}, {}},
        {"workspace:encounters", text("Open Encounters", "Begegnungen öffnen"), navigate, {}, {}},
        {"workspace:compendium", text("Open Compendium", "Kompendium öffnen"), navigate, {}, {}},
        {"workspace:live", text("Open Live", "Live öffnen"), navigate, {}, {}},
        {"action:open_tabletop_toolkit", text("Open dice, objectives and clues", "Würfel, Ziele und Hinweise öffnen"), text("Live table tools", "Live-Tischwerkzeuge"), {}, {}},
        {"action:open_objectives", text("Open objectives", "Ziele öffnen"), text("Live table tools", "Live-Tischwerkzeuge"), {}, {}},
        {"action:open_clues", text("Open clues", "Hinweise öffnen"), text("Live table tools", "Live-Tischwerkzeuge"), {}, {}},
        {"action:review_player_display", text("Review Player Display", "Spieleranzeige prüfen"), text("Live", "Live"), {}, {}},
        {"workspace:settings", text("Open Settings and Project Health", "Einstellungen und Projektzustand öffnen"), navigate, {}, {}},
        {"action:new_character", text("New player character in Cast", "Neuer Spielercharakter unter Figuren"), create, {}, {}},
        {"action:new_npc", text("New NPC or creature in Cast", "Neuer NSC oder neue Kreatur unter Figuren"), create, {}, {}},
        {"action:new_map", text("New map in World", "Neue Karte in Welt"), create, {}, {}},
        {"action:new_encounter", text("New encounter in Encounters", "Neue Begegnung unter Begegnungen"), create, {}, {}},
        {"action:new_item", text("New item in Compendium", "Neuer Gegenstand im Kompendium"), create, {}, {}},
        {"action:save", text("Save Campaign", "Kampagne speichern"), text("Campaign", "Kampagne"), {}, {}},
        {"action:rebuild_index", text("Rebuild Campaign Index", "Kampagnenindex neu aufbauen"), text("Project Health", "Projektzustand"), {}, {}}};

    // The palette is a first-class navigation surface. It must never become a
    // back door to controllers that the retirement contract removed from
    // navigation. Keep this guard even when the static command list changes.
    items.erase(std::remove_if(items.begin(), items.end(), [](const CampaignCommandPaletteItem &item) {
                    const gm::ux::LegacySurfaceRegistration *surface = gm::ux::findLegacySurface(item.id);
                    return surface && !gm::ux::isLegacyEntryPointAllowed(*surface, gm::ux::LegacyEntryPoint::UniversalSearch);
                }),
                items.end());

    for (const CampaignSearchDocument &document : index.documents())
    {
        const std::string openLabel = document.type == "Objective"
            ? text("Open objective: ", "Ziel öffnen: ")
            : document.type == "Clue"
                ? text("Open clue: ", "Hinweis öffnen: ")
                : text("Open ", "Öffnen: ");
        const std::string category = document.type == "Objective"
            ? text("Table objective", "Tischziel")
            : document.type == "Clue"
                ? text("Table clue", "Tischhinweis")
                : document.type;
        items.push_back(CampaignCommandPaletteItem{"open:" + document.id, openLabel + document.title, category, document.id, document.type});
        if (document.type == "Random Table")
            items.push_back(CampaignCommandPaletteItem{"roll:" + document.id,
                text("Roll ", "Würfeln: ") + document.title, text("Live table", "Live-Tabelle"),
                document.id, document.type});
    }

    return items;
}

std::vector<CampaignCommandPaletteItem> filterItems(const std::vector<CampaignCommandPaletteItem> &items, const std::string &query, std::size_t limit)
{
    std::vector<CampaignCommandPaletteItem> matches;
    for (const CampaignCommandPaletteItem &item : items)
    {
        if (!containsText(item.label, query) && !containsText(item.category, query) && !containsText(item.entityId, query))
            continue;
        matches.push_back(item);
        if (matches.size() >= limit)
            break;
    }
    const std::string normalizedQuery = lowerCopy(query);
    const bool objectiveShortcut = normalizedQuery == "objectives" || normalizedQuery == "ziele";
    const bool clueShortcut = normalizedQuery == "clues" || normalizedQuery == "hinweise";
    if (objectiveShortcut || clueShortcut)
    {
        const std::string directId = objectiveShortcut ? "action:open_objectives" : "action:open_clues";
        std::stable_sort(matches.begin(), matches.end(), [&](const CampaignCommandPaletteItem &left,
                                                             const CampaignCommandPaletteItem &right) {
            const auto rank = [&](const CampaignCommandPaletteItem &item) {
                if (item.id == directId) return 0;
                if (item.id == "action:open_tabletop_toolkit") return 2;
                return 1;
            };
            return rank(left) < rank(right);
        });
    }
    return matches;
}
}
