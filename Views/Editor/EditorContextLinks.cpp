#include "Views/Editor/EditorViewDetails.hpp"
#include "Models/Project/WorldContextLinks.hpp"
#include "Models/UX/CampaignNavigationResolver.hpp"

using namespace gm::editor_view_detail;

void EditorWindowView::renderContextLinks(const EntityId &sourceId)
{
    const Campaign &campaign = _controller.getCurrentCampaign();
    if (!WorldContextLinks::supportsSource(campaign, sourceId)) return;
    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    if (!ImGui::CollapsingHeader(german ? "Verknüpfte Orte und Gruppen" : "Linked places and groups")) return;
    const auto apply = [&] {
        const EntityId source = _pendingContextSource, target = _pendingContextTarget;
        const auto kind = static_cast<WorldContextLinks::Kind>(_pendingContextKind);
        const auto result = _controller.applyWorldInspectorMutation([&](Campaign &proposed) {
            return WorldContextLinks::set(proposed, source, kind, target);
        }, "change contextual world link");
        reportWorldInspectorResult(result, "Link saved.", "Verknüpfung gespeichert.");
        if (result.applied()) _pendingContextKind = -1;
    };
    if (_pendingContextKind >= 0)
    {
        ImGui::TextWrapped("%s", german ? "Eine Verknüpfung wurde nicht gespeichert. Wiederhole den Versuch oder verwirf diese Änderung."
            : "A link was not saved. Retry or discard that change.");
        if (ImGui::Button(german ? "Verknüpfung erneut speichern" : "Retry link save")) apply();
        if (ImGui::Button(german ? "Verknüpfungsänderung verwerfen" : "Discard link change")) _pendingContextKind = -1;
    }
    for (int index = 0; index < 3; ++index)
    {
        const auto kind = static_cast<WorldContextLinks::Kind>(index);
        std::vector<std::pair<EntityId, std::string>> targets;
        const auto append = [&](const auto &entries) {
            for (const auto &entry : entries)
                if (entry.metadata.id != sourceId && gm::ux::resolveNavigationEntity(campaign, entry.metadata.id).available)
                    targets.emplace_back(entry.metadata.id, entry.name);
        };
        if (kind == WorldContextLinks::Kind::Place) append(campaign.worldPlaces);
        else if (kind == WorldContextLinks::Kind::Faction) append(campaign.factions);
        else append(campaign.shops);
        const EntityId current = WorldContextLinks::target(campaign, sourceId, kind);
        std::string preview = current.empty() ? (german ? "Keine" : "None") : (german ? "Nicht verfügbar" : "Unavailable");
        for (const auto &entry : targets) if (entry.first == current) preview = entry.second;
        const char *english[] = {"Place", "Faction", "Shop"};
        const char *germanLabels[] = {"Ort", "Fraktion", "Händler"};
        ImGui::PushID(index);
        ImGui::BeginDisabled(_pendingContextKind >= 0);
        EntityId requested; bool changed = false;
        if (FormUi::BeginCombo(german ? germanLabels[index] : english[index], preview.c_str()))
        {
            if (ImGui::Selectable(german ? "Keine" : "None", current.empty())) changed = !current.empty();
            for (const auto &entry : targets)
            {
                const std::string label = entry.second + "##" + entry.first;
                if (ImGui::Selectable(label.c_str(), entry.first == current)) { requested = entry.first; changed = requested != current; }
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        if (changed) { _pendingContextSource = sourceId; _pendingContextTarget = requested; _pendingContextKind = index; apply(); }
        if (!current.empty() && gm::ux::resolveNavigationEntity(campaign, current).available &&
            ImGui::Button(german ? "Verknüpften Eintrag öffnen" : "Open linked entry"))
            _controller.openWorkspaceEntity(WorldContextLinks::targetType(kind), current);
        ImGui::PopID();
    }
}
