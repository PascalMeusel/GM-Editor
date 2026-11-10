#pragma once
#include "Models/Character/PortraitRecipe.hpp"
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "Views/Editor/Widgets/UiTaskLayout.hpp"
#include "imgui.h"
#include "Models/Character/PortraitArtwork.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Views/Editor/Widgets/ImageTextureCache.hpp"
#include "Views/Editor/Widgets/UiPortraitPicker.hpp"

namespace PortraitUi
{
inline const char *text(const char *en, const char *de)
{
    return uiTextForLocale(en, de);
}
inline std::filesystem::path artworkPath(int species, int collection = 0)
{
    species = std::clamp(species, 0, gm::portraits::speciesCount - 1);
    collection = std::clamp(collection, 0, gm::portraits::currentCollection);
    const auto relative = std::filesystem::path("Resources/Portraits") /
        gm::portraits::collectionFolders[collection] / gm::portraits::artworkFiles[species];
    for (const auto &root : ApplicationPaths::runtimeSearchRoots())
    {
        std::error_code error;
        const auto candidate = root / relative;
        if (std::filesystem::is_regular_file(candidate, error)) return candidate;
    }
    return ApplicationPaths::executableDirectory() / relative;
}
inline void draw(const gm::portraits::Recipe &recipe, float size = 168.f)
{
    size = std::min(size, std::max(1.f, ImGui::GetContentRegionAvail().x));
    const auto &art = getCachedImageTexture(artworkPath(recipe.species, recipe.collection));
    const ImVec2 dimensions(size, size * 1.5f);
    if (art.loaded)
    {
        const auto region = gm::portraits::artworkRegion(recipe.expression, art.width, art.height);
        ImGui::Image(art.textureId, dimensions, ImVec2(region.u0, region.v0), ImVec2(region.u1, region.v1));
    }
    else
    {
        ImGui::Dummy(dimensions);
        ImGui::TextWrapped("%s", text("This portrait is not included in the installed collection. Your saved choice is preserved; you can select another portrait below.", "Dieses Porträt ist in der installierten Sammlung nicht enthalten. Die gespeicherte Auswahl bleibt erhalten; unten kann ein anderes Porträt gewählt werden."));
    }
}
inline void preview(const EntityMetadata &metadata, float size = 168.f)
{
    if (gm::portraits::exists(metadata))
        draw(gm::portraits::read(metadata), size);
}
inline bool editor(EntityMetadata &metadata)
{
    ImGui::PushID("PortraitRecipe");
    auto r = gm::portraits::read(metadata);
    bool changed = false;
    ImGui::SeparatorText(text("Portrait", "Porträt"));
    const bool wide = ImGui::GetContentRegionAvail().x >= 440.0f;
    const bool columns = wide && ImGui::BeginTable("PortraitChoices", 2, ImGuiTableFlags_SizingStretchProp);
    if (columns)
    {
        ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 176.0f);
        ImGui::TableSetupColumn("Choices", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
    }
    draw(r, columns ? 168.0f : 96.0f);
    if (columns) ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped(
        "%s", text("Two portrait collections · eight people per species in each", "Zwei Porträtsammlungen · je acht Personen pro Spezies"));
    ImGui::PopStyleColor();
    const auto &speciesEn = gm::portraits::speciesNames;
    const char *speciesDe[] = {"Mensch", "Elf", "Zwerg", "Ork", "Tiefling", "Drachenblütiger", "Tabaxi", "Untoter", "Aasimar", "Gnom", "Goliath", "Halbling", "Halbelf", "Halbork", "Goblin", "Hobgoblin", "Grottenschrat", "Kobold", "Aarakocra", "Kenku", "Tortle", "Echsenmensch", "Firbolg", "Genasi", "Gith", "Triton", "Yuan-ti", "Changeling", "Shifter", "Kriegsgeschmiedeter", "Satyr", "Fee", "Harengon", "Owlin", "Leonin", "Minotaurus", "Zentaur", "Loxodon", "Giff", "Thri-kreen", "Plasmoid", "Autognom", "Hadozee", "Kalashtar", "Verdan", "Locathah", "Grung"};
    const auto combo = [&](const char *label, const char *id, int &v, const char *const *en, const char *const *de, int count, bool installedSpeciesOnly = false) {
        TaskUi::fieldLabel(label);
        const auto items =
            gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? de : en;
        ImGui::PushID(label);
        const ImGuiID choiceId=ImGui::GetID(id);
        const bool open=ImGui::BeginCombo(id, items[v]);
        ImGuiContext &g=*ImGui::GetCurrentContext();
        IMGUI_TEST_ENGINE_ITEM_INFO(choiceId,id,0);
        if (open)
        {
            for (int i = 0; i < count; ++i)
            {
                std::error_code error;
                if (installedSpeciesOnly && !std::filesystem::is_regular_file(artworkPath(i, r.collection), error))
                    continue;
                if (ImGui::Selectable(items[i], v == i))
                {
                    v = i;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
    };
    combo(text("Portrait species", "Porträt-Spezies"), "##PortraitSpecies", r.species, speciesEn, speciesDe, gm::portraits::speciesCount, true);
    const char *collectionsEn[] = {"Original collection", "New company"};
    const char *collectionsDe[] = {"Ursprüngliche Sammlung", "Neue Gefährten"};
    combo(text("Portrait collection", "Porträtsammlung"), "##PortraitCollection", r.collection, collectionsEn, collectionsDe,
          gm::portraits::currentCollection + 1);
    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    changed = identityPicker(r.expression, getCachedImageTexture(artworkPath(r.species, r.collection)), german) || changed;
    if (!gm::portraits::exists(metadata) && ImGui::Button(text("Use this portrait", "Porträt übernehmen")))
        changed = true;
    if (columns) ImGui::EndTable();
    if (changed)
    {
        gm::portraits::write(metadata, r);
        DraftEditSignal::record(true);
    }
    ImGui::PopID();
    return changed;
}
} // namespace PortraitUi
