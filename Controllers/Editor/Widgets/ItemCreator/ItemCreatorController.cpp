#include "ItemCreatorController.hpp"
#include "ItemCreatorControllerSupport.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/CampaignEntityDraft.hpp"
#include "Models/Project/EditorDraftRecovery.hpp"
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

bool ItemCreatorController::attachImage(const std::filesystem::path &path)
{
    if(!draftRecoveryReady())return false;
    ProjectAssetImport::AssetImportRequest request;
    request.kind=ProjectAssetImport::AssetKind::Image;request.sourcePath=path;
    request.assetId=_model.metadata.id+"_image";request.displayName=_model.name;
    const auto result=ProjectAssetImport::importAsset(_currentCampaign,request);
    if(!result.success){_errorMessage=result.errorMessage;return false;}
    _model.metadata.customProperties["item.image"]=result.projectPath;
    markDraftEdited();_errorMessage.clear();return true;
}

void ItemCreatorController::browseImage()
{
    if(const auto path=gm::assets::chooseImage(_errorMessage))attachImage(*path);
}

bool ItemCreatorController::prepareImageHandout()
{
    const auto image=_model.metadata.customProperties.find("item.image");
    if(image==_model.metadata.customProperties.end() || image->second.empty())return false;
    const std::string imagePath=image->second;
    if(!saveItem())return false;
    // Snapshot only the explicitly player-facing image and item name. Item
    // rules, powers, GM descriptions and later edits never auto-publish.
    Campaign proposed=_currentCampaign;
    HandoutModel handout;handout.title=_model.name;handout.filePath=imagePath;
    proposed.handouts.push_back(handout);
    if(!ProjectSaveService::getInstance().saveCampaign(proposed,"prepare item image handout")) {
        _errorMessage=ProjectSaveService::getInstance().state().lastError;return false;
    }
    _currentCampaign=std::move(proposed);
    _statusMessage=uiText("Image handout saved, hidden from players. Review and reveal it in Player Display.",
        "Bild-Handout gespeichert, für Spieler verborgen. In der Spieleranzeige prüfen und freigeben.");
    return true;
}

ItemCreatorController::ItemCreatorController(Campaign &currentCampaign) : _currentCampaign(currentCampaign)
{
    _view = std::make_unique<ItemCreatorView>(*this);
    createNewDraft();
}

void ItemCreatorController::configure()
{
_retainedDrafts.clear();
    _model = ItemModel{};
    _selectedItemId.clear();
    _draftCampaignId = _currentCampaign.metadata.id;
    createNewDraft();
    _retainedDrafts.clear();
}

void ItemCreatorController::requestCatalogBrowser()
{
    _view->requestCatalogBrowser();
}

bool ItemCreatorController::selectItemById(const EntityId &itemId)
{
    if(!draftRecoveryReady())return false;
    for (int index = 0; index < static_cast<int>(_currentCampaign.customItems.size()); ++index)
    {
        if (_currentCampaign.customItems[static_cast<std::size_t>(index)].metadata.id != itemId)
            continue;
        selectItem(index);
        return true;
    }
    return false;
}

void ItemCreatorController::createNewDraft()
{
    retainCurrentDraft();
    _selectedItemId.clear();
    _draftBase.reset();
    _draftEdited = false;
    _model = ItemModel{};
    _model.rulesetKey = _currentCampaign.ruleSet ? _currentCampaign.ruleSet->getName() : "custom";
    _model.category = "Custom";
    _model.itemType = "Custom";
    _powerDraftEdited = false;
    _itemPowerDraft = MonsterActionModel{};
    _itemPowerDraft.actionKind = "Item Power";
    _selectedItemIndex = -1;
    _statusMessage.clear();
    _errorMessage.clear();
}

void ItemCreatorController::update(std::shared_ptr<GLFWwindow> window)
{
    if (!m_isOpen)
        return;

    refreshSelectedItemIndex();
    _view->update();
    if (!_view->isVisible())
        m_isOpen = false;
}

