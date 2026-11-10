#include "Views/Editor/EditorWorldInspectorSupport.hpp"
#include <cmath>
#include <limits>

void EditorWindowView::drawWorldShopTransactions(EntityId shopId,EntityId itemId,std::string itemName)
{
    const bool de=gm::localization::LocalizationService::instance().locale()==gm::localization::Locale::German;
    const auto text=[de](const char *en,const char *german){return de?german:en;};
    ImGui::SeparatorText(text("Record transaction","Transaktion erfassen"));
    ImGui::TextWrapped("%s",text("Moves items between shop stock and Party Inventory. Record payment separately; coin totals stay unchanged.",
        "Verschiebt Gegenstände zwischen Händler und Gruppeninventar. Zahlungen separat erfassen; Münzbestände bleiben unverändert."));
    FormUi::InputInt(text("Transfer quantity","Transfermenge"),&_worldTransferQuantity);
    _worldTransferQuantity=std::max(1,_worldTransferQuantity);
    const auto *saved=WorldInspector::findShop(_controller.currentCampaign(),shopId);
    const auto index=saved?WorldInspectorDrafts::uniqueStockIndex(*saved,itemId):WorldInspectorDrafts::MissingStock;
    const int shopBefore=index==WorldInspectorDrafts::MissingStock?0:saved->stock[index].quantity;
    const int partyBefore=WorldInspector::partyQuantity(_controller.currentCampaign(),itemId);
    ImGui::Text(text("Shop: %d | Party Inventory: %d","Händler: %d | Gruppeninventar: %d"),shopBefore,partyBefore);
    const auto items=_controller.availableWorldItems();
    const auto item=std::find_if(items.begin(),items.end(),[&](const auto &v){return v.metadata.id==itemId;});
    if(item!=items.end() && index!=WorldInspectorDrafts::MissingStock) {
        const auto &row=saved->stock[index];
        const double buy=item->baseValue*row.priceMultiplier*row.factionModifier*row.reputationModifier;
        const double sell=item->baseValue*row.sellPriceMultiplier*row.factionModifier*row.reputationModifier;
        if(std::isfinite(buy*_worldTransferQuantity)&&std::isfinite(sell*_worldTransferQuantity)&&buy>=0&&sell>=0)
            ImGui::TextWrapped(text("Price guide: buy %.0f cp each (%.0f cp total); sell %.0f cp each (%.0f cp total).",
                "Preisrichtwert: Kauf %.0f KM je Stück (gesamt %.0f KM); Verkauf %.0f KM je Stück (gesamt %.0f KM)."),
                buy,buy*_worldTransferQuantity,sell,sell*_worldTransferQuantity);
    }
    const auto open=[&](bool purchase) {
        const auto result=_controller.saveWorldDraft({WorldInspectorDrafts::Part::Stock,shopId,itemId},"save stock before transaction review");
        if(!result.applied()){reportWorldInspectorResult(result,"Stock saved.","Bestand gespeichert.");return;}
        const auto *fresh=WorldInspector::findShop(_controller.currentCampaign(),shopId);
        const auto row=fresh?WorldInspectorDrafts::uniqueStockIndex(*fresh,itemId):WorldInspectorDrafts::MissingStock;
        if(row==WorldInspectorDrafts::MissingStock){_worldInspectorStatus=text("Stock is missing or ambiguous.","Bestand fehlt oder ist nicht eindeutig.");return;}
        _worldStockTransfer={shopId,itemId,_worldTransferQuantity,fresh->stock[row].quantity,
            WorldInspector::partyQuantity(_controller.currentCampaign(),itemId),purchase};
        _worldStockTransferReviewed=true;
        ImGui::OpenPopup(localizedUiLabel(purchase?"Confirm World Purchase":"Confirm World Sale").c_str());
    };
    ImGui::BeginDisabled(item==items.end() || index==WorldInspectorDrafts::MissingStock);
    if(drawUiButton(text("Party buys","Gruppe kauft"),UiButtonRole::Secondary,ImVec2(140,0)))open(true);
    if(ImGui::GetContentRegionAvail().x>=290)ImGui::SameLine();
    if(drawUiButton(text("Party sells","Gruppe verkauft"),UiButtonRole::Secondary,ImVec2(140,0)))open(false);
    ImGui::EndDisabled();
    for(bool purchase:{true,false}) {
        const auto *viewport=ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(),ImGuiCond_Appearing,ImVec2(.5f,.5f));
        setResponsiveWindowSize(ImVec2(620,340),ImVec2(400,240));
        if(!ImGui::BeginPopupModal(localizedUiLabel(purchase?"Confirm World Purchase":"Confirm World Sale").c_str(),nullptr,ImGuiWindowFlags_NoSavedSettings))continue;
        const auto review=_worldStockTransfer;
        if(!_worldStockTransferReviewed || review.shopId!=shopId || review.itemId!=itemId || review.purchase!=purchase) {
            ImGui::CloseCurrentPopup();ImGui::EndPopup();continue;
        }
        ImGui::TextWrapped(text("%s: %d × %s. Shop: %d → %lld. Party Inventory: %d → %lld. Coin totals stay unchanged.",
            "%s: %d × %s. Händler: %d → %lld. Gruppeninventar: %d → %lld. Münzbestände bleiben unverändert."),
            purchase?text("Purchase","Kauf"):text("Sale","Verkauf"),review.quantity,itemName.c_str(),review.shopBefore,
            static_cast<long long>(review.shopBefore)+(purchase?-static_cast<long long>(review.quantity):review.quantity),review.partyBefore,
            static_cast<long long>(review.partyBefore)+(purchase?static_cast<long long>(review.quantity):-static_cast<long long>(review.quantity)));
        const int destination=purchase?review.partyBefore:review.shopBefore;
        const bool possible=review.quantity<=(purchase?review.shopBefore:review.partyBefore) && destination>=0 &&
            review.quantity<=std::numeric_limits<int>::max()-destination;
        if(!possible)ImGui::TextWrapped("%s",text("This quantity is unavailable or exceeds the destination limit. Cancel and change the quantity.",
            "Diese Menge ist nicht verfügbar oder überschreitet das Ziellimit. Abbrechen und Menge ändern."));
        ImGui::BeginDisabled(!possible);
        if(drawUiButton(purchase?text("Record purchase","Kauf erfassen"):text("Record sale","Verkauf erfassen"),UiButtonRole::Primary)) {
            const auto result=_controller.applyWorldInspectorMutation([review](Campaign &c){return WorldInspectorDrafts::applyTransfer(c,review);},
                purchase?"record World shop purchase":"record World shop sale");
            reportWorldInspectorResult(result,"Transaction recorded.","Transaktion erfasst.");
            if(result.applied()){_worldStockTransferReviewed=false;ImGui::CloseCurrentPopup();}
        }
        ImGui::EndDisabled();
        TaskUi::sameLineIfFits();
        if(drawUiButton(text("Cancel","Abbrechen"),UiButtonRole::Secondary)||ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            _worldStockTransferReviewed=false;ImGui::CloseCurrentPopup();
        }
        if(!_worldInspectorStatus.empty())ImGui::TextWrapped("%s",_worldInspectorStatus.c_str());
        ImGui::EndPopup();
    }
}
