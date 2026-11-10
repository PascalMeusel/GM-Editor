#include "Views/Editor/EditorWorldInspectorSupport.hpp"
#include <cmath>

void EditorWindowView::renderWorldShopInspector(const EntityId &shopId)
{
    if(!worldRecoveryReady()){renderWorldRecoveryNotice();return;}
    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const auto text = [german](const char *english, const char *deutsch) { return german ? deutsch : english; };
    const auto prepareShopConfirmation = []() {
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        setResponsiveWindowSize(ImVec2(620.0f, 340.0f), ImVec2(400.0f, 240.0f));
    };
    if (_worldDraftEntityId != shopId || _worldDraftReloadRequested)
        reloadWorldInspectorDraft(shopId);
    if(_worldDraftEntityId!=shopId)return;
    if (!WorldInspector::findShop(_controller.currentCampaign(), shopId))
    {
        WorkspaceUi::emptyState(text("Shop unavailable", "Händler nicht verfügbar"),
                                text("Select another World entry.", "Wähle einen anderen Welteintrag."));
        return;
    }

    ImGui::SeparatorText(text("Shop details", "Händlerdetails"));
    {
    DraftEditSignal::Scope shopEdits([this]{_worldFormEdited=true;});
    FormUi::InputText(text("Name", "Name"), &_worldShopDraft.name);
    FormUi::InputText(text("Merchant", "Händlerperson"), &_worldShopDraft.merchantName);
    FormUi::InputTextMultiline(text("Description", "Beschreibung"), &_worldShopDraft.description, ImVec2(-1.0f, 80.0f), ImGuiInputTextFlags_WordWrap);
    FormUi::InputTextMultiline(text("Faction and reputation notes", "Fraktions- und Rufnotizen"),
                               &_worldShopDraft.factionReputationNotes, ImVec2(-1.0f, 64.0f), ImGuiInputTextFlags_WordWrap);
    if (drawUiButton(text("Save details", "Details speichern"), UiButtonRole::Primary, ImVec2(150.0f, 0.0f)))
    {
        reportWorldInspectorResult(_controller.saveWorldDraft({WorldInspectorDrafts::Part::ShopDetails,shopId,{}}, "edit World shop"),
                                   "Shop details saved.", "Händlerdetails gespeichert.");
    }
    }
    ImGui::SameLine();
    if (drawUiButton(text("Delete shop", "Händler löschen"), UiButtonRole::Destructive, ImVec2(150.0f, 0.0f)))
        ImGui::OpenPopup(localizedUiLabel("Delete World Shop").c_str());
    prepareShopConfirmation();
    if (ImGui::BeginPopupModal(localizedUiLabel("Delete World Shop").c_str(), nullptr,
                               ImGuiWindowFlags_NoSavedSettings))
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::TextWrapped(
            localizedUiLabel("%s").c_str(),
            text(
                "Delete this shop and its stock list? Item definitions and Party Inventory remain intact.",
                "Diesen Händler und seine Bestandsliste löschen? Gegenstandsdefinitionen und Gruppeninventar bleiben erhalten."));
        if (drawModalActionButtons(text("Delete shop", "Händler löschen"), UiButtonRole::Destructive,
                                   text("Cancel", "Abbrechen"), 150.0f))
        {
            const WorldInspector::MutationResult result = _controller.applyWorldInspectorMutation(
                [shopId](Campaign &campaign) { return WorldInspector::deleteShop(campaign, shopId); },
                "delete World shop", {}, WorldInspectorDrafts::Scope::entity(shopId));
            if (result.applied())
            {
                _workspaceSelectionId.clear();
                _worldDraftEntityId.clear();
            }
            _worldInspectorStatus = result.applied() ? text("Shop deleted.", "Händler gelöscht.") : result.message;
        }
        ImGui::EndPopup();
    }

    ImGui::SeparatorText(text("Stock and transactions", "Bestand und Transaktionen"));
    const std::vector<ItemModel> items = _controller.availableWorldItems();
    if (!items.empty())
    {
        _worldAddStockItemIndex = std::clamp(_worldAddStockItemIndex, 0, static_cast<int>(items.size()) - 1);
        const std::string selectedName = items[static_cast<std::size_t>(_worldAddStockItemIndex)].name;
        const float stockPopupWidth = std::max(240.0f, ImGui::GetContentRegionAvail().x);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SetNextWindowSizeConstraints(ImVec2(stockPopupWidth, 0.0f), ImVec2(stockPopupWidth, 360.0f));
        if (FormUi::BeginCombo(text("Add an item", "Gegenstand hinzufügen"),
                              selectedName.empty() ? text("Unnamed item", "Unbenannter Gegenstand") : selectedName.c_str(),
                              ImGuiComboFlags_HeightLarge))
        {
            ImGui::TextUnformatted(text("Search items", "Gegenstände suchen"));
            if (ImGui::IsWindowAppearing()) {
                _worldStockFilter.Clear();
                ImGui::SetKeyboardFocusHere();
            }
            _worldStockFilter.Draw("##WorldStockSearch", stockPopupWidth - ImGui::GetStyle().WindowPadding.x * 2.0f);
            ImGui::Separator();
            int matches = 0;
            for (int index = 0; index < static_cast<int>(items.size()); ++index) {
                const auto &candidate = items[static_cast<std::size_t>(index)];
                if (!_worldStockFilter.PassFilter(candidate.name.c_str())) continue;
                ++matches;
                ImGui::PushID(candidate.metadata.id.c_str());
                if (ImGui::Selectable(candidate.name.empty() ? text("Unnamed item", "Unbenannter Gegenstand") : candidate.name.c_str(),
                                      _worldAddStockItemIndex == index))
                    _worldAddStockItemIndex = index;
                ImGui::PopID();
            }
            if (matches == 0) ImGui::TextDisabled("%s", text("No matching items.", "Keine passenden Gegenstände."));
            ImGui::EndCombo();
        }
        if (drawSmallUiButton(text("Add stock", "Bestand hinzufügen"), UiButtonRole::Secondary))
        {
            const EntityId itemId = items[static_cast<std::size_t>(_worldAddStockItemIndex)].metadata.id;
            reportWorldInspectorResult(
                _controller.applyWorldInspectorMutation(
                    [shopId, itemId](Campaign &campaign) { return WorldInspector::addStock(campaign, shopId, itemId); },
                    "add World shop stock"),
                "Stock item added.", "Bestand hinzugefügt.");
            const ShopModel *savedShop = WorldInspector::findShop(_controller.currentCampaign(), shopId);
            if (savedShop) for (int index = 0; index < static_cast<int>(savedShop->stock.size()); ++index)
                if (savedShop->stock[static_cast<std::size_t>(index)].itemId == itemId) {
                    _worldSelectedStockIndex = index;
                    _worldTransferQuantity = 1;
                    break;
                }
        }
    }
    else
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), text("Create a campaign item before adding stock.",
                                                                 "Erstelle zuerst einen Kampagnengegenstand."));

    enum class DeferredStockAction { None, Save, Remove };
    DeferredStockAction deferredStockAction = DeferredStockAction::None;
    EntityId deferredItemId;

    if (_worldShopDraft.stock.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(),
                            text("No stock entries yet.", "Noch keine Bestandseinträge."));
    }
    else if (ImGui::BeginTable("WorldShopStock", ImGui::GetContentRegionAvail().x < 640.0f ? 1 : 2,
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings))
    {
        const bool stacked = ImGui::TableGetColumnCount() == 1;
        ImGui::TableSetupColumn(text("Stock", "Bestand"), ImGuiTableColumnFlags_WidthFixed, 210.0f);
        if (!stacked) ImGui::TableSetupColumn(text("Selected item", "Ausgewählter Gegenstand"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        for (int index = 0; index < static_cast<int>(_worldShopDraft.stock.size()); ++index)
        {
            const ShopStockEntryModel &entry = _worldShopDraft.stock[static_cast<std::size_t>(index)];
            const ItemModel *item = nullptr;
            for (const ItemModel &candidate : items)
                if (candidate.metadata.id == entry.itemId)
                {
                    item = &candidate;
                    break;
                }
            const std::string label =
                (item && !item->name.empty() ? item->name : text("Missing item", "Fehlender Gegenstand")) + "  ×" +
                std::to_string(entry.quantity);
            ImGui::PushID(index);
            if (ImGui::Selectable(label.c_str(), _worldSelectedStockIndex == index))
            {
                _worldSelectedStockIndex = index;
                _worldTransferQuantity = 1;
            }
            ImGui::PopID();
        }
        if (stacked) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Separator();
        } else ImGui::TableSetColumnIndex(1);
        if (_worldSelectedStockIndex >= 0 && _worldSelectedStockIndex < static_cast<int>(_worldShopDraft.stock.size()))
        {
            ShopStockEntryModel &entry = _worldShopDraft.stock[static_cast<std::size_t>(_worldSelectedStockIndex)];
            const ItemModel *item = nullptr;
            for (const ItemModel &candidate : items)
                if (candidate.metadata.id == entry.itemId)
                {
                    item = &candidate;
                    break;
                }
            ImGui::Text(localizedUiLabel("%s").c_str(),
                        item && !item->name.empty()
                            ? item->name.c_str()
                            : text("Missing item definition", "Fehlende Gegenstandsdefinition"));
            {
            DraftEditSignal::Scope stockEdits([this]{_worldFormEdited=true;});
            FormUi::InputInt(text("Quantity", "Menge"), &entry.quantity);
            entry.quantity = std::max(0, entry.quantity);
            FormUi::Checkbox(text("Restocks", "Wird aufgefüllt"), &entry.restocks);
            FormUi::InputText(text("Availability", "Verfügbarkeit"), &entry.availability);
            if(ImGui::TreeNode(text("Pricing and negotiation", "Preise und Verhandlung"))) {
            FormUi::InputDouble(text("Buy multiplier", "Kaufmultiplikator"), &entry.priceMultiplier, 0.05, 0.25,
                                "%.2f");
            FormUi::InputDouble(text("Sell multiplier", "Verkaufsmultiplikator"), &entry.sellPriceMultiplier, 0.05,
                                0.25, "%.2f");
            FormUi::InputDouble(text("Faction modifier", "Fraktionsmodifikator"), &entry.factionModifier, 0.05, 0.25,
                                "%.2f");
            FormUi::InputDouble(text("Reputation modifier", "Rufmodifikator"), &entry.reputationModifier, 0.05, 0.25,
                                "%.2f");
            FormUi::InputTextMultiline(text("Haggle notes", "Feilschnotizen"), &entry.haggleNotes,
                                       ImVec2(-1.0f, 64.0f), ImGuiInputTextFlags_WordWrap);
            ImGui::TreePop();
            }
            }
            if (drawUiButton(text("Save stock", "Bestand speichern"), UiButtonRole::Primary, ImVec2(140.0f, 0.0f)))
            {
                deferredStockAction = DeferredStockAction::Save;
                deferredItemId = entry.itemId;
            }
            ImGui::SameLine();
            if (drawUiButton(text("Remove", "Entfernen"), UiButtonRole::Destructive, ImVec2(110.0f, 0.0f)))
                ImGui::OpenPopup(localizedUiLabel("Remove World Stock").c_str());
            prepareShopConfirmation();
            if (ImGui::BeginPopupModal(localizedUiLabel("Remove World Stock").c_str(), nullptr,
                                       ImGuiWindowFlags_NoSavedSettings))
            {
                if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                    ImGui::CloseCurrentPopup();
                ImGui::TextWrapped(localizedUiLabel("%s").c_str(),
                                   text("Remove this item from the shop stock list?",
                                        "Diesen Gegenstand aus der Bestandsliste entfernen?"));
                if (drawModalActionButtons(text("Remove", "Entfernen"), UiButtonRole::Destructive,
                                           text("Cancel", "Abbrechen"), 130.0f))
                {
                    deferredStockAction = DeferredStockAction::Remove;
                    deferredItemId = entry.itemId;
                }
                ImGui::EndPopup();
            }

            drawWorldShopTransactions(shopId, entry.itemId, item && !item->name.empty() ? item->name : text("Missing item", "Fehlender Gegenstand"));
        }
        ImGui::EndTable();
    }
    switch (deferredStockAction)
    {
    case DeferredStockAction::Save:
        reportWorldInspectorResult(_controller.saveWorldDraft({WorldInspectorDrafts::Part::Stock,shopId,deferredItemId},
            "edit World shop stock"), "Stock saved.", "Bestand gespeichert.");
        break;
    case DeferredStockAction::Remove:
        reportWorldInspectorResult(_controller.applyWorldInspectorMutation(
            [shopId,deferredItemId](Campaign &campaign) {
                const auto *shop=WorldInspector::findShop(campaign,shopId);
                const auto index=shop?WorldInspectorDrafts::uniqueStockIndex(*shop,deferredItemId):WorldInspectorDrafts::MissingStock;
                if(index==WorldInspectorDrafts::MissingStock)return WorldInspector::MutationResult{
                    WorldInspector::MutationStatus::Conflict,"The selected stock entry is missing or ambiguous.",shopId};
                return WorldInspector::removeStock(campaign,shopId,index);
            }, "remove World shop stock", {}, {WorldInspectorDrafts::Part::Stock,shopId,deferredItemId}),
            "Stock entry removed.", "Bestandseintrag entfernt.");
        break;
    case DeferredStockAction::None:
        break;
    }
    if (!_worldInspectorStatus.empty())
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _worldInspectorStatus.c_str());
}
