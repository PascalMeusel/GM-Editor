#include "compact_popup_platform.hpp"
// Only a CPU ImGui context: no application, GLFW, renderer, audio service,
// native platform windows, OS input, clipboard logging, or clipboard callbacks.
class HeadlessPanel
{
    ImGuiContext *context_ = ImGui::CreateContext();
    std::unique_ptr<CompactPopupPlatform> platform_;
    std::unique_ptr<UiInteractionTrace> trace_;
    std::string caseContext_="existing session case",stepContext_="initial";
    std::vector<UiInteractionTrace::Item> controls(const char *label, ImGuiID exactId = 0) const
    {
        if (!exactId) return trace_->find(label);
        const auto found = trace_->items().find(exactId);
        if (found != trace_->items().end() && found->second.hasRectangle) return {found->second};
        return {};
    }
public:
    std::function<void()> draw;
    std::string rendered;
    explicit HeadlessPanel(bool german,ImVec2 size=ImVec2(1500,1000),float scale=1.f,float baseFont=0.f)
    {
        gm::localization::LocalizationService::instance().setPreference(german ?
            gm::localization::LocalePreference::German : gm::localization::LocalePreference::English, false);
        auto &io = ImGui::GetIO(); io.IniFilename = nullptr;
        // Match ApplicationLifecycle: popup Escape cancellation is handled by
        // ImGui keyboard navigation, even when all activation uses the mouse.
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.DisplaySize = size; io.DeltaTime = 1.f / 60.f;
        if(baseFont>0.f){ImFontConfig font;font.SizePixels=baseFont;io.Fonts->AddFontDefault(&font);}
        ImGui::GetStyle().FontScaleMain=scale;
        auto &platform = ImGui::GetPlatformIO();
        platform.Platform_GetClipboardTextFn = nullptr; platform.Platform_SetClipboardTextFn = nullptr;
        platform.Platform_SetImeDataFn = nullptr; platform.Platform_OpenInShellFn = nullptr;
        unsigned char *pixels = nullptr; int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        require(pixels && width > 0 && height > 0, "CPU font atlas unavailable");
        trace_ = std::make_unique<UiInteractionTrace>(context_);
    }
    ~HeadlessPanel() { trace_.reset(); ImGui::DestroyContext(context_); }
    void compactOwner() { require(context_->FrameCount==0,"Configure synthetic viewport before first frame");
        platform_=std::make_unique<CompactPopupPlatform>(); }
    void context(std::string value){caseContext_=std::move(value);std::cout<<"UI group: "<<caseContext_<<'\n';}
    void step(std::string value){stepContext_=std::move(value);}
    [[noreturn]] void controlFailure(const char *reason,const char *label,ImGuiID exactId=0,const char *hint=nullptr) const
    {
        std::ostringstream s;s<<reason<<"\ncase="<<caseContext_<<" stage="<<stepContext_<<" label='"<<label<<"' exactId="<<exactId
            <<" hint="<<(hint?hint:"<none>")<<" active="<<context_->ActiveId<<" hovered="<<context_->HoveredId
            <<" nav="<<(context_->NavWindow?context_->NavWindow->Name:"<none>")<<" popupDepth="<<context_->OpenPopupStack.Size
            <<" frame="<<context_->FrameCount<<" viewport="<<ImGui::GetIO().DisplaySize.x<<"x"<<ImGui::GetIO().DisplaySize.y
            <<" main_work="<<ImGui::GetMainViewport()->WorkPos.x<<","<<ImGui::GetMainViewport()->WorkPos.y
            <<"+"<<ImGui::GetMainViewport()->WorkSize.x<<"x"<<ImGui::GetMainViewport()->WorkSize.y;
        const auto rect=[&](const ImRect &r){s<<"("<<r.Min.x<<","<<r.Min.y<<")-("<<r.Max.x<<","<<r.Max.y<<")";};
        const auto chain=[&](ImGuiWindow *window){for(int depth=0;window&&depth<8;++depth,window=window->ParentWindow) {
            s<<"\n  owner["<<depth<<"]="<<window->Name<<" flags="<<window->Flags<<" pos="<<window->Pos.x<<","<<window->Pos.y
                <<" size="<<window->Size.x<<"x"<<window->Size.y<<" clip=";rect(window->InnerClipRect);s<<" work=";rect(window->WorkRect);
            s<<" scroll="<<window->Scroll.x<<","<<window->Scroll.y<<" max="<<window->ScrollMax.x<<","<<window->ScrollMax.y
                <<" content="<<window->ContentSize.x<<","<<window->ContentSize.y<<" ideal="<<window->ContentSizeIdeal.x<<","<<window->ContentSizeIdeal.y
                <<" padding="<<window->WindowPadding.x<<","<<window->WindowPadding.y<<" font="<<window->FontRefSize
                <<" viewport_work="<<window->Viewport->WorkPos.x<<","<<window->Viewport->WorkPos.y
                <<"+"<<window->Viewport->WorkSize.x<<"x"<<window->Viewport->WorkSize.y;
        }};
        for(const auto &item:controls(label,exactId)){s<<"\ncandidate id="<<item.id<<" label='"<<item.label<<"' rect=";rect(ImRect(item.minimum,item.maximum));chain(ImGui::FindWindowByName(item.window.c_str()));}
        for(const auto &popup:context_->OpenPopupStack){s<<"\nopen popup id="<<popup.PopupId;chain(popup.Window);}
        if(hint)for(auto *window:context_->Windows)if(window->WasActive&&std::string(window->Name).find(hint)!=std::string::npos){s<<"\nscroll hint match";chain(window);}
        throw std::runtime_error(s.str());
    }
    void frame(int count = 1)
    {
        while (count--)
        {
            trace_->beginFrame(); ImGui::NewFrame();
            if(platform_)ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
            ImGui::SetNextWindowPos(platform_?ImGui::GetMainViewport()->WorkPos:ImVec2(0,0));
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("Presentation privacy fixture", nullptr,
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            ImGui::LogToBuffer();
            draw();
            // LogFinish deliberately clears its internal buffer. Copy it first.
            rendered.assign(context_->LogBuffer.c_str(), context_->LogBuffer.size());
            ImGui::LogFinish(); ImGui::End(); ImGui::Render();
            if(platform_)ImGui::UpdatePlatformWindows();
        }
    }
    void absent(const char *label) const
    { require(trace_->find(label).empty(), "A private edit control was submitted in presentation mode"); }
    bool hasControl(const char *label, ImGuiID exactId = 0) const { return !controls(label, exactId).empty(); }
    ImGuiID controlId(const char *label) const
    {
        const auto items = controls(label);
        if (items.size() != 1) throw std::runtime_error(std::string("Expected exactly one production privacy-test control: ") + label);
        return items.front().id;
    }
    void click(const char *label, ImGuiID exactId = 0)
    {
        // Settle hover/layout before sending a press to the observed widget.
        for (int settle = 0; settle < 3; ++settle)
        {
            const auto items = controls(label, exactId);
            if (items.size() != 1) controlFailure("Expected exactly one production privacy-test control",label,exactId);
            const auto &item = items.front();
            auto *window = ImGui::FindWindowByName(item.window.c_str());
            const ImVec2 center((item.minimum.x + item.maximum.x) * .5f, (item.minimum.y + item.maximum.y) * .5f);
            if(!window||!window->InnerClipRect.Contains(center))controlFailure("Privacy-test control is clipped",label,exactId);
            ImGui::GetIO().AddMousePosEvent(center.x, center.y); frame();
        }
        if(exactId&&context_->HoveredId!=exactId)controlFailure("Exact privacy-test control is not an enabled mouse target",label,exactId);
        ImGui::GetIO().AddMouseButtonEvent(0, true); frame();
        ImGui::GetIO().AddMouseButtonEvent(0, false); frame(2);
    }
    void type(const char *label, const char *text)
    {
        click(label); require(context_->ActiveId != 0, "Privacy-test input did not gain focus");
        auto &io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiMod_Ctrl, true); io.AddKeyEvent(ImGuiKey_A, true); frame();
        io.AddKeyEvent(ImGuiKey_A, false); io.AddKeyEvent(ImGuiMod_Ctrl, false); frame();
        if (*text) { io.AddInputCharactersUTF8(text); frame(2); }
        else { io.AddKeyEvent(ImGuiKey_Backspace, true); frame(); io.AddKeyEvent(ImGuiKey_Backspace, false); frame(); }
    }
    std::vector<UiInteractionTrace::Item> activeControls(const char *label) const
    {
        auto items=controls(label);
        auto *popup=context_->OpenPopupStack.empty()?nullptr:context_->OpenPopupStack.back().Window;
        items.erase(std::remove_if(items.begin(),items.end(),[&](const auto &item){
            auto *owner=ImGui::FindWindowByName(item.window.c_str());
            // ImGui also registers whole windows with their window ID/name.
            // Those diagnostic records are not controls inside the window.
            if(owner&&item.id==owner->ID)return true;
            if(!popup)return false;
            while(owner&&owner!=popup)owner=owner->ParentWindow;
            return owner!=popup;
        }),items.end());
        return items;
    }
    UiInteractionTrace::Item visibleItem(const char *label) const
    {
        const auto items=activeControls(label);
        if(items.size()!=1)controlFailure("Expected one active production control",label);
        return items.front();
    }
    void clickVisible(const char *label,const char *scrollHint=nullptr)
    {
        // A successful history command deliberately ends its popup frame early
        // to avoid stale model pointers. AlwaysAutoResize consumes that shorter
        // content on the next frame; let bounded idle layout settle before
        // acquiring any item/owner geometry. Persistent clipping still fails
        // through the same strict checks below, with no synthetic click.
        frame(3);
        if(activeControls(label).empty()) {
            auto *scroll=context_->OpenPopupStack.empty()?nullptr:context_->OpenPopupStack.back().Window;
            if(!scroll&&scrollHint)for(auto *window:context_->Windows)
                if(window->WasActive&&std::string(window->Name).find(scrollHint)!=std::string::npos)scroll=window;
            if(!scroll||scroll->ScrollMax.y<=0)controlFailure("Unsubmitted menu/combat control has no observed scrolling pane",label,0,scrollHint);
            for(int direction:{1,-1})for(int step=0;step<24&&activeControls(label).empty();++step) {
                const auto hover=scroll->InnerClipRect.GetCenter();const float before=scroll->Scroll.y;
                ImGui::GetIO().AddMousePosEvent(hover.x,hover.y);frame();
                ImGui::GetIO().AddMouseWheelEvent(0,float(-direction*4));frame(2);
                if(scroll->Scroll.y==before){scroll->Scroll.y=ImClamp(before+direction*scroll->FontRefSize*12.f,0.f,scroll->ScrollMax.y);frame(2);}
            }
        }
        for(int attempt=0;attempt<32;++attempt) {
            const auto item=visibleItem(label);auto *owner=ImGui::FindWindowByName(item.window.c_str());
            if(!owner)controlFailure("Observed menu/combat owner missing",label,item.id,scrollHint);
            const auto center=ImRect(item.minimum,item.maximum).GetCenter();
            if(item.minimum.x<owner->InnerClipRect.Min.x-1.f||item.maximum.x>owner->InnerClipRect.Max.x+1.f)
                controlFailure("Menu/combat production control is horizontally clipped",label,item.id,scrollHint);
            if(scrollHint) {
                auto *expected=owner;
                while(expected&&std::string(expected->Name).find(scrollHint)==std::string::npos)expected=expected->ParentWindow;
                if(!expected)controlFailure("Menu/combat scroll owner does not match the observed expected pane",label,item.id,scrollHint);
            }
            if(owner->InnerClipRect.Contains(center)) {click(label,item.id);return;}
            auto *scroll=owner;
            while(scroll->ParentWindow && scroll->ScrollMax.y<=0)scroll=scroll->ParentWindow;
            if(scroll->ScrollMax.y<=0)controlFailure("Clipped menu/combat control has no scrolling ancestor",label,item.id,scrollHint);
            const float before=scroll->Scroll.y;
            const float direction=center.y<owner->InnerClipRect.Min.y?4.f:-4.f;
            const auto hover=scroll->InnerClipRect.GetCenter();
            ImGui::GetIO().AddMousePosEvent(hover.x,hover.y);frame();
            ImGui::GetIO().AddMouseWheelEvent(0,direction);frame(2);
            // Same observed-child fallback as the established workspace harness.
            if(scroll->Scroll.y==before){scroll->Scroll.y=ImClamp(before-direction*scroll->FontRefSize*3.f,0.f,scroll->ScrollMax.y);frame(2);}
        }
        controlFailure("Cannot reach menu/combat production control",label,0,scrollHint);
    }
    void typeVisible(const char *label,const char *text)
    {
        clickVisible(label);const auto id=visibleItem(label).id;
        if(context_->ActiveId!=id)controlFailure("Menu/combat input lacks exact keyboard focus",label,id);
        auto &io=ImGui::GetIO();io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);frame();
        io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();
        io.AddInputCharactersUTF8(text);frame(2);
    }
    void escape()
    {
        auto &io=ImGui::GetIO();const auto active=context_->ActiveId;const int depth=context_->OpenPopupStack.Size;
        const std::string nav=context_->NavWindow?context_->NavWindow->Name:"<none>";
        io.AddKeyEvent(ImGuiKey_Escape,true);frame();io.AddKeyEvent(ImGuiKey_Escape,false);frame(2);
        if(depth&&!context_->OpenPopupStack.empty())std::cerr<<"Escape popup diagnostic: flags="<<io.ConfigFlags
            <<" depth="<<depth<<"->"<<context_->OpenPopupStack.Size<<" active="<<active<<"->"<<context_->ActiveId
            <<" nav="<<nav<<"->"<<(context_->NavWindow?context_->NavWindow->Name:"<none>")<<'\n';
    }
    bool popupOpen()const{return !context_->OpenPopupStack.empty();}
