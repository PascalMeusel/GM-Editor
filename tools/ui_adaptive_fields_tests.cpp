#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "imgui_internal.h"
#include <iostream>

int main()
{
    int failures=0;
    const auto check=[&](bool ok,const char *message) {
        if(!ok) {++failures;std::cerr<<message<<'\n';}
    };
    ImGui::CreateContext();
    auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DeltaTime=1.f/60;
    io.DisplaySize={1280,720};
    unsigned char *pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    for(float scale:{1.f,1.5f,2.f})
    for(float width:{180.f,280.f,430.f,720.f})
    for(int frame=0;frame<2;++frame)
    {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({width,700});
        ImGui::Begin("Adaptive inspector",nullptr,ImGuiWindowFlags_NoDecoration);
        ImGui::SetWindowFontScale(scale);
        ImGui::Indent(20*scale); // Reproduce the nested Advanced section.
        const float right=ImGui::GetCursorScreenPos().x+ImGui::GetContentRegionAvail().x;
        const auto fits=[&]() {check(ImGui::GetItemRectMax().x<=right+.5f,"Control or label exceeds inspector width");};
        std::string text="Draft";float value=55;int number=3;double real=1;
        for(const char *label:{"Linked Entity Type", "Verknuepfte Kampagnenfigur", "Title##Scene", "Title###StableScene"})
        {
            const auto expected=ImGui::GetID(label);
            const float top=ImGui::GetCursorScreenPos().y;
            ImGui::SetNextItemWidth(-1);
            FormUi::InputText(label,&text);
            fits();check(ImGui::GetItemID()==expected,"Stacking changed a field ID");
            check(ImGui::GetItemRectMin().y>top,"Full-width labelled input must put the label above");
        }
        ImGui::SetNextItemWidth(500);
        const auto sliderId=ImGui::GetID("Token Size");
        FormUi::SliderFloat("Token Size",&value,.5f,4.f);fits();
        check(ImGui::GetItemID()==sliderId,"Slider lost its stable ID");
        ImGui::SetNextItemWidth(500);
        FormUi::InputInt("Maximum participants",&number);fits();
        FormUi::InputDouble("Audio fade duration",&real);fits();
        FormUi::Combo("Encounter difficulty",&number,"One\0Two\0Three\0Four\0");fits();
        FormUi::InputTextMultiline("Description and GM notes",&text,{1000,36});fits();
        const float top=ImGui::GetCursorScreenPos().y;
        ImGui::SetNextItemWidth(1000);
        FormUi::InputText("##AlreadyLabelled",&text);fits();
        check(ImGui::GetItemRectMin().y==top,"Hidden label unexpectedly added another row");
        ImGui::Unindent(20*scale);
        ImGui::End();ImGui::Render();
    }
    // Table-cell widths and explicit short controls must also be respected.
    ImGui::NewFrame();ImGui::SetNextWindowSize({600,650});
    ImGui::Begin("Tables");
    if(ImGui::BeginTable("Fields",2,ImGuiTableFlags_SizingStretchSame))
    {
        for(int column=0;column<2;++column)
        {
            ImGui::TableNextColumn();ImGui::PushID(column);
            const float right=ImGui::GetCursorScreenPos().x+ImGui::GetContentRegionAvail().x;
            ImGui::SetNextItemWidth(-1);int value=0;
            FormUi::InputInt("Verknuepfte Gegenstaende",&value);
            check(ImGui::GetItemRectMax().x<=right+.5f,"Table field spills into adjacent column");
            const float top=ImGui::GetCursorScreenPos().y;
            ImGui::SetNextItemWidth(70);
            FormUi::SliderInt("X",&value,0,80);
            check(ImGui::GetItemRectMin().y==top,"Short label should stay inline when it fits");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::End();ImGui::Render();
    // Wrapped labels remain fully clickable, with stable IDs at either width.
    bool enabled=false;ImGuiID toggleId=0;ImVec2 labelClick;
    for(int frame=0;frame<6;++frame)
    {
        if(frame==3) {io.AddMousePosEvent(labelClick.x,labelClick.y);io.AddMouseButtonEvent(0,true);}
        if(frame==4) io.AddMouseButtonEvent(0,false);
        ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});
        ImGui::SetNextWindowSize({frame==0?1000.f:250.f,600});
        ImGui::Begin("Wrapped toggles",nullptr,ImGuiWindowFlags_NoDecoration);
        if(frame==1) ImGui::SetWindowFocus();
        const float right=ImGui::GetCursorScreenPos().x+ImGui::GetContentRegionAvail().x;
        FormUi::Checkbox("Hide this linked campaign character from all players",&enabled);
        check(ImGui::GetItemRectMax().x<=right+.5f,"Checkbox text spills out of panel");
        if(frame==0) toggleId=ImGui::GetItemID();
        else check(ImGui::GetItemID()==toggleId,"Wrapping changed checkbox identity");
        if(frame==2) labelClick={ImGui::GetItemRectMin().x+60,ImGui::GetItemRectMin().y+8};
        FormUi::RadioButton("Verknuepfte Kampagnenfigur automatisch verwenden",true);
        check(ImGui::GetItemRectMax().x<=right+.5f,"Radio label spills out of panel");
        ImGui::End();ImGui::Render();
    }
    check(enabled,"Wrapped checkbox label stopped being clickable");
    // Opening a stacked combo must preserve both parent and popup ID stacks.
    ImVec2 comboClick;
    bool popupSeen=false;
    for(int frame=0;frame<6;++frame)
    {
        if(frame==2) {io.AddMousePosEvent(comboClick.x,comboClick.y);io.AddMouseButtonEvent(0,true);}
        if(frame==3) io.AddMouseButtonEvent(0,false);
        ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({280,500});
        ImGui::Begin("Combo",nullptr,ImGuiWindowFlags_NoDecoration);
        if(frame==0) ImGui::SetWindowFocus();
        ImGui::SetNextItemWidth(-1);
        auto *parent=ImGui::GetCurrentWindow();const int depth=parent->IDStack.Size;
        if(FormUi::BeginCombo("Linked Entity Type","Character"))
        {
            popupSeen=true;ImGui::Selectable("Character");
            if(frame==5) ImGui::CloseCurrentPopup();
            ImGui::EndCombo();
        }
        else if(frame==1) comboClick={ImGui::GetItemRectMin().x+40,ImGui::GetItemRectMin().y+10};
        check(parent->IDStack.Size==depth,"Combo leaked a parent ID stack entry");
        ImGui::End();ImGui::Render();
    }
    check(popupSeen,"Stacked combo stopped opening");
    // Clear combo focus before exercising text entry in another window.
    io.AddMousePosEvent(-100,-100);
    // Typing and keyboard focus survive switching between wide/stacked layout.
    std::string draft="Keep ";ImGuiID active=0;
    for(int frame=0;frame<6;++frame)
    {
        if(frame==3) io.AddInputCharactersUTF8("editing");
        ImGui::NewFrame();ImGui::SetNextWindowSize({frame<3?900.f:220.f,400});
        ImGui::Begin("Focused field");
        if(frame==1) {ImGui::SetWindowFocus();ImGui::SetKeyboardFocusHere();}
        ImGui::SetNextItemWidth(200);
        FormUi::InputText("A translated entity link label",&draft);
        if(frame==2) active=ImGui::GetActiveID();
        if(frame>=3) check(active!=0 && ImGui::GetActiveID()==active,"Resize interrupted keyboard editing");
        ImGui::End();ImGui::Render();
    }
    check(draft.find("editing")!=std::string::npos,"Responsive field lost typed text");
    ImGui::DestroyContext();
    std::cout<<"Adaptive field failures: "<<failures<<'\n';
    return failures?1:0;
}
