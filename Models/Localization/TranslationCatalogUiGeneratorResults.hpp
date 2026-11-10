#pragma once

#include "Models/Localization/TranslationCatalogData.hpp"

namespace gm::localization::detail
{
inline constexpr std::array<UiLiteralEntry, 45> TranslationCatalogUiGeneratorResults = {{
    // Deterministic map-generator output that is shown in candidate cards,
    // progress strips, and generated map metadata.
    UiLiteralEntry{"Generated Route", "Erzeugte Route"},
    UiLiteralEntry{"Representative travel route generated from the semantic way network.", "Repräsentative Reiseroute, die aus dem semantischen Wegenetz erzeugt wurde."},
    UiLiteralEntry{"Generated Cave", "Erzeugte Höhle"},
    UiLiteralEntry{"Editable cave network generated from sliders and a stable seed.", "Bearbeitbares Höhlennetz, das aus Reglern und einem festen Seed erzeugt wurde."},
    UiLiteralEntry{"Generated Cave Route", "Erzeugte Höhlenroute"},
    UiLiteralEntry{"Representative cave route generated from the semantic tunnel network.", "Repräsentative Höhlenroute, die aus dem semantischen Tunnelnetz erzeugt wurde."},
    UiLiteralEntry{"Primary route generated from the V2 feature graph.", "Hauptroute, die aus dem V2-Elementgraphen erzeugt wurde."},
    UiLiteralEntry{"Editable V2 map with deterministic terrain, hydrology, settlements, and cost-aware routes.", "Bearbeitbare V2-Karte mit reproduzierbarem Gelände, Gewässern, Siedlungen und kostenbewussten Routen."},
    UiLiteralEntry{"Shaping settlement terrain", "Siedlungsgelände wird geformt"},
    UiLiteralEntry{"Planning main streets", "Hauptstraßen werden geplant"},
    UiLiteralEntry{"Building districts and blocks", "Viertel und Häuserblöcke werden angelegt"},
    UiLiteralEntry{"Adding gates and landmarks", "Tore und Orientierungspunkte werden hinzugefügt"},
    UiLiteralEntry{"Checking settlement access", "Zugänglichkeit der Siedlung wird geprüft"},
    UiLiteralEntry{"Old Road", "Alte Straße"},
    UiLiteralEntry{"Temple Lane", "Tempelgasse"},
    UiLiteralEntry{"Mill Lane", "Mühlengasse"},
    UiLiteralEntry{"West Gate Road", "Straße zum Westtor"},
    UiLiteralEntry{"East Gate Road", "Straße zum Osttor"},
    UiLiteralEntry{"North Gate Road", "Straße zum Nordtor"},
    UiLiteralEntry{"South Gate Road", "Straße zum Südtor"},
    UiLiteralEntry{"Inner Ring", "Innerer Ring"},
    UiLiteralEntry{"City River", "Stadtfluss"},
    UiLiteralEntry{"River Road", "Flussstraße"},
    UiLiteralEntry{"Bridge Street", "Brückenstraße"},
    UiLiteralEntry{"South Bridge Road", "Straße zur Südbrücke"},
    UiLiteralEntry{"Harbor Road", "Hafenstraße"},
    UiLiteralEntry{"Quayside", "Kaiweg"},
    UiLiteralEntry{"Market Lane", "Marktgasse"},
    UiLiteralEntry{"Wall Road", "Mauerstraße"},
    UiLiteralEntry{"Market Road", "Marktstraße"},
    UiLiteralEntry{"Gate Road", "Torstraße"},
    UiLiteralEntry{"Market Quarter", "Marktviertel"},
    UiLiteralEntry{"Old Quarter", "Altstadtviertel"},
    UiLiteralEntry{"Temple Quarter", "Tempelviertel"},
    UiLiteralEntry{"Craft Quarter", "Handwerkerviertel"},
    UiLiteralEntry{"Settlement Center", "Siedlungszentrum"},
    UiLiteralEntry{"City Gate", "Stadttor"},
    UiLiteralEntry{"Main Gate Road", "Straße zum Haupttor"},
    UiLiteralEntry{"Stone road", "Steinstraße"},
    UiLiteralEntry{"Editable settlement site with districts, gates, connected streets, landmarks, and accessible building blocks.", "Bearbeitbarer Siedlungsschauplatz mit Vierteln, Toren, verbundenen Straßen, Orientierungspunkten und zugänglichen Häuserblöcken."},
    UiLiteralEntry{"Required feature", "Erforderliches Element"},
    UiLiteralEntry{"Generated Map", "Erzeugte Karte"},
    UiLiteralEntry{"Editable semantic map generated from sliders and a stable seed.", "Bearbeitbare semantische Karte, die aus Reglern und einem festen Seed erzeugt wurde."},
    UiLiteralEntry{"The usable map surface is split into disconnected areas.", "Die nutzbare Kartenfläche ist in voneinander getrennte Bereiche aufgeteilt."},
    UiLiteralEntry{"Regenerate terrain or add a valid crossing.", "Erzeuge das Gelände neu oder füge einen gültigen Übergang hinzu."},

}};
} // namespace gm::localization::detail
