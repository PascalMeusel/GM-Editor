#include "Views/Editor/EditorNavigationSupport.hpp"

void EditorWindowView::renderGlobalSearch()
{
    if (!_globalSearchOpen)
        return;

    const bool germanUi =
        gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const auto uiText = [germanUi](const char *english, const char *german) { return germanUi ? german : english; };

    const CompactUiDensityScope uiDensity;
    setResponsiveWindowSize(ImVec2(720.0f, 520.0f), ImVec2(480.0f, 360.0f));
    if (!ImGui::Begin(localizedUiLabel("Global Search").c_str(), &_globalSearchOpen))
    {
        ImGui::End();
        return;
    }

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
    FormUi::InputTextWithHint(localizedUiLabel("##GlobalSearchQuery").c_str(),
                              localizedUiText("Search names, tags, notes, scenes, maps, audio, shops, and dungeons"),
                              &_globalSearchQuery);
    if (drawUiButton("Save Search...", !_globalSearchQuery.empty(),
                     "Enter a search query before saving a smart collection.", UiButtonRole::Secondary,
                     ImVec2(126.0f, 0.0f), "Review and save the current query as a smart collection."))
        ImGui::OpenPopup(localizedUiLabel("Save Smart Collection").c_str());
    setResponsiveAutoSizeWindow(620.0f, 400.0f);
    if (ImGui::BeginPopupModal(localizedUiLabel("Save Smart Collection").c_str(), nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        const int previewCount =
            _globalSearchQuery.empty()
                ? 0
                : static_cast<int>(_controller.searchCurrentCampaign(_globalSearchQuery, 100).size());
        ImGui::TextWrapped(localizedUiLabel("Save this search as a smart collection?").c_str());
        drawInlineLabelValue("Query", _globalSearchQuery.c_str());
        drawInlineLabelValue("Preview", (std::to_string(previewCount) + " result(s), capped at 100").c_str());
        ImGui::SetNextItemWidth(360.0f);
        FormUi::InputTextWithHint(localizedUiLabel("Collection Name").c_str(),
                                  localizedUiText("Example: Active NPC threads"), &_newCollectionName);
        const std::string collectionName = _newCollectionName.empty() ? _globalSearchQuery : _newCollectionName;
        if (drawModalActionButtons("Save Collection", UiButtonRole::Primary))
        {
            _controller.saveCurrentSearchAsFilter(collectionName, _globalSearchQuery);
            _newCollectionName.clear();
        }
        ImGui::EndPopup();
    }
    const std::vector<CampaignSavedFilterModel> savedFilters = _controller.savedFilters();
    const std::vector<CampaignEntityBookmarkModel> favorites = _controller.favoriteItems();
    const std::vector<CampaignEntityBookmarkModel> recent = _controller.recentItems();
    const std::vector<CampaignBrokenReference> brokenReferences = _controller.brokenReferences();
    const bool hasSearchTools =
        !savedFilters.empty() || !favorites.empty() || !recent.empty() || !brokenReferences.empty();
    ImGui::SetNextItemOpen(false, ImGuiCond_Once);
    if (ImGui::CollapsingHeader(localizedUiLabel("Search Tools / Diagnostics").c_str()))
    {
        if (drawUiButton("Rebuild Index", UiButtonRole::Advanced, ImVec2(128.0f, 0.0f),
                         "Rebuild search documents, graph summaries, and broken-reference diagnostics."))
            _controller.rebuildCampaignIndex();
        const CampaignIndexRebuildResult &rebuildResult = _controller.lastIndexRebuildResult();
        if (!rebuildResult.rebuiltAtUtc.empty())
        {
            ImGui::TextColored(
                kMutedText,
                localizedUiLabel("Index: %d docs, %d graph nodes, %d graph edges, %d broken refs at %s").c_str(),
                rebuildResult.searchableDocuments, rebuildResult.graphNodes, rebuildResult.graphEdges,
                rebuildResult.brokenReferences, rebuildResult.rebuiltAtUtc.c_str());
        }

        if (!savedFilters.empty())
        {
            ImGui::SeparatorText(localizedUiLabel("Smart Collections").c_str());
            for (const CampaignSavedFilterModel &filter : savedFilters)
            {
                const std::vector<CampaignSearchResult> collectionResults =
                    _controller.evaluateSavedFilter(filter, 100);
                if (drawSmallUiButton(
                        (filter.name + " (" + std::to_string(collectionResults.size()) + ")##open" + filter.metadata.id)
                            .c_str(),
                        UiButtonRole::Secondary, "Apply this smart collection query."))
                    _globalSearchQuery = filter.query;
                ImGui::SameLine();
                if (drawSmallUiButton(("Delete##delete" + filter.metadata.id).c_str(), UiButtonRole::Destructive,
                                      "Delete this saved smart collection."))
                {
                    _controller.deleteSavedFilter(filter.metadata.id);
                    break;
                }
                if (!filter.query.empty())
                {
                    ImGui::SameLine();
                    ImGui::TextColored(kMutedText, localizedUiLabel("%s").c_str(), filter.query.c_str());
                }
            }
        }

        if (!favorites.empty() || !recent.empty())
        {
            ImGui::SeparatorText(localizedUiLabel("Favorites & Recent").c_str());
            for (const CampaignEntityBookmarkModel &bookmark : favorites)
            {
                if (drawSmallUiButton((std::string(uiText("Favorite", "Favorit")) + ": " + bookmark.label + "##fav" +
                                       bookmark.entityId)
                                          .c_str(),
                                      UiButtonRole::Primary, "Open this favorite entity."))
                    _controller.executePaletteCommand(
                        CampaignCommandPaletteItem{"open:" + bookmark.entityId, bookmark.label, bookmark.entityType,
                                                   bookmark.entityId, bookmark.entityType});
                ImGui::SameLine();
                const std::string entityType = localizedUiLabel(bookmark.entityType.c_str());
                ImGui::TextColored(kWarmAccent, localizedUiLabel("[%s]").c_str(), entityType.c_str());
            }
            for (const CampaignEntityBookmarkModel &bookmark : recent)
            {
                if (drawSmallUiButton((std::string(uiText("Recent", "Zuletzt verwendet")) + ": " + bookmark.label +
                                       "##recent" + bookmark.entityId)
                                          .c_str(),
                                      UiButtonRole::Secondary, "Open this recently used entity."))
                    _controller.executePaletteCommand(
                        CampaignCommandPaletteItem{"open:" + bookmark.entityId, bookmark.label, bookmark.entityType,
                                                   bookmark.entityId, bookmark.entityType});
                ImGui::SameLine();
                ImGui::TextColored(kMutedText, localizedUiLabel("%d opens").c_str(), bookmark.openCount);
            }
        }

        if (!brokenReferences.empty())
        {
            ImGui::SeparatorText(localizedUiLabel("Broken References").c_str());
            ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.38f, 1.0f), localizedUiLabel("%d broken references").c_str(),
                               static_cast<int>(brokenReferences.size()));
            if (ImGui::BeginChild("BrokenReferenceDiagnostics", ImVec2(0.0f, 96.0f), true))
            {
                for (const CampaignBrokenReference &diagnostic : brokenReferences)
                {
                    ImGui::BulletText("%s [%s] %s", diagnostic.sourceTitle.c_str(), diagnostic.sourceType.c_str(),
                                      diagnostic.relationship.c_str());
                    ImGui::TextColored(kMutedText, localizedUiLabel("Missing: %s").c_str(),
                                       diagnostic.missingEntityId.c_str());
                }
            }
            ImGui::EndChild();
        }
    }
    else if (hasSearchTools)
    {
        ImGui::TextColored(kMutedText, localizedUiLabel("Search tools are available in the collapsed drawer.").c_str());
    }
    ImGui::Separator();

    if (_globalSearchQuery.empty())
    {
        ImGui::TextColored(kMutedText, localizedUiLabel("Enter a search term.").c_str());
        ImGui::End();
        return;
    }

    const std::vector<CampaignSearchResult> results = _controller.searchCurrentCampaign(_globalSearchQuery, 100);
    ImGui::TextColored(kMutedText, localizedUiLabel("%d results").c_str(), static_cast<int>(results.size()));

    if (!_selectedTagEntityId.empty())
    {
        ImGui::SeparatorText(localizedUiLabel("Entity Details").c_str());
        pushBoldFont();
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _selectedTagEntityLabel.c_str());
        popBoldFont();
        ImGui::TextColored(kMutedText, localizedUiLabel("Tags").c_str());
        if (renderTagEditor("GlobalSearchTagEditor", _selectedTagEntityTags, _tagEditorState,
                            _controller.allCampaignTags()))
            _controller.setEntityTags(_selectedTagEntityId, _selectedTagEntityTags);

        const std::vector<CampaignBacklink> backlinks = _controller.backlinksToEntity(_selectedTagEntityId);
        ImGui::SeparatorText(localizedUiLabel("Backlinks").c_str());
        ImGui::TextColored(kMutedText, localizedUiLabel("Backlinks: %d").c_str(), static_cast<int>(backlinks.size()));
        if (backlinks.empty())
        {
            ImGui::TextDisabled(localizedUiLabel("No campaign entities currently link to this result.").c_str());
        }
        else
        {
            for (const CampaignBacklink &backlink : backlinks)
            {
                const std::string sourceType = localizedUiLabel(backlink.sourceType.c_str());
                ImGui::BulletText("%s [%s]", backlink.sourceTitle.c_str(), sourceType.c_str());
                ImGui::SameLine();
                ImGui::TextColored(kMutedText, localizedUiLabel("%s").c_str(),
                                   localizedUiText(backlink.relationship.c_str()));
            }
        }
    }

    ImGui::BeginChild("GlobalSearchResults", ImVec2(0.0f, 0.0f), true);
    for (const CampaignSearchResult &result : results)
    {
        const CampaignSearchDocument &document = result.document;
        pushBoldFont();
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), document.title.c_str());
        popBoldFont();
        ImGui::SameLine();
        const std::string documentType = document.type == "Objective" ? uiText("Objective", "Ziel") :
                                         document.type == "Clue" ? uiText("Clue", "Hinweis") :
                                         localizedUiLabel(document.type.c_str());
        ImGui::TextColored(kWarmAccent, localizedUiLabel("[%s]").c_str(), documentType.c_str());
        if (!document.subtitle.empty())
            ImGui::TextColored(kMutedText, localizedUiLabel("%s").c_str(), document.subtitle.c_str());
        if (!document.actionHint.empty())
            ImGui::TextColored(kMutedText, localizedUiLabel("Next: %s").c_str(),
                               localizedUiText(document.actionHint.c_str()));
        if (!document.tags.empty())
        {
            std::string tags;
            for (const std::string &tag : document.tags)
            {
                if (tag.empty())
                    continue;
                if (!tags.empty())
                    tags += ", ";
                tags += tag;
            }
            if (!tags.empty())
                ImGui::TextColored(kMutedText, localizedUiLabel("Tags: %s").c_str(), tags.c_str());
        }
        const std::string preview = document.body.size() > 180 ? document.body.substr(0, 177) + "..." : document.body;
        if (!preview.empty())
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), preview.c_str());
        const std::string openLabel = "Open##SearchOpen" + document.id;
        if (drawSmallUiButton(openLabel.c_str(), UiButtonRole::Primary, "Open the workspace that owns this entity."))
        {
            CampaignCommandPaletteItem item{"open:" + document.id, "Open " + document.title, document.type, document.id,
                                            document.type};
            _controller.executePaletteCommand(item);
        }
        ImGui::SameLine();
        const std::string copyLabel = "Copy ID##SearchCopyId" + document.id;
        if (drawSmallUiButton(copyLabel.c_str(), UiButtonRole::Secondary,
                              "Copy the stable reference ID for linking or diagnostics."))
            ImGui::SetClipboardText(document.id.c_str());
        ImGui::SameLine();
        if (drawSmallUiButton(("Edit Tags##" + document.id).c_str(), UiButtonRole::Secondary,
                              "Edit tags for this search result."))
        {
            _selectedTagEntityId = document.id;
            _selectedTagEntityLabel = document.title + " [" + document.type + "]";
            _selectedTagEntityTags = _controller.tagsForEntity(document.id);
            _tagEditorState.draftTag.clear();
        }
        ImGui::SameLine();
        const std::string backlinksLabel = "Backlinks##SearchBacklinks" + document.id;
        if (drawSmallUiButton(backlinksLabel.c_str(), UiButtonRole::Secondary,
                              "Reveal campaign entities that reference this result."))
        {
            _selectedTagEntityId = document.id;
            _selectedTagEntityLabel = document.title + " [" + document.type + "]";
            _selectedTagEntityTags = _controller.tagsForEntity(document.id);
            _tagEditorState.draftTag.clear();
        }
        ImGui::SameLine();
        const bool favorite = _controller.isFavoriteEntity(document.id);
        const std::string favoriteLabel = std::string(favorite ? "Unfavorite##" : "Favorite##") + document.id;
        if (drawSmallUiButton(favoriteLabel.c_str(), favorite ? UiButtonRole::Advanced : UiButtonRole::Secondary,
                              favorite ? "Remove this entity from favorites." : "Add this entity to favorites."))
            _controller.setFavoriteEntity(document.id, document.type, document.title, !favorite);
        ImGui::TextColored(kMutedText, localizedUiLabel("Reference ID: %s").c_str(), document.id.c_str());
        ImGui::Separator();
    }
    ImGui::EndChild();
    ImGui::End();
}

