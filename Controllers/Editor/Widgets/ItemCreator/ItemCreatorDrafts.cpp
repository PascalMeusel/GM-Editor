#include "ItemCreatorController.hpp"
#include "ItemCreatorControllerSupport.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/CampaignEntityDraft.hpp"
#include "Models/Project/NativeAssetPicker.hpp"
#include "Models/Project/ProjectAssetImportService.hpp"
#include <algorithm>
#include <cctype>

namespace
{
std::string uiText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? german : english;
}

}

void ItemCreatorController::onShow()
{
    _view->requestEditorTab();
    _view->show();
    _view->requestDockOnNextUpdate();
}


void ItemCreatorController::resumeDraft()
{
    if (_draftCampaignId != _currentCampaign.metadata.id) configure();
}

bool ItemCreatorController::hasUnsavedDraft() const
{
    if (_draftEdited || _powerDraftEdited) return true;
    return std::any_of(_retainedDrafts.begin(), _retainedDrafts.end(),
        [](const auto &entry) { return entry.second.dirty || entry.second.powerDirty; });
}

bool ItemCreatorController::clearUnfinishedPower()
{
    if(!draftRecoveryReady())return false;
    const auto previous=_itemPowerDraft;const bool dirty=_powerDraftEdited;
    _itemPowerDraft=MonsterActionModel{};_itemPowerDraft.actionKind="Item Power";_powerDraftEdited=false;
    if(_recoveryChanged)_recoveryChanged();
    if(_recoveryCheckpoint&&!_recoveryCheckpoint()) {
        _itemPowerDraft=previous;_powerDraftEdited=dirty;
        _errorMessage=uiText("The unfinished power was retained because its recovery checkpoint could not be updated.",
            "Die unfertige Kraft bleibt erhalten, weil ihre Sicherung nicht aktualisiert werden konnte.");return false;
    }
    _errorMessage.clear();return true;
}

bool ItemCreatorController::flushDrafts()
{
    if (!hasUnsavedDraft()) return true;
    const EntityId original = _model.metadata.id;
    if ((_draftEdited || _powerDraftEdited) && !saveItem()) return false;
    retainCurrentDraft();
    std::vector<EntityId> pending;
    for (const auto &entry : _retainedDrafts)
        if (entry.second.dirty || entry.second.powerDirty) pending.push_back(entry.first);
    for (const auto &id : pending)
        if (!restoreDraft(id) || !saveItem()) return false;
    restoreDraft(original);
    _retainedDrafts.erase(original);
    return true;
}

void ItemCreatorController::retainCurrentDraft()
{
    if (!_model.metadata.id.empty())
        _retainedDrafts[_model.metadata.id] = RetainedDraft{_model, _itemPowerDraft, _selectedItemId, _draftEdited, _powerDraftEdited, _draftBase};
}

bool ItemCreatorController::restoreDraft(const EntityId &id)
{
    const auto found = _retainedDrafts.find(id);
    if (found == _retainedDrafts.end()) return false;
    const auto draft = found->second;
    _model = draft.item;
    _draftEdited = draft.dirty;
    _itemPowerDraft = draft.power;
    _powerDraftEdited = draft.powerDirty;
    _selectedItemId = draft.sourceId;
    _draftBase = draft.base;
    _statusMessage.clear();
    _errorMessage.clear();
    refreshSelectedItemIndex();
    return true;
}

bool ItemCreatorController::refreshSelectedItemIndex()
{
    _selectedItemIndex = -1;
    if (_selectedItemId.empty()) return true;
    for (std::size_t i = 0; i < _currentCampaign.customItems.size(); ++i)
        if (_currentCampaign.customItems[i].metadata.id == _selectedItemId)
        {
            _selectedItemIndex = static_cast<int>(i);
            return true;
        }
    _errorMessage = uiText("This item is unavailable. Your draft is retained; copy it to create a new item.",
        "Dieser Gegenstand ist nicht mehr vorhanden. Dein Entwurf bleibt erhalten; kopiere ihn als neuen Gegenstand.");
    return false;
}

void ItemCreatorController::copyItemAsDraft(const ItemModel &item)
{
    if (!draftRecoveryReady()) return;
    const bool pendingPower=&item==&_model&&_powerDraftEdited;
    auto powerCopy=_itemPowerDraft;
    ItemModel copy = item;
    retainCurrentDraft();
    const EntityMetadata identity{"item"};
    copy.metadata.id = identity.id;
    copy.metadata.createdAtUtc = identity.createdAtUtc;
    copy.metadata.updatedAtUtc = identity.updatedAtUtc;
    setArchivedTag(copy, false);
    copy.name += uiText(" (copy)", " (Kopie)");
    for (auto &power : copy.powers)
    {
        power.sourceEntityId = copy.metadata.id;
        power.sourceLabel = copy.name;
    }
    _model = std::move(copy);
    _draftEdited = true;
    _selectedItemId.clear();
    _draftBase.reset();
    _selectedItemIndex = -1;
    _powerDraftEdited = pendingPower;
    _itemPowerDraft = pendingPower?std::move(powerCopy):MonsterActionModel{};
    if(pendingPower) {_itemPowerDraft.sourceEntityId=_model.metadata.id;_itemPowerDraft.sourceLabel=_model.name;}
    else _itemPowerDraft.actionKind = "Item Power";
    _statusMessage.clear();
    _errorMessage.clear();
    if(_recoveryChanged)_recoveryChanged();
}