#include "player_review_headless_helpers.hpp"
#include "compact_popup_input_helpers.hpp"
    void boundedPopup(const char *anchor,bool picker,int settleFrames=3)
    {
        frame(settleFrames);const auto item=visibleItem(anchor);auto *window=ImGui::FindWindowByName(item.window.c_str());
        while(window&&!(window->Flags&ImGuiWindowFlags_Popup))window=window->ParentWindow;
        if(!window||context_->OpenPopupStack.Size!=1)controlFailure("Expected one actual production popup",anchor,item.id);
        const auto *viewport=ImGui::GetMainViewport();
        const float expected=std::min(32.f*window->FontRefSize,viewport->WorkSize.x*.9f);
        const bool fits=window->Size.x>200.f&&(!picker||std::abs(window->Size.x-expected)<1.f)&&
            window->Size.x<=viewport->WorkSize.x*.95f+1.f&&window->Size.y<=viewport->WorkSize.y*(picker?.85f:.9f)+1.f&&
            window->Pos.x>=viewport->WorkPos.x-1.f&&window->Pos.y>=viewport->WorkPos.y-1.f&&
            window->Pos.x+window->Size.x<=viewport->WorkPos.x+viewport->WorkSize.x+1.f&&
            window->Pos.y+window->Size.y<=viewport->WorkPos.y+viewport->WorkSize.y+1.f;
        if(!fits)controlFailure("Production popup is tiny or exceeds viewport bounds",anchor,item.id);
    }
};
