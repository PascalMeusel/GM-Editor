#pragma once

#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Models/Inventory/InventoryModels.hpp"
#include <map>
#include <filesystem>
#include <functional>
#include <optional>
#include "Views/Editor/Widgets/Inventory/ItemCreator/ItemCreatorView.hpp"

class ItemCreatorController : public IWidgetController
{
public:
    explicit ItemCreatorController(Campaign &currentCampaign);

    void update(std::shared_ptr<GLFWwindow> window) override;
    void configure();
    void resumeDraft();
    void requestCatalogBrowser();
    void markDraftEdited() { _draftEdited = true; if(_recoveryChanged)_recoveryChanged(); }
    bool draftRecoveryReady() { return !_recoveryReady || _recoveryReady(); }
    bool hasUnsavedDraft() const;
    bool flushDrafts();
    bool saveItem(bool allowArchiveChange = false);
    bool clearUnfinishedPower();
    void copyItemAsDraft(const ItemModel &item);
    void setAssignHandler(std::function<void(EntityId)> handler) { _assignHandler = std::move(handler); }
    void setPlayerReviewHandler(std::function<void(EntityId)> handler) { _playerReviewHandler = std::move(handler); }
    bool selectItemById(const EntityId &itemId);
    bool attachImage(const std::filesystem::path &path);
    void browseImage();
    bool prepareImageHandout();

    ItemModel &getModel() { return _model; }
    std::vector<std::string> getAvailableCategories() const;

private:
    friend struct ContextPreviewUiProbe;
    std::function<void(EntityId)> _playerReviewHandler;
    friend class ItemCreatorView;
    friend class EditorDraftRecoveryController;
    friend struct EditorDraftRecoveryProbe;

    struct CatalogEntry
    {
        ItemModel item;
        std::string source;
        bool campaignCustom = false;
        int customItemIndex = -1;
    };

    struct ItemReferenceSummary
    {
        int playerEntryCount = 0;
        int npcEntryCount = 0;
        int totalQuantity = 0;
        int storyLinkCount = 0;
        std::vector<std::string> labels;

        bool hasReferences() const
        {
            return playerEntryCount > 0 || npcEntryCount > 0 || storyLinkCount > 0;
        }
    };

    struct RetainedDraft
    {
        ItemModel item;
        MonsterActionModel power;
        EntityId sourceId;
        bool dirty = false;
        bool powerDirty = false;
        std::optional<ItemModel> base;
    };
    std::map<EntityId, RetainedDraft> _retainedDrafts;
    bool _draftEdited = false;
    bool _powerDraftEdited = false;
    std::optional<ItemModel> _draftBase;
    std::function<bool()> _recoveryReady, _recoveryCheckpoint;
    std::function<void()> _recoveryChanged, _recoveryCommitted;
    std::function<void(EntityId)> _assignHandler;
    EntityId _selectedItemId, _draftCampaignId;
    void retainCurrentDraft();
    bool restoreDraft(const EntityId &id);
    bool refreshSelectedItemIndex();
    ItemModel _model;
    MonsterActionModel _itemPowerDraft;
    std::unique_ptr<ItemCreatorView> _view;
    Campaign &_currentCampaign;
    int _selectedItemIndex = -1;
    int _selectedCatalogIndex = 0;
    int _selectedCatalogCategoryIndex = 0;
    int _selectedCatalogSourceIndex = 0;
    std::string _catalogSearch;
    std::string _statusMessage;
    std::string _errorMessage;

    void createNewDraft();
    void selectItem(int itemIndex);
    void selectCatalogCustomItem(const CatalogEntry &entry);
    bool persistCampaignChanges();
    void archiveSelectedItem(bool archived);
    void deleteSelectedItem();
    void addPowerToDraftItem();
    void removePowerFromDraftItem(int actionIndex);
    void removeDeletedItemReferences(Campaign &campaign, const EntityId &itemId);
    ItemReferenceSummary getReferenceSummary(const EntityId &itemId) const;
    std::vector<CatalogEntry> getCatalogEntries() const;
    std::vector<std::string> getCatalogCategories(const std::vector<CatalogEntry> &entries) const;
    std::vector<CatalogEntry> getVisibleCatalogEntries(const std::vector<CatalogEntry> &entries, const std::vector<std::string> &categories) const;
    void onShow() override;
};
