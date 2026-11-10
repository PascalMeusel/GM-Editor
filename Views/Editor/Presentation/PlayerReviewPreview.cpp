#include "Views/Editor/Presentation/PlayerReviewContent.hpp"
#include "Views/Editor/WorkspaceUi.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorOutputPreviewRendering.hpp"
#include "Views/Editor/Widgets/Map/MapTokenLegend.hpp"
#include "Views/Editor/Widgets/ImageTextureCache.hpp"
#include <cmath>
namespace gm::player_review_ui
{
namespace
{
void mapPreview(const char *id,const MapModel &map,const MapRenderPlan &plan,const FrozenImageAsset &asset,float height)
{
    if(ImGui::BeginChild(id, ImVec2(-1.0f, std::max(120.0f, height)), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float cellSize = std::max(0.1f, std::min(available.x / static_cast<float>(std::max(1, map.width)),
                                                   available.y / static_cast<float>(std::max(1, map.height))));
    const ImVec2 mapSize(cellSize * static_cast<float>(std::max(1, map.width)),
                         cellSize * static_cast<float>(std::max(1, map.height)));
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const ImVec2 origin(cursor.x + std::max(0.0f, (available.x - mapSize.x) * 0.5f),
                        cursor.y + std::max(0.0f, (available.y - mapSize.y) * 0.5f));
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(origin, ImVec2(origin.x + mapSize.x, origin.y + mapSize.y), IM_COL32(27, 31, 33, 255));

    bool backgroundDrawn = false;
    const char *backgroundStatus=nullptr;
    if (plan.backgroundVisible)
    {
        const std::string &storedPath = plan.backgroundInteractiveProjectRelativePath;
        if (!storedPath.empty()&&ImGui::IsRectVisible(origin,ImVec2(origin.x+mapSize.x,origin.y+mapSize.y)))
        {
            const CachedImageTexture &texture = asset.valid()?getAsyncImageTexture(asset,
                static_cast<std::uint64_t>(ImGui::GetFrameCount())):findCachedImageTexture(asset);
            if (texture.loaded&&texture.textureId&&texture.width>0&&texture.height>0)
            {
                const MapNormalizedRect crop =
                    map.importedReference.enabled ? map.importedReference.crop : MapNormalizedRect{};
                const float rotation = map.importedReference.enabled ? map.importedReference.rotationDegrees : 0.0f;
                const float radians = -rotation * 0.01745329251994329577f;
                const float cosine = std::cos(radians);
                const float sine = std::sin(radians);
                const auto uv = [&](float x, float y) {
                    const float centeredX = x - 0.5f;
                    const float centeredY = y - 0.5f;
                    const float rotatedX = cosine * centeredX - sine * centeredY + 0.5f;
                    const float rotatedY = sine * centeredX + cosine * centeredY + 0.5f;
                    return ImVec2(crop.x + rotatedX * crop.width, crop.y + rotatedY * crop.height);
                };
                const ImU32 tint = ImGui::GetColorU32(
                    ImVec4(1.0f, 1.0f, 1.0f,
                           map.importedReference.enabled ? 1.0f : std::clamp(map.backgroundImageOpacity, 0.0f, 1.0f)));
                drawList->AddImageQuad(texture.textureId, origin, ImVec2(origin.x + mapSize.x, origin.y),
                                       ImVec2(origin.x + mapSize.x, origin.y + mapSize.y),
                                       ImVec2(origin.x, origin.y + mapSize.y), uv(0.0f, 0.0f), uv(1.0f, 0.0f),
                                       uv(1.0f, 1.0f), uv(0.0f, 1.0f), tint);
                backgroundDrawn = true;
            }
            else backgroundStatus=asset.valid()&&!texture.loadAttempted?
                uiTextForLocale("Loading reviewed map image…","Geprüftes Kartenbild wird geladen…"):
                uiTextForLocale("Reviewed map image is unavailable.","Das geprüfte Kartenbild ist nicht verfügbar.");
        }
    }

    gm::mapui::drawMapCreatorOutputPreview(drawList, origin, map, plan, cellSize, backgroundDrawn,
                                           plan.applyFog ? getMapFogTexture(false).textureId : 0, true);
    drawList->AddRect(origin, ImVec2(origin.x + mapSize.x, origin.y + mapSize.y), IM_COL32(120, 128, 130, 220));
    if(backgroundStatus){
        drawList->PushClipRect(origin,ImVec2(origin.x+mapSize.x,origin.y+mapSize.y),true);
        drawList->AddText(nullptr,0.f,ImVec2(origin.x+4,origin.y+4),ImGui::GetColorU32(ImGuiCol_Text),
            backgroundStatus,nullptr,std::max(1.f,mapSize.x-8));drawList->PopClipRect();
    }
    }
    ImGui::EndChild();
    ImGui::PushID(id);
    gm::mapui::drawMapTokenLegend(map, plan);
    ImGui::PopID();
}

}
void drawMaps(const PlayerReviewSession &session)
{
    const auto *gm=session.gmMap(),*player=session.playerMap();
    const auto &review=session.review();
    if(!gm||!player)return;
    const auto playerAsset=session.playerImage(review.playerMapPlan.backgroundInteractiveProjectRelativePath,
        review.playerMapPlan.backgroundInteractiveContentHash);
    const bool columns=ImGui::GetContentRegionAvail().x>=760.f;
    const auto draw=[&](bool privateView,float height){
        ImGui::TextUnformatted(privateView?localizedUiText("GM view (private)"):uiTextForLocale("Player preview","Spielervorschau"));
        mapPreview(privateView?"ReviewGmMap":"ReviewPlayerMap",privateView?*gm:*player,
            privateView?review.gmMapPlan:review.playerMapPlan,privateView?session.gmBackground():playerAsset,height);
    };
    if(columns&&ImGui::BeginTable("ReviewMaps",2,ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_NoSavedSettings))
    {ImGui::TableNextColumn();draw(true,250);ImGui::TableNextColumn();draw(false,250);ImGui::EndTable();}
    else if(!columns){draw(true,160);draw(false,160);}
    if(session.gmArtOmitted())ImGui::TextWrapped("%s",uiTextForLocale(
        "Private map artwork could not be frozen and is omitted from this preview.",
        "Das private Kartenbild konnte nicht gesichert werden und fehlt in dieser Vorschau."));
    ImGui::SeparatorText(localizedUiText("Visibility check"));
    const auto count=[](const MapRenderPlan &p){return p.generatedFeatureIds.size()+p.manualFeatureIds.size()+p.regionIds.size()+p.routeIds.size()+p.tokenIds.size();};
    ImGui::TextWrapped(uiTextForLocale("Reviewed map objects: %d public, %d private or unrevealed.",
        "Geprüfte Kartenobjekte: %d öffentlich, %d privat oder unentdeckt."),int(count(review.playerMapPlan)),
        int(count(review.gmMapPlan)-std::min(count(review.gmMapPlan),count(review.playerMapPlan))));
}
}
