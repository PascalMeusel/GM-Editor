#include "Controllers/Editor/EditorWindowControllerSupport.hpp"

#include <cctype>

namespace
{
std::string trimCampaignName(std::string value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }).base();
    return first < last ? std::string(first, last) : std::string{};
}

std::string normalizedCampaignName(std::string value)
{
    value = trimCampaignName(std::move(value));
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}
} // namespace

std::vector<ItemModel> EditorWindowController::availableWorldItems() const
{
    std::vector<ItemModel> items;
    if (_currentCampaign.ruleSet)
        items = _currentCampaign.ruleSet->getItems();
    items.insert(items.end(), _currentCampaign.customItems.begin(), _currentCampaign.customItems.end());
    std::sort(items.begin(), items.end(), [](const ItemModel &left, const ItemModel &right) {
        return left.name < right.name;
    });
    return items;
}

void EditorWindowController::activateWorkspaceByIndex(int workspaceIndex)
{
    static constexpr EditorWorkspace workspaces[] = {
        EditorWorkspace::Home,
        EditorWorkspace::Story,
        EditorWorkspace::Cast,
        EditorWorkspace::World,
        EditorWorkspace::Encounters,
        EditorWorkspace::Compendium,
        EditorWorkspace::Live,
        EditorWorkspace::Settings};
    constexpr int workspaceCount = static_cast<int>(sizeof(workspaces) / sizeof(workspaces[0]));
    if (workspaceIndex < 0 || workspaceIndex >= workspaceCount)
        return;
    view->activateWorkspace(workspaces[workspaceIndex]);
}

void EditorWindowController::executePaletteCommand(const CampaignCommandPaletteItem &item)
{
    if (!item.entityId.empty())
    {
        CampaignBookmarks::recordOpened(_currentCampaign, item);
        persistCurrentCampaign("record recent item");
    }

    if (const gm::ux::LegacySurfaceRegistration *surface = gm::ux::findLegacySurface(item.id);
        surface && !gm::ux::isLegacyEntryPointAllowed(*surface, gm::ux::LegacyEntryPoint::UniversalSearch))
        return;

    if (item.id == "workspace:home")
        view->activateWorkspace(EditorWorkspace::Home);
    else if (item.id == "workspace:story")
        view->activateWorkspace(EditorWorkspace::Story);
    else if (item.id == "workspace:cast")
        view->activateWorkspace(EditorWorkspace::Cast);
    else if (item.id == "workspace:world")
        view->activateWorkspace(EditorWorkspace::World);
    else if (item.id == "workspace:encounters")
        view->activateWorkspace(EditorWorkspace::Encounters);
    else if (item.id == "workspace:compendium")
        view->activateWorkspace(EditorWorkspace::Compendium);
    else if (item.id == "workspace:live")
        view->activateWorkspace(EditorWorkspace::Live);
    else if (item.id == "action:open_tabletop_toolkit")
        showTabletopToolkit();
    else if (item.id == "action:open_objectives")
        showTabletopObjectives();
    else if (item.id == "action:open_clues")
        showTabletopClues();
    else if (item.id == "action:review_player_display")
    {
        if (!view->activateWorkspace(EditorWorkspace::Live)) return;
        showPresentationPanel();
    }
    else if (item.id == "workspace:settings")
        view->activateWorkspace(EditorWorkspace::Settings);
    else if (item.id == "action:new_character")
    {
        if (!view->activateWorkspace(EditorWorkspace::Cast)) return;
        showPlayerCreator();
    }
    else if (item.id == "action:new_npc")
    {
        if (!view->activateWorkspace(EditorWorkspace::Cast)) return;
        showNPCCreator();
    }
    else if (item.id == "action:new_map")
    {
        if (!view->activateWorkspace(EditorWorkspace::World)) return;
        showMapCreator();
    }
    else if (item.id == "action:new_encounter")
    {
        if (!view->activateWorkspace(EditorWorkspace::Encounters)) return;
        showEncounterBuilder();
    }
    else if (item.id == "action:new_item")
    {
        if (!view->activateWorkspace(EditorWorkspace::Compendium)) return;
        showItemCreator();
    }
    else if (item.id == "action:save")
        saveCurrentCampaign("command palette save");
    else if (item.id == "action:rebuild_index")
        rebuildCampaignIndex();
    else if (item.id.rfind("roll:", 0) == 0)
    {
        RandomTableRollResult roll = rollRandomTable(item.entityId);
        if (!view->activateWorkspace(EditorWorkspace::Compendium, true)) return;
        hideAllWorkspaceTools();
        view->_compendiumCategoryFilter = 4;
        view->_workspaceSelectionId = item.entityId;
        view->reloadRandomTableDraft(item.entityId);
        if (roll.rolled)
        {
            view->_randomTableRollHistory.insert(view->_randomTableRollHistory.begin(), roll);
            if (view->_randomTableRollHistory.size() > 10)
                view->_randomTableRollHistory.resize(10);
            view->_randomTableStatus = RandomTables::formatRoll(roll);
        }
        else
            view->_randomTableStatus = roll.error;
    }
    else if (item.id.rfind("open:", 0) == 0)
        openWorkspaceEntity(item.entityType, item.entityId);
}

