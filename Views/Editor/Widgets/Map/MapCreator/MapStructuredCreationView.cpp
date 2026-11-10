#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "MapCreatorView.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorController.hpp"
#include "Models/Map/MapWorldLandform.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace {
const char *landformLabel(MapLandform shape)
{
    switch (shape) {
    case MapLandform::Islands: return uiTextForLocale("Islands", "Inseln");
    case MapLandform::InlandLake: return uiTextForLocale("Big lake", "Großer See");
    case MapLandform::SingleIsland: return uiTextForLocale("Single island", "Einzelne Insel");
    case MapLandform::Coast: return uiTextForLocale("Coast", "Küste");
    case MapLandform::Peninsula: return uiTextForLocale("Peninsula", "Halbinsel");
    default: return uiTextForLocale("Continental", "Kontinental");
    }
}

void layoutPreview(MapLandform shape, unsigned seed, int water, int forest, int mountain,
                   ImVec2 size, bool detail, bool regionalV12 = false)
{
    const auto p = ImGui::GetCursorScreenPos();
    auto *draw = ImGui::GetWindowDrawList();
    const int columns = detail ? 112 : 44, rows = detail ? 76 : 27;
    const auto point = [&](float x,float y) { return ImVec2(p.x+x*size.x,p.y+y*size.y); };
    draw->PushClipRect(p, ImVec2(p.x+size.x,p.y+size.y),true);
    draw->AddRectFilled(p,point(1,1),IM_COL32(61,122,137,255),6);
    for (int y=0;y<rows;++y) for (int x=0;x<columns;++x)
    {
        const float u=(x+.5f)/columns,v=(y+.5f)/rows;
        if (mapWorldLandformScore(shape,seed,u*2-1,v*2-1,water,regionalV12)<=0) continue;
        draw->AddRectFilled(point(float(x)/columns,float(y)/rows),
                           point(float(x+1)/columns,float(y+1)/rows),IM_COL32(224,214,168,255));
    }
    if (detail)
    {
        for (int i=0;i<155;++i)
        {
            unsigned h=seed+unsigned(i)*2654435761u; h^=h>>16; h*=2246822519u; h^=h>>13;
            const float u=.04f+float(h%1000)/1087.f,v=.04f+float((h/1000)%1000)/1087.f;
            if(mapWorldLandformScore(shape,seed,u*2-1,v*2-1,water,regionalV12)<.10f) continue;
            const auto pos=point(u,v);
            if(int((h>>20)%100)<forest)
                draw->AddCircleFilled(pos,std::max(2.f,size.x*.006f),IM_COL32(75,120,83,220),6);
            else if(int((h>>12)%100)<mountain)
                draw->AddTriangleFilled(ImVec2(pos.x-4,pos.y+3),ImVec2(pos.x,pos.y-5),ImVec2(pos.x+4,pos.y+3),IM_COL32(116,116,100,220));
        }
    }
    draw->PopClipRect();
    draw->AddRect(p,point(1,1),IM_COL32(109,139,140,180),6);
    ImGui::Dummy(size);
}
}

