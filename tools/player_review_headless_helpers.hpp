// Included inside HeadlessPanel. These helpers use only observed production
// controls and queued ImGui input; no OS input, NavId assignment or approval.
    void waitReview(PlayerReviewSession &owner)
    {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(owner.preparing()&&std::chrono::steady_clock::now()<deadline)
        {frame();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        require(!owner.preparing(),"Production review preparation exceeded bounded test watchdog");
        frame(2);
    }
    void reviewVisible(const UiInteractionTrace::Item &item) const
    {
        const auto *owner=ImGui::FindWindowByName(item.window.c_str());
        const auto *vp=owner?owner->Viewport:nullptr;
        const ImRect bounds(item.minimum,item.maximum);
        if(!owner||!vp||!owner->InnerClipRect.Contains(bounds)||
            !ImRect(vp->WorkPos,ImVec2(vp->WorkPos.x+vp->WorkSize.x,vp->WorkPos.y+vp->WorkSize.y)).Contains(bounds))
            controlFailure("Review control is not fully inside owner clip/work area",item.label.c_str(),item.id);
    }
    void reviewState(const char *label,bool disabled)
    {
        ImGuiID id=0;
        for(int settle=0;settle<3;++settle){const auto item=visibleItem(label);reviewVisible(item);
            require(!id||id==item.id,"Review widget changed identity while checking state");id=item.id;
            const auto center=ImRect(item.minimum,item.maximum).GetCenter();
            ImGui::GetIO().AddMousePosEvent(center.x,center.y);frame();}
        if(context_->HoveredId!=id||context_->HoveredIdIsDisabled!=disabled)
            controlFailure("Review control enabled state differs from observed hover",label,id);
    }
    void reviewClickRelease(const char *label)
    {
        reviewState(label,false);
        ImGui::GetIO().AddMouseButtonEvent(0,true);frame();
        ImGui::GetIO().AddMouseButtonEvent(0,false);frame();
        // Deliberately no following draw: the production foreground action
        // returns before drawMaps on this release frame. The caller suppresses
        // the next frame through the real presentation gate before GL is used.
    }
    void reviewBounds(const char *confirm,const char *cancel,float expectedFont)
    {
        const auto action=visibleItem(confirm),closing=visibleItem(cancel);
        reviewVisible(action);reviewVisible(closing);
        auto *popup=ImGui::FindWindowByName(closing.window.c_str());
        require(popup&&(popup->Flags&ImGuiWindowFlags_Popup),"Review footer is outside actual popup");
        const auto *vp=popup->Viewport;
        const ImRect work(vp->WorkPos,ImVec2(vp->WorkPos.x+vp->WorkSize.x,vp->WorkPos.y+vp->WorkSize.y));
        if(!work.Contains(popup->Rect())||std::abs(popup->FontRefSize-expectedFont)>.01f)
            controlFailure("Review popup bounds or actual scaled font differ",cancel,closing.id);
        require(action.window==closing.window,"Review footer controls have different owners");
    }
    void reviewKey(ImGuiKey key,bool shift=false)
    {
        auto &io=ImGui::GetIO();io.AddMousePosEvent(-FLT_MAX,-FLT_MAX);
        if(shift)io.AddKeyEvent(ImGuiMod_Shift,true);
        io.AddKeyEvent(key,true);frame();io.AddKeyEvent(key,false);
        if(shift)io.AddKeyEvent(ImGuiMod_Shift,false);frame(2);
    }
    std::string reviewInputState(const char *label) const
    {
        std::ostringstream out;out<<"frame="<<context_->FrameCount<<" popup_depth="<<context_->OpenPopupStack.Size
            <<" nav_id="<<context_->NavId<<" nav_cursor="<<context_->NavCursorVisible
            <<" nav_window="<<(context_->NavWindow?context_->NavWindow->Name:"<none>")
            <<" nav_focus_scope="<<context_->NavFocusScopeId<<" nav_layer="<<context_->NavLayer
            <<" active_id="<<context_->ActiveId<<" hovered_id="<<context_->HoveredId
            <<" activate_id="<<context_->NavActivateId<<" activate_pressed="<<context_->NavActivatePressedId
            <<" space_down="<<ImGui::IsKeyDown(ImGuiKey_Space)<<" tab_down="<<ImGui::IsKeyDown(ImGuiKey_Tab);
        const auto items=activeControls(label);out<<" target_count="<<items.size();
        for(const auto &item:items)out<<" target_id="<<item.id<<" target_owner="<<item.window;
        const auto focused=trace_->items().find(context_->NavId);
        if(focused!=trace_->items().end())out<<" focused_label='"<<focused->second.label<<"' focused_owner="<<focused->second.window;
        return out.str();
    }
    std::string reviewPublicText(const char *cancel) const
    {
        const auto closing=visibleItem(cancel);const auto *popup=ImGui::FindWindowByName(closing.window.c_str());
        require(popup&&(popup->Flags&ImGuiWindowFlags_Popup),"Public-review text has no observed popup owner");
        const std::string first=gm::localization::trUi("Visible to players"),last=gm::localization::trUi("Hidden from players");
        const auto begin=rendered.find(first),end=rendered.find(last);
        if(begin==std::string::npos||end==std::string::npos||begin>=end||
            rendered.find(first,begin+first.size())!=std::string::npos||rendered.find(last,end+last.size())!=std::string::npos)
            throw std::runtime_error("Expected exactly one ordered production public-review text section\n"+rendered);
        // Both markers are emitted by drawContent inside the review popup.
        // The underlying live Handout Reveals panel legitimately logs new text.
        return rendered.substr(begin+first.size(),end-begin-first.size());
    }
    std::string reviewBodyState() const
    {
        std::ostringstream out;out<<" logging="<<context_->LogEnabled<<" log_unclip="<<context_->ItemUnclipByLog;
        for(const auto *body:context_->Windows) {
            if(std::string(body->Name).find("ReviewBody")==std::string::npos)continue;
            out<<" body='"<<body->Name<<"' active="<<body->Active<<" hidden="<<body->Hidden<<" skip="<<body->SkipItems
                <<" skip_refresh="<<body->SkipRefresh<<" appearing="<<body->Appearing<<" child_flags="<<body->ChildFlags
                <<" nav_scope="<<body->NavRootFocusScopeId<<" clip=("<<body->InnerClipRect.Min.x<<","<<body->InnerClipRect.Min.y
                <<")-("<<body->InnerClipRect.Max.x<<","<<body->InnerClipRect.Max.y<<") scroll="<<body->Scroll.x<<","<<body->Scroll.y
                <<" scroll_max="<<body->ScrollMax.x<<","<<body->ScrollMax.y<<" content="<<body->ContentSize.x<<","<<body->ContentSize.y;
        }
        return out.str();
    }
    void reviewTabTo(const char *label,bool reverse=false)
    {
        ImGuiID id=0;std::string window;std::ostringstream history;
        for(int attempt=0;attempt<30;++attempt) {
            history<<"\nqueued_tabs="<<attempt<<" reverse="<<reverse<<" "<<reviewInputState(label);
            const auto items=activeControls(label);
            if(items.size()>1)controlFailure("Ambiguous keyboard review target",label);
            if(items.size()==1){const auto &item=items.front();
                require(!id||(id==item.id&&window==item.window),"Keyboard review target changed owner or identity");
                id=item.id;window=item.window;
                // Popup initialization may choose Cancel as NavId while the
                // keyboard cursor is hidden. Space cannot activate that state.
                // A real Tab must establish keyboard focus in the exact owner.
                if(context_->NavId==id&&context_->NavCursorVisible&&context_->NavWindow&&
                    window==context_->NavWindow->Name){reviewVisible(item);return;}}
            // An offscreen body action may become visible through real Tab's
            // own scrolling. No wheel fallback or scroll assignment is used.
            reviewKey(ImGuiKey_Tab,reverse);
        }
        history<<"\nqueued_tabs=30 reverse="<<reverse<<" "<<reviewInputState(label);
        auto *popup=context_->OpenPopupStack.empty()?nullptr:context_->OpenPopupStack.back().Window;
        for(const auto &[observedId,item]:trace_->items()) {
            auto *owner=ImGui::FindWindowByName(item.window.c_str()),*ancestor=owner;
            while(ancestor&&ancestor!=popup)ancestor=ancestor->ParentWindow;
            if(!popup||!ancestor||!item.hasRectangle||item.label.empty())continue;
            history<<"\nobserved_id="<<observedId<<" label='"<<item.label<<"' owner="<<item.window
                <<" child_flags="<<owner->ChildFlags<<" root_nav="<<owner->RootWindowForNav->Name
                <<" rect=("<<item.minimum.x<<","<<item.minimum.y<<")-("<<item.maximum.x<<","<<item.maximum.y<<")";
        }
        std::cerr<<"Observed real Tab sequence:"<<history.str()<<'\n';
        controlFailure("Real Tab navigation did not reach review control",label,id);
    }