std::vector<CampaignEntityBookmarkModel> EditorWindowController::favoriteItems() const
{
    return CampaignBookmarks::favorites(_currentCampaign);
}

std::vector<CampaignEntityBookmarkModel> EditorWindowController::recentItems() const
{
    return CampaignBookmarks::recent(_currentCampaign, 10);
}

bool EditorWindowController::isFavoriteEntity(const EntityId &entityId) const
{
    return CampaignBookmarks::isFavorite(_currentCampaign, entityId);
}

void EditorWindowController::setFavoriteEntity(const EntityId &entityId, const std::string &entityType, const std::string &label, bool favorite)
{
    CampaignBookmarks::setFavorite(_currentCampaign, entityId, entityType, label, favorite);
    persistCurrentCampaign("edit favorite");
}

CampaignIndexRebuildResult EditorWindowController::rebuildCampaignIndex()
{
    EntityRegistry::getInstance().rebuild(_currentCampaign);
    _workspace.lastIndexRebuildResult = CampaignIndexRebuild::rebuild(_currentCampaign);
    _workspace.cachedSearchIndex.rebuild(_currentCampaign);
    _workspace.searchIndexDirty = false;
    return _workspace.lastIndexRebuildResult;
}

std::vector<CampaignSearchResult> EditorWindowController::evaluateSavedFilter(const CampaignSavedFilterModel &filter, std::size_t limit) const
{
    ensureSearchIndex();
    return CampaignCollections::evaluate(_workspace.cachedSearchIndex, filter, limit);
}

void EditorWindowController::saveCurrentSearchAsFilter(const std::string &name, const std::string &query)
{
    CampaignSavedFilterModel filter;
    filter.name = name.empty() ? "Saved Search" : name;
    filter.query = query;
    filter.smartCollection = true;
    _currentCampaign.savedFilters.push_back(filter);
    persistCurrentCampaign("save smart collection");
}

void EditorWindowController::deleteSavedFilter(const EntityId &filterId)
{
    auto &filters = _currentCampaign.savedFilters;
    filters.erase(std::remove_if(filters.begin(), filters.end(), [&](const CampaignSavedFilterModel &filter) {
        return filter.metadata.id == filterId;
    }), filters.end());
    persistCurrentCampaign("delete smart collection");
}

bool EditorWindowController::updateCampaignDetails(const std::string &name, const std::string &description,
                                                   std::string *error)
{
    const std::string trimmedName = trimCampaignName(name);
    if (trimmedName.empty())
    {
        if (error) *error = uiText("Campaign name is required.", "Ein Kampagnenname ist erforderlich.");
        return false;
    }

    const std::string normalizedName = normalizedCampaignName(trimmedName);
    for (const Campaign &existing : CampaignRepository::loadCampaignSummaries())
    {
        if (existing.metadata.id != _currentCampaign.metadata.id &&
            normalizedCampaignName(existing.name) == normalizedName)
        {
            if (error) *error = uiText("Another campaign already uses that name.",
                                       "Eine andere Kampagne verwendet diesen Namen bereits.");
            return false;
        }
    }

    if (_currentCampaign.name == trimmedName && _currentCampaign.description == description)
    {
        if (error) error->clear();
        return true;
    }

    Campaign staged = _currentCampaign;
    staged.name = trimmedName;
    staged.description = description;
    const ProjectSaveResult result = ProjectSaveService::getInstance().saveCampaignDetailed(staged, "save campaign details");
    if (!result.committed())
    {
        if (error) *error = result.error.empty()
                                ? uiText("Campaign details could not be saved.",
                                         "Kampagnendetails konnten nicht gespeichert werden.")
                                : result.error;
        return false;
    }

    _currentCampaign = std::move(staged);
    markSearchIndexDirty();
    if (error) error->clear();
    return true;
}

void EditorWindowController::setCampaignDescription(const std::string &description, const std::string &saveReason)
{
    (void)saveReason;
    std::string ignored;
    updateCampaignDetails(_currentCampaign.name, description, &ignored);
}

bool EditorWindowController::setCampaignContentLanguage(const std::string &language)
{
    const std::string normalized = language == "de" ? "de" : "en";
    if (_currentCampaign.contentLanguage == normalized)
        return true;

    const std::string previous = _currentCampaign.contentLanguage;
    _currentCampaign.contentLanguage = normalized;
    if (persistCurrentCampaign("change campaign content language"))
        return true;

    _currentCampaign.contentLanguage = previous;
    return false;
}

void EditorWindowController::markSearchIndexDirty()
{
    _workspace.searchIndexDirty = true;
}

void EditorWindowController::ensureSearchIndex() const
{
    if (!_workspace.searchIndexDirty)
        return;

    _workspace.cachedSearchIndex.rebuild(_currentCampaign);
    _workspace.searchIndexDirty = false;
}