void EditorWindowView::renderCommandPalette()
{
    if (!_commandPaletteOpen)
        return;

    const CompactUiDensityScope uiDensity;
    setResponsiveWindowSize(ImVec2(640.0f, 520.0f), ImVec2(440.0f, 340.0f));
    if (!ImGui::Begin(localizedUiLabel("Command Palette").c_str(), &_commandPaletteOpen))
    {
        ImGui::End();
        return;
    }

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
    FormUi::InputTextWithHint(localizedUiLabel("##CommandPaletteQuery").c_str(),
                              localizedUiText("Search commands or entities"), &_commandPaletteQuery);
    ImGui::Separator();

    const std::vector<CampaignCommandPaletteItem> items = _controller.commandPaletteItems(_commandPaletteQuery);
    ImGui::BeginChild("CommandPaletteItems", ImVec2(0.0f, 0.0f), true);
    for (const CampaignCommandPaletteItem &item : items)
    {
        pushBoldFont();
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), item.label.c_str());
        popBoldFont();
        ImGui::SameLine();
        ImGui::TextColored(kWarmAccent, localizedUiLabel("[%s]").c_str(), item.category.c_str());
        ImGui::SameLine();
        if (drawSmallUiButton(("Run##" + item.id).c_str(), UiButtonRole::Primary,
                              "Run this command or open this entity."))
        {
            _controller.executePaletteCommand(item);
            _commandPaletteOpen = false;
            _commandPaletteQuery.clear();
        }
        ImGui::Separator();
    }
    ImGui::EndChild();
    ImGui::End();
}