void MapCreatorView::drawStructuredEntities()
{
    auto &entities=_controller._generation.setupEntities;
    int remove=-1;
    for (int i=0;i<static_cast<int>(entities.size());++i)
    {
        const auto &entity=entities[i];
        ImGui::PushID(i);
        ImGui::TextWrapped("%s · %s",entity.name.c_str(),entity.nation
            ? uiTextForLocale("Nation","Nation") : uiTextForLocale("City","Stadt"));
        ImGui::SameLine();
        if (ImGui::SmallButton(uiTextForLocale("Remove","Entfernen"))) remove=i;
        ImGui::PopID();
    }
    if (remove>=0)
    {
        const auto removed = entities[remove];
        entities.erase(entities.begin()+remove);
        if (removed.nation && !removed.existing)
            for (auto &entity : entities)
                if (!entity.existing && entity.nationId==removed.id) entity.nationId.clear();
        _controller.markDraftEdited();
        _controller.synchronizeStructuredSetup();
        _controller.invalidateGenerationRequest();
    }
    if (drawUiButton(uiTextForLocale("+ City", "+ Stadt"),UiButtonRole::Secondary))
    { _controller._generation.setupEntityIsNation=false; _controller._generation.setupEntityName.clear(); _controller._generation.setupEntityNationId.clear(); ImGui::OpenPopup("MapSetupEntity"); }
    ImGui::SameLine();
    if (drawUiButton(uiTextForLocale("+ Nation", "+ Nation"),UiButtonRole::Secondary))
    { _controller._generation.setupEntityIsNation=true; _controller._generation.setupEntityName.clear(); ImGui::OpenPopup("MapSetupEntity"); }
    ImGui::SetNextWindowViewport(ImGui::GetWindowViewport()->ID);
    ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(.5f,.5f));
    if (!ImGui::BeginPopup("MapSetupEntity")) return;
    const bool nation=_controller._generation.setupEntityIsNation;
    const auto append=[&](const std::string &name,const EntityId &id,bool existing,const EntityId &nationId=EntityId{}) {
        const auto key=mapSetupNameKey(name);
        if (key.empty()) return;
        if (std::any_of(entities.begin(),entities.end(),[&](const auto &e){return mapSetupNameKey(e.name)==key;}))
        { ImGui::CloseCurrentPopup(); return; }
        entities.push_back({id,name,nation,existing,nationId});
        _controller.markDraftEdited();
        _controller.synchronizeStructuredSetup();
        _controller.invalidateGenerationRequest();
        ImGui::CloseCurrentPopup();
    };
    ImGui::TextUnformatted(nation ? uiTextForLocale("Add a nation","Eine Nation hinzufügen") : uiTextForLocale("Add a city","Eine Stadt hinzufügen"));
    ImGui::SetNextItemWidth(300);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    FormUi::InputText(uiTextForLocale("Name","Name"),&_controller._generation.setupEntityName);
    if (!nation)
    {
        std::string selectedNation=uiTextForLocale("Unassigned","Ohne Zuordnung");
        for (const auto &entry : entities)
            if (entry.nation && entry.id==_controller._generation.setupEntityNationId) selectedNation=entry.name;
        if (FormUi::BeginCombo(uiTextForLocale("Nation for a new city","Nation einer neuen Stadt"),selectedNation.c_str()))
        {
            if (ImGui::Selectable(uiTextForLocale("Unassigned","Ohne Zuordnung"),_controller._generation.setupEntityNationId.empty())) _controller._generation.setupEntityNationId.clear();
            for (const auto &entry : entities)
                if (entry.nation && ImGui::Selectable((entry.name+"##"+entry.id).c_str(),entry.id==_controller._generation.setupEntityNationId)) _controller._generation.setupEntityNationId=entry.id;
            ImGui::EndCombo();
        }
    }
    const auto inputKey=mapSetupNameKey(_controller._generation.setupEntityName);
    const bool duplicate=std::any_of(entities.begin(),entities.end(),[&](const auto &e){return mapSetupNameKey(e.name)==inputKey;});
    const bool full=std::count_if(entities.begin(),entities.end(),[&](const auto &e){return e.nation==nation;})>=12;
    const bool valid=!inputKey.empty() && !duplicate && !full;
    if(duplicate) ImGui::TextDisabled("%s",uiTextForLocale("Already included on this map","Bereits auf dieser Karte enthalten"));
    ImGui::BeginDisabled(!valid);
    if (drawUiButton(uiTextForLocale("Add to map","Zur Karte hinzufügen"),UiButtonRole::Primary))
    {
        auto name=_controller._generation.setupEntityName;
        name=name.substr(name.find_first_not_of(" \t\r\n")); name.erase(name.find_last_not_of(" \t\r\n")+1);
        const auto key=mapSetupNameKey(name);
        if(nation)
        {
            const auto &items=_controller._currentCampaign.nations;
            const auto found=std::find_if(items.begin(),items.end(),[&](const auto &n){return mapSetupNameKey(n.name)==key;});
            if(found!=items.end()) append(found->name,found->metadata.id,true);
            else append(name,EntityMetadata{"nation"}.id,false);
        }
        else
        {
            const auto &items=_controller._currentCampaign.worldPlaces;
            const auto found=std::find_if(items.begin(),items.end(),[&](const auto &n){return n.type!=WorldPlaceType::Landmark && n.type!=WorldPlaceType::Site && mapSetupNameKey(n.name)==key;});
            if(found!=items.end()) append(found->name,found->metadata.id,true,found->nationId);
            else append(name,EntityMetadata{"world_place"}.id,false,_controller._generation.setupEntityNationId);
        }
    }
    ImGui::EndDisabled();
    ImGui::SeparatorText(uiTextForLocale("From this campaign","Aus dieser Kampagne"));
    ImGui::BeginChild("MapSetupExisting",ImVec2(360,200));
    const auto choice=[&](const auto &entity,const EntityId &nationId=EntityId{}) {
        if(!mapSetupNameKey(_controller._generation.setupEntityName).empty() && mapSetupNameKey(entity.name).find(mapSetupNameKey(_controller._generation.setupEntityName))==std::string::npos) return;
        const bool selected=std::any_of(entities.begin(),entities.end(),[&](const auto &e){return e.id==entity.metadata.id || mapSetupNameKey(e.name)==mapSetupNameKey(entity.name);});
        ImGui::BeginDisabled(selected || full);
        if(ImGui::Selectable((entity.name+"##"+entity.metadata.id).c_str(),selected)) append(entity.name,entity.metadata.id,true,nationId);
        ImGui::EndDisabled();
    };
    if(nation) for(const auto &n:_controller._currentCampaign.nations) choice(n);
    else for(const auto &p:_controller._currentCampaign.worldPlaces)
        if(p.type!=WorldPlaceType::Landmark && p.type!=WorldPlaceType::Site) choice(p,p.nationId);
    ImGui::EndChild();
    ImGui::EndPopup();
}

