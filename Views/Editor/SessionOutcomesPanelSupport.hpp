#pragma once
#include "Views/Editor/SessionOutcomesPanel.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/TextDraftRecovery.hpp"
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui.h"
#include <algorithm>

namespace
{
const char *ot(const char *en, const char *de)
{ return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? de : en; }
bool ob(const char *en, const char *de, bool primary = false)
{ return drawUiButton(ot(en, de), primary ? UiButtonRole::Primary : UiButtonRole::Secondary); }
void ol(const char *en, const char *de) { ImGui::TextUnformatted(ot(en, de)); ImGui::SetNextItemWidth(-1); }
std::string targetLabel(const Campaign &c, const SessionOutcomes::Target &target)
{
    using T = SessionOutcomes::TargetType;
    if (target.type == T::None) return ot("Session only", "Nur Sitzung");
    if (target.type == T::Npc) for (const auto &v : c.npcs) if (v.metadata.id == target.id) return v.name;
    if (target.type == T::Place) for (const auto &v : c.worldPlaces) if (v.metadata.id == target.id) return v.name;
    if (target.type == T::Objective) for (const auto &v : c.storyObjectives) if (v.metadata.id == target.id) return v.title;
    if (target.type == T::Clue) for (const auto &v : c.storyClues) if (v.metadata.id == target.id) return v.title;
    return ot("Unavailable linked entry", "Verknüpfter Eintrag nicht verfügbar");
}
const char *stateLabel(SessionOutcomes::State state)
{
    if (state == SessionOutcomes::State::Applied) return ot("Applied", "Übernommen");
    if (state == SessionOutcomes::State::Rejected) return ot("Rejected", "Verworfen");
    return ot("Awaiting review", "Noch zu prüfen");
}
SessionOutcomes::Action defaultAction(SessionOutcomes::TargetType target)
{
    using namespace SessionOutcomes;
    switch (target)
    {
    case TargetType::Npc: return Action::NpcAttitude;
    case TargetType::Place: return Action::PlaceNote;
    case TargetType::Objective: return Action::ObjectiveState;
    case TargetType::Clue: return Action::ClueStatus;
    default: return Action::RecordOnly;
    }
}
const char *actionLabel(SessionOutcomes::Action action)
{
    using A = SessionOutcomes::Action;
    switch (action)
    {
    case A::NpcAttitude: return ot("NPC attitude", "NSC-Haltung");
    case A::NpcNote: return ot("NPC dialogue notes", "NSC-Gesprächsnotizen");
    case A::PlaceNote: return ot("Place notes", "Ortsnotizen");
    case A::ObjectiveState: return ot("Objective status", "Zielstatus");
    case A::ClueStatus: return ot("Clue status", "Hinweisstatus");
    default: return ot("Session record", "Sitzungseintrag");
    }
}
std::string valueLabel(SessionOutcomes::Action action, const std::string &value)
{
    using A = SessionOutcomes::Action;
    if (action == A::ObjectiveState)
    {
        if (value == "0") return ot("Planned", "Geplant");
        if (value == "1") return ot("Active", "Aktiv");
        if (value == "2") return ot("Completed", "Abgeschlossen");
        if (value == "3") return ot("Failed", "Gescheitert");
    }
    if (action == A::ClueStatus)
    {
        if (value == "0") return ot("Hidden", "Verborgen");
        if (value == "1") return ot("Discovered", "Entdeckt");
        if (value == "2") return ot("Shared", "Geteilt");
    }
    return value.empty() ? ot("(empty)", "(leer)") : value;
}
}
