#pragma once
#include "Views/Editor/ExchangePanel.hpp"
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "Views/Editor/Presentation/PresentationReviewUi.hpp"
#include <algorithm>

namespace ExchangeUi
{
inline const char *tr(const char *en,const char *de){return uiTextForLocale(en,de);}
inline bool button(const char *en,const char *de,bool enabled=true)
{return drawUiButton(tr(en,de),enabled,tr("Wait for the current operation or cancel it.","Warte auf den laufenden Vorgang oder brich ihn ab."));}
inline void text(const std::string &value)
{
    ImGui::PushTextWrapPos(0);
    ImGui::TextUnformatted(value.data(),value.data()+value.size());
    ImGui::PopTextWrapPos();
}
inline std::string singleLine(std::string value)
{for(auto &ch:value)if(ch=='\n'||ch=='\r'||ch=='\t')ch=' ';return value;}
inline void rawText(const char *id,const std::string &value)
{
    // Unwrapped, clipped TextUnformatted avoids laying out a multi-MiB note
    // every frame. Native scrollbars retain the exact visible source text.
    if(ImGui::BeginChild(id,ImVec2(0,ImGui::GetTextLineHeightWithSpacing()*8),ImGuiChildFlags_Borders,ImGuiWindowFlags_HorizontalScrollbar))
        ImGui::TextUnformatted(value.data(),value.data()+value.size());
    ImGui::EndChild();
}
inline void label(const char *en,const char *de)
{ImGui::TextWrapped("%s",tr(en,de));ImGui::SetNextItemWidth(-1);}
inline void diagnostic(const char *id,const std::string &message)
{
    ImGui::PushID(id);
    if(ImGui::TreeNode(tr("Technical details","Technische Details"))){text(message);ImGui::TreePop();}
    ImGui::PopID();
}
inline const char *state(CampaignMarkdownExchange::UpdateState state)
{
    using S=CampaignMarkdownExchange::UpdateState;
    switch(state){
    case S::Unchanged:return tr("Unchanged","Unverändert");
    case S::AlreadyApplied:return tr("Already applied","Bereits übernommen");
    case S::Eligible:return tr("Ready to update","Kann aktualisiert werden");
    case S::Conflict:return tr("Conflict","Konflikt");
    case S::Unavailable:return tr("Unavailable","Nicht verfügbar");
    }
    return "";
}
}