void MapCreatorView::drawStructuredCreation()
{
    auto &map=_controller._draftMap;
    const auto setupChanged=[&]() {
        _controller.markDraftEdited();
        _controller.synchronizeStructuredSetup();
        _controller.invalidateGenerationRequest();
    };
    _controller._generation.structuredCreation=true;
    const float unit=std::max(1.f,ImGui::GetFontSize()/16.f);
    const bool overview=map.atlasLevel==MapLevel::World || map.atlasLevel==MapLevel::Region;
    if(overview && map.generation.recipe.landform==MapLandform::Automatic)
        map.generation.recipe.landform=MapLandform::Continental;
    const auto resize=[&]() {
        _draftWidth=std::clamp(_draftWidth,4,120); _draftHeight=std::clamp(_draftHeight,4,120);
        if(map.width!=_draftWidth || map.height!=_draftHeight)
        { _controller.captureUndoSnapshot("Resize Map"); map.width=_draftWidth; map.height=_draftHeight; _controller.resizeDraftCells(); }
    };
    ImGui::TextUnformatted(_controller._persistence.editingMapIndex < 0
        ? uiTextForLocale("Create a map","Eine Karte erstellen")
        : uiTextForLocale("Map setup","Karteneinstellungen"));
    ImGui::TextUnformatted(uiTextForLocale("Map name", "Kartenname"));
    ImGui::SetNextItemWidth(-1.0f);
    undoableInputText("##SetupMapName", map.name, "Edit Map Details");
    ImGui::SetNextItemWidth(std::min(420.f*unit,ImGui::GetContentRegionAvail().x));
    auto selectedPurposeLabel=localizedUiLabel(mapCreationIntentLabel(map.creationIntent));
    if(gm::mapui::mapCreatorExperimentalIntent(map.creationIntent))
        selectedPurposeLabel+=uiTextForLocale(" (Experimental)"," (Experimentell)");
    if(FormUi::BeginCombo("##SetupMapPurpose",selectedPurposeLabel.c_str()))
    {
        for(int i=0;i<=static_cast<int>(MapCreationIntent::TacticalEncounter);++i)
        {
            const auto intent=static_cast<MapCreationIntent>(i);
            auto label=localizedUiLabel(mapCreationIntentLabel(intent));
            if(gm::mapui::mapCreatorExperimentalIntent(intent)) label+=uiTextForLocale(" (Experimental)"," (Experimentell)");
            if(ImGui::Selectable(label.c_str(),map.creationIntent==intent))
            {
                _controller.captureUndoSnapshot("Edit Map Details");
                _controller._generation.selectedCreationIntentIndex=i; _controller.syncCreationIntentFromIndex(true);
                _draftWidth=map.width; _draftHeight=map.height; _controller.invalidateGenerationRequest();
            }
        }
        ImGui::EndCombo();
    }
    if (overview)
        ImGui::TextWrapped("%s", map.atlasLevel==MapLevel::World
            ? uiTextForLocale("World: an overview of nations and major cities. Choose Region for the places and journeys around your next adventure.",
                "Welt: Überblick über Nationen und große Städte. Wähle Region für Orte und Reisen rund um dein nächstes Abenteuer.")
            : uiTextForLocale("Region: a closer view for local places and journeys. It is generated separately; it is not an automatic crop of a world map.",
                "Region: eine nähere Ansicht für lokale Orte und Reisen. Sie wird separat erzeugt und ist kein automatischer Ausschnitt einer Weltkarte."));
    const float width=ImGui::GetContentRegionAvail().x;
    const bool wide=width>900.f*unit;
    const float footerHeight=ImGui::GetFrameHeightWithSpacing()+12+
        (_controller._integration.statusMessage.empty()?0.f:ImGui::GetTextLineHeightWithSpacing());
    const float bodyHeight=std::max(180.f,ImGui::GetContentRegionAvail().y-footerHeight);
    if(wide) { ImGui::BeginTable("MapSetupLayout",2,ImGuiTableFlags_SizingStretchProp); ImGui::TableSetupColumn("Controls",ImGuiTableColumnFlags_WidthFixed,410.f*unit); ImGui::TableSetupColumn("Preview"); ImGui::TableNextColumn(); }
    ImGui::BeginChild("MapSetupControls",ImVec2(0,bodyHeight));
    if(overview)
    {
        ImGui::TextUnformatted(uiTextForLocale("Geography","Geografie"));
        const int shapeColumns=ImGui::GetContentRegionAvail().x>=360.f*unit?3:2;
        ImGui::BeginTable("MapShapeChoices",shapeColumns,ImGuiTableFlags_SizingStretchSame);
        for(const auto shape:{MapLandform::Islands,MapLandform::Continental,MapLandform::InlandLake,
                             MapLandform::SingleIsland,MapLandform::Coast,MapLandform::Peninsula})
        {
            ImGui::TableNextColumn(); ImGui::PushID(static_cast<int>(shape));
            const auto start=ImGui::GetCursorScreenPos(); const float cardWidth=ImGui::GetContentRegionAvail().x;
            layoutPreview(shape,_controller._generation.generatorSeed,_controller._generation.generatorWaterLevel,0,0,ImVec2(cardWidth,70.f*unit),false,map.atlasLevel==MapLevel::Region);
            ImGui::SetCursorScreenPos(start);
            if(ImGui::InvisibleButton("##Shape",ImVec2(cardWidth,70.f*unit)))
            { if (map.generation.recipe.landform!=shape) { map.generation.recipe.landform=shape; setupChanged(); } }
            if(drawUiButton(landformLabel(shape),map.generation.recipe.landform==shape?UiButtonRole::Primary:UiButtonRole::Secondary,ImVec2(-1,0)))
            { if (map.generation.recipe.landform!=shape) { map.generation.recipe.landform=shape; setupChanged(); } }
            ImGui::PopID();
        }
        ImGui::EndTable();
        ImGui::TextWrapped("%s",map.generation.recipe.landform==MapLandform::Continental
            ?uiTextForLocale("Inland terrain continuing beyond the map edges.","Binnenland, das über die Kartenränder hinausreicht.")
            :map.generation.recipe.landform==MapLandform::InlandLake
            ?uiTextForLocale("One large lake surrounded by land.","Ein großer See, von Land umgeben.")
            :map.generation.recipe.landform==MapLandform::SingleIsland
            ?uiTextForLocale("One main island with sea on every side.","Eine Hauptinsel, ringsum von Meer umgeben.")
            :map.generation.recipe.landform==MapLandform::Coast
            ?uiTextForLocale("A coastline with inland terrain on one side and open sea on the other.","Eine Küste mit Binnenland auf einer Seite und offenem Meer auf der anderen.")
            :map.generation.recipe.landform==MapLandform::Peninsula
            ?uiTextForLocale("A tongue of land extending from the mainland into the sea.","Eine Landzunge, die vom Festland ins Meer reicht.")
            :uiTextForLocale("Several islands separated by sea.","Mehrere Inseln, durch Meer getrennt."));
    }
    ImGui::Spacing();
    const auto slider=[&](const char *label,int &value,int max=100,int min=0) { value=std::max(value,min); ImGui::TextUnformatted(label); ImGui::SetNextItemWidth(-1); if(FormUi::SliderInt((std::string("##")+label).c_str(),&value,min,max)) setupChanged(); };
    if(map.creationIntent==MapCreationIntent::TravelScene)
    {
        const char *purposes[] = {uiTextForLocale("Quiet journey", "Ruhige Reise"),
            uiTextForLocale("Discovery", "Entdeckung"), uiTextForLocale("Meeting", "Begegnung"),
            uiTextForLocale("Obstacle", "Hindernis"), uiTextForLocale("Danger", "Gefahr")};
        const char *biomes[] = {uiTextForLocale("Forest", "Wald"), uiTextForLocale("Highlands", "Hochland"),
            uiTextForLocale("Mountain pass", "Gebirgspass"), uiTextForLocale("River", "Fluss"),
            uiTextForLocale("Grassland", "Grasland"), uiTextForLocale("Lake", "See"), uiTextForLocale("Coast", "Küste")};
        _controller._generation.travelPurposeIndex=std::clamp(_controller._generation.travelPurposeIndex,0,4);
        _controller._generation.travelBiomeIndex=std::clamp(_controller._generation.travelBiomeIndex,0,6);
        ImGui::TextUnformatted(uiTextForLocale("What happens here?", "Was passiert hier?"));
        ImGui::SetNextItemWidth(-1);
        if(FormUi::Combo("##TravelPurpose",&_controller._generation.travelPurposeIndex,purposes,5)) setupChanged();
        ImGui::TextUnformatted(uiTextForLocale("Landscape", "Landschaft"));
        ImGui::SetNextItemWidth(-1);
        if(FormUi::Combo("##TravelBiome",&_controller._generation.travelBiomeIndex,biomes,7)) setupChanged();
    }
    else if(map.creationIntent==MapCreationIntent::TacticalEncounter)
    {
        const MapLandscapeProfile landscapes[] = {MapLandscapeProfile::ForestFrontier,
            MapLandscapeProfile::RiverValley, MapLandscapeProfile::Highlands};
        const char *templates[] = {uiTextForLocale("Forest road", "Waldweg"),
            uiTextForLocale("River crossing", "Flussübergang"), uiTextForLocale("Ruins", "Ruinen")};
        int selected = _controller._generation.generatorLandscapeIndex == static_cast<int>(MapLandscapeProfile::ForestFrontier) ? 0
            : (_controller._generation.generatorLandscapeIndex == static_cast<int>(MapLandscapeProfile::Highlands) ||
               _controller._generation.generatorLandscapeIndex == static_cast<int>(MapLandscapeProfile::MountainPass)) ? 2 : 1;
        ImGui::TextUnformatted(uiTextForLocale("Encounter setting", "Schauplatz"));
        ImGui::SetNextItemWidth(-1);
        if(FormUi::BeginCombo("##TacticalSetting", templates[selected]))
        {
            for(int i=0;i<3;++i)
                if(ImGui::Selectable(templates[i], i==selected))
                {
                    _controller._generation.generatorLandscapeIndex=static_cast<int>(landscapes[i]);
                    _controller._generation.generatorPurposeIndex=static_cast<int>(MapGenerationPurpose::Wilderness);
                    setupChanged();
                }
            ImGui::EndCombo();
        }
        slider(uiTextForLocale("Extra cover", "Zusätzliche Deckung"), _controller._generation.generatorTacticalDensity, 60);
        slider(uiTextForLocale("Objectives", "Ziele"), _controller._generation.generatorRequestedLandmarks, 3, 1);
    }
    else if(map.creationIntent==MapCreationIntent::Settlement)
    {
        slider(uiTextForLocale("Building density", "Bebauungsdichte"), _controller._generation.generatorSettlementDensity);
        slider(uiTextForLocale("Woodland", "Waldanteil"), _controller._generation.generatorForestDensity);
        slider(uiTextForLocale("Street complexity", "Straßennetz"), _controller._generation.generatorRouteDensity);
        showHelpTooltip(uiTextForLocale("Adds neighborhood streets. Main access streets remain connected.",
            "Ergänzt Straßen in den Vierteln. Die Hauptzufahrten bleiben verbunden."));
    }
    else if(map.scope!=MapScope::Dungeon)
    {
        const char *waterLabel = !overview ? uiTextForLocale("Water", "Wasser")
            : map.generation.recipe.landform == MapLandform::InlandLake ? uiTextForLocale("Lake size", "Seegröße")
            : (map.generation.recipe.landform == MapLandform::Islands || map.generation.recipe.landform == MapLandform::SingleIsland ||
               map.generation.recipe.landform == MapLandform::Coast || map.generation.recipe.landform == MapLandform::Peninsula)
                ? uiTextForLocale("Sea coverage", "Meeresanteil")
            : uiTextForLocale("Rivers and lakes", "Flüsse und Seen");
        slider(waterLabel,_controller._generation.generatorWaterLevel);
        slider(uiTextForLocale("Forests","Wälder"),_controller._generation.generatorForestDensity);
        slider(uiTextForLocale("Mountains","Berge"),_controller._generation.generatorMountainDensity);
        slider(uiTextForLocale("Cities and towns","Städte und Orte"),_controller._generation.generatorRequestedSettlements,12,
            overview?static_cast<int>(std::count_if(_controller._generation.setupEntities.begin(),_controller._generation.setupEntities.end(),[](const auto &e){return !e.nation;})):0);
        slider(uiTextForLocale("Roads and paths","Straßen und Wege"),_controller._generation.generatorRouteDensity);
    }
    else { if(map.creationIntent==MapCreationIntent::Cave) slider(uiTextForLocale("Water","Wasser"),_controller._generation.generatorWaterLevel); slider(uiTextForLocale("Passages","Gänge"),_controller._generation.generatorRouteDensity); slider(uiTextForLocale("Details and cover","Details und Deckung"),_controller._generation.generatorTacticalDensity); }
    if(overview && ImGui::TreeNode(uiTextForLocale("Cities and nations (optional)","Städte und Nationen (optional)")))
    { drawStructuredEntities(); ImGui::TreePop(); }
    if(ImGui::TreeNode(uiTextForLocale("Map options","Kartenoptionen")))
    {
        ImGui::SetNextItemWidth(-1);
        if(FormUi::Combo(uiTextForLocale("Size","Größe"),&_controller._generation.selectedSizePresetIndex,localizedComboItems("Small\0Standard\0Large\0Custom\0").c_str()))
        { _controller.captureUndoSnapshot("Resize Map"); _controller.syncCreationIntentFromIndex(true); _draftWidth=map.width; _draftHeight=map.height; setupChanged(); }
        if(map.sizePreset==MapSizePreset::Custom)
        {
            FormUi::InputInt(uiTextForLocale("Width","Breite"),&_draftWidth); bool commit=ImGui::IsItemDeactivatedAfterEdit();
            FormUi::InputInt(uiTextForLocale("Height","Höhe"),&_draftHeight); commit|=ImGui::IsItemDeactivatedAfterEdit(); if(commit) resize();
        }
        if(map.creationIntent!=MapCreationIntent::TacticalEncounter && map.creationIntent!=MapCreationIntent::TravelScene)
            slider(uiTextForLocale("Landmarks","Orientierungspunkte"),_controller._generation.generatorRequestedLandmarks,16);
        FormUi::Checkbox(uiTextForLocale("Keep random seed","Zufallswert behalten"),&_keepGenerationSeed);
        if(_keepGenerationSeed && FormUi::InputScalar(uiTextForLocale("Seed","Zufallswert"),ImGuiDataType_U32,&_controller._generation.generatorSeed)) setupChanged();
        if(drawUiButton(uiTextForLocale("Start with a blank map","Mit leerer Karte beginnen"),UiButtonRole::Secondary)) { resize(); setWorkflowStep(3); }
        ImGui::TreePop();
    }
    if(drawUiButton(uiTextForLocale("Import a map image…","Kartenbild importieren…"),UiButtonRole::Secondary)) _controller.browseForReferenceImage();
    if(!wide)
    {
        ImGui::TextUnformatted(uiTextForLocale("Layout preview · approximate", "Formvorschau · ungefähr"));
        drawSetupPreview(ImVec2(ImGui::GetContentRegionAvail().x,220.f*unit));
    }
    ImGui::EndChild();
    if(wide)
    {
        ImGui::TableNextColumn(); ImGui::BeginChild("MapSetupPreview",ImVec2(0,bodyHeight));
        ImGui::TextUnformatted(uiTextForLocale("Layout preview · approximate","Formvorschau · ungefähr"));
        drawSetupPreview(ImVec2(ImGui::GetContentRegionAvail().x,std::min(ImGui::GetContentRegionAvail().x*.7f,bodyHeight-90)));
        ImGui::TextWrapped("%s", map.creationIntent == MapCreationIntent::TravelScene
            ? uiTextForLocale("Choose what happens on the journey and the landscape. Designs provide different places for that scene.", "Wähle, was auf der Reise geschieht, und die Landschaft. Die Entwürfe bieten verschiedene Orte für diese Szene.")
            : uiTextForLocale("Choose the shape and amounts. Designs add detailed terrain, routes and places.","Wähle Form und Mengen. Die Entwürfe ergänzen Gelände, Wege und Orte im Detail."));
        ImGui::EndChild(); ImGui::EndTable();
    }
    if(!_controller._integration.statusMessage.empty())
    {
        ImGui::BeginChild("MapSetupStatus",ImVec2(0,ImGui::GetTextLineHeightWithSpacing()),ImGuiChildFlags_None,ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::TextUnformatted(_controller._integration.statusMessage.c_str());
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",_controller._integration.statusMessage.c_str());
        ImGui::EndChild();
    }
    if(drawUiButton(uiTextForLocale("Create designs","Entwürfe erstellen"),UiButtonRole::Primary,ImVec2(-1,0)))
    {
        resize(); _controller.synchronizeStructuredSetup();
        _previewedGenerationCandidate=0; _lastInteractivePreviewCandidate=-1; _resetDesignPreviewScroll=true;
        _controller.queueDraftGeneration(); setWorkflowStep(2);
    }
}
