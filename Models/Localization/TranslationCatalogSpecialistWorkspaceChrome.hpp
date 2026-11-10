#pragma once

#include "Models/Localization/TranslationCatalogData.hpp"

namespace gm::localization::detail
{
inline constexpr std::array<UiLiteralEntry, 15> TranslationCatalogSpecialistWorkspaceChrome = {{
    UiLiteralEntry{"Workspace summary", "Arbeitsbereichsübersicht"},
    UiLiteralEntry{"Story readiness", "Geschichtenbereitschaft"},
    UiLiteralEntry{"At the table", "Am Spieltisch"},
    UiLiteralEntry{"Atlas context", "Atlaskontext"},
    UiLiteralEntry{"Encounter readiness", "Begegnungsbereitschaft"},
    UiLiteralEntry{"Library usage", "Bibliotheksnutzung"},
    UiLiteralEntry{"Story arcs", "Handlungsbögen"},
    UiLiteralEntry{"Open a scene, finish its table notes, then stage it for Live.",
                   "Öffne eine Szene, vervollständige ihre Spieltisch-Notizen und bereite sie dann für Live vor."},
    UiLiteralEntry{"Player characters", "Spielfiguren"},
    UiLiteralEntry{"Creatures", "Kreaturen"},
    UiLiteralEntry{"Prepared encounters", "Vorbereitete Begegnungen"},
    UiLiteralEntry{"Eligible maps", "Geeignete Karten"},
    UiLiteralEntry{"Choose a dungeon, encounter, or city map. World maps remain in the atlas.",
                   "Wähle eine Dungeon-, Begegnungs- oder Stadtkarte. Weltkarten bleiben im Atlas."},
    UiLiteralEntry{"Finish creating this player or discard the draft before saving the campaign.",
                   "Schließe die Erstellung dieser Spielfigur ab oder verwirf den Entwurf, bevor du die Kampagne speicherst."},
    UiLiteralEntry{"Suggest player identity", "Identität vorschlagen"},
}};
} // namespace gm::localization::detail