bool ItemCreatorController::saveItem(bool allowArchiveChange)
{
    if(!draftRecoveryReady())return false;
    markDraftEdited();

    _statusMessage.clear();
    _errorMessage.clear();
    if (_model.name.empty())
    {
        _errorMessage = uiText("Name is required.", "Ein Name ist erforderlich.");
        return false;
    }
    if (_powerDraftEdited)
    {
        if (_itemPowerDraft.name.empty())
        {
            _errorMessage = uiText("Name the pending item power before saving. Its fields are retained.",
                "Benenne die ausstehende Gegenstandskraft vor dem Speichern. Ihre Felder bleiben erhalten.");
            return false;
        }
        addPowerToDraftItem();
    }

    if (_model.category.empty())
        _model.category = _model.itemType.empty() ? "Custom" : _model.itemType;
    _model.containerCapacityWeight = std::max(0.0, _model.containerCapacityWeight);
    _model.containerCapacityItems = std::max(0, _model.containerCapacityItems);
    _model.maxCharges = std::max(0, _model.maxCharges);
    _model.maxDurability = std::max(0, _model.maxDurability);
    if (isWeaponItem(_model))
    {
        if (_model.weaponAbility.empty())
            _model.weaponAbility = "Strength";
        _model.weaponReachFeet = std::max(0, _model.weaponReachFeet);
        _model.weaponRangeFeet = std::max(_model.weaponReachFeet, _model.weaponRangeFeet);
        _model.weaponLongRangeFeet = std::max(_model.weaponRangeFeet, _model.weaponLongRangeFeet);
        if (_model.weaponAttackKind.empty())
            _model.weaponAttackKind = _model.weaponRangeFeet > _model.weaponReachFeet ? "Ranged" : "Melee";
    }

    if (!refreshSelectedItemIndex()) return false;
    const bool updating = !_selectedItemId.empty();
    const auto current=std::find_if(_currentCampaign.customItems.begin(),_currentCampaign.customItems.end(),
        [&](const auto &item){return item.metadata.id==_model.metadata.id;});
    const bool alreadySaved=current!=_currentCampaign.customItems.end()&&EditorDraftRecovery::sameItem(*current,_model);
    if(!alreadySaved && ((updating && (!_draftBase || current==_currentCampaign.customItems.end() ||
            !EditorDraftRecovery::sameItem(*current,*_draftBase))) ||
        (!updating && current!=_currentCampaign.customItems.end()))) {
        _errorMessage=uiText("This item's saved content changed. Your draft is retained; review it or copy it to a new item.",
            "Der gespeicherte Gegenstand wurde geändert. Dein Entwurf bleibt erhalten; prüfe ihn oder kopiere ihn als neuen Gegenstand.");
        return false;
    }
    if (updating && !allowArchiveChange &&
        isItemArchived(_currentCampaign.customItems[static_cast<std::size_t>(_selectedItemIndex)]) != isItemArchived(_model))
    {
        _errorMessage = uiText("The item's archive state changed. Your draft is retained; review Archive or Restore before saving.",
            "Der Archivstatus wurde geändert. Dein Entwurf bleibt erhalten; prüfe Archivieren oder Wiederherstellen vor dem Speichern.");
        return false;
    }
    _model.metadata.touch();
    // RAM equality permits retrying an already-applied draft, but is not proof
    // that the repository committed it. Always cross the durable save boundary.
    if (!persistCampaignEntityDraft(_currentCampaign, _model, &Campaign::customItems,
        [](Campaign &proposed) { return ProjectSaveService::getInstance().saveCampaign(proposed, "save item draft"); }))
    {
        _errorMessage = uiText("Not saved. Your item draft is retained. Use Save to retry. ",
            "Nicht gespeichert. Dein Gegenstandsentwurf bleibt erhalten. Nutze Speichern erneut. ") +
            ProjectSaveService::getInstance().state().lastError;
        return false;
    }
    _selectedItemId = _model.metadata.id;
    refreshSelectedItemIndex();
    _model = _currentCampaign.customItems[static_cast<std::size_t>(_selectedItemIndex)];
    _draftBase = _model;
    _draftEdited = false;
    _retainedDrafts.erase(_model.metadata.id);
    _catalogSearch.clear();
    _selectedCatalogCategoryIndex = 0;
    _selectedCatalogSourceIndex = 0;
    const auto entries = getCatalogEntries();
    const auto visible = getVisibleCatalogEntries(entries, getCatalogCategories(entries));
    const auto selected = std::find_if(visible.begin(), visible.end(), [&](const CatalogEntry &entry) {
        return entry.campaignCustom && entry.item.metadata.id == _model.metadata.id;
    });
    if (selected != visible.end()) _selectedCatalogIndex = static_cast<int>(std::distance(visible.begin(), selected));
    _statusMessage = (updating ? uiText("Updated: ", "Aktualisiert: ") : uiText("Created: ", "Erstellt: ")) + _model.name;
    if(_recoveryCommitted)_recoveryCommitted();
    return true;
}

bool ItemCreatorController::persistCampaignChanges()
{
    if (ProjectSaveService::getInstance().saveCampaign(_currentCampaign))
    {
        _errorMessage.clear();
        return true;
    }
    _statusMessage.clear();
    _errorMessage = uiText("Not saved. Changes remain in memory. Use Save to retry. ",
                           "Nicht gespeichert. Änderungen bleiben im Speicher. Nutze Speichern für einen neuen Versuch. ") +
                    ProjectSaveService::getInstance().state().lastError;
    return false;
}
