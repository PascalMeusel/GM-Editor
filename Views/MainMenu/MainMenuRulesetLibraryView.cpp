#include "MainMenuCampaignSupport.hpp"

void MainMenuWindowView::drawRulesetStudioLibrary()
{
            const std::vector<RulesetStudioFamily> families = _controller.loadRulesetStudioLibrary();
            ImGui::SeparatorText(rulesetStudioText("Ruleset library", "Regelwerk-Bibliothek"));
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), rulesetStudioText(
                "Open an editable draft or inspect the immutable versions currently used by campaigns.",
                "Öffne einen bearbeitbaren Entwurf oder prüfe die unveränderlichen Versionen, die Kampagnen verwenden."));

            if (drawUiButton(rulesetStudioText("New ruleset", "Neues Regelwerk"),
                             UiButtonRole::Primary,
                             ImVec2(180.0f, 0.0f),
                             rulesetStudioText("Start a new executable ruleset definition.",
                                               "Erstellt eine neue ausführbare Regelwerk-Definition.")))
            {
                beginNewRulesetDefinition();
            }
            continueRulesetActionRow(rulesetStudioText("Close", "Schließen"), 120.0f);
            if (drawUiButton(rulesetStudioText("Close", "Schließen"), UiButtonRole::Secondary, ImVec2(120.0f, 0.0f)))
            {
                _showAddRulesetPopup = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::Separator();
            if (families.empty())
            {
                ImGui::TextDisabled(localizedUiLabel("%s").c_str(), rulesetStudioText(
                    "No custom rulesets yet. Create one to define sheets, checks, resources, and conditions without code.",
                    "Noch keine eigenen Regelwerke. Erstelle eines, um Bögen, Proben, Ressourcen und Zustände ohne Code zu definieren."));
            }
            else
            {
                _selectedRulesetFamilyIndex = std::clamp(_selectedRulesetFamilyIndex, 0, static_cast<int>(families.size()) - 1);
                const float libraryChildHeight = std::max(
                    1.0f,
                    ImGui::GetContentRegionAvail().y - ImGui::GetStyle().CellPadding.y * 2.0f);
                if (ImGui::BeginTable("RulesetStudioLibrary", 2,
                                      ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoSavedSettings))
                {
                    ImGui::TableSetupColumn(localizedUiLabel("Library").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.34f);
                    ImGui::TableSetupColumn(localizedUiLabel("Detail").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.66f);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::BeginChild("RulesetFamilies", ImVec2(0.0f, libraryChildHeight), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
                    for (int index = 0; index < static_cast<int>(families.size()); ++index)
                    {
                        const RulesetStudioFamily &family = families[static_cast<std::size_t>(index)];
                        const std::string label = family.name + "##RulesetFamily" + std::to_string(index);
                        if (ImGui::Selectable(label.c_str(), _selectedRulesetFamilyIndex == index))
                        {
                            _selectedRulesetFamilyIndex = index;
                            _selectedPublishedRulesetIndex = 0;
                        }
                        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), (std::to_string(family.drafts.size()) +
                                                   rulesetStudioText(" draft, ", " Entwurf, ") +
                                                   std::to_string(family.publishedVersions.size()) +
                                                   rulesetStudioText(" versions", " Versionen")).c_str());
                    }
                    ImGui::EndChild();

                    ImGui::TableSetColumnIndex(1);
                    ImGui::BeginChild("RulesetFamilyDetail", ImVec2(0.0f, libraryChildHeight), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
                    const RulesetStudioFamily &family = families[static_cast<std::size_t>(_selectedRulesetFamilyIndex)];
                    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), family.name.c_str());
                    ImGui::TextDisabled(localizedUiLabel("%s: %s").c_str(), rulesetStudioText("Stable ruleset ID", "Stabile Regelwerk-ID"), family.rulesetId.c_str());
                    ImGui::TextDisabled(localizedUiLabel("%s: %d").c_str(), rulesetStudioText("Campaigns using this ruleset", "Kampagnen mit diesem Regelwerk"), family.campaignUseCount());

                    ImGui::SeparatorText(rulesetStudioText("Editable draft", "Bearbeitbarer Entwurf"));
                    if (!family.drafts.empty())
                    {
                        const RulesetDefinitionModel &draft = family.drafts.front().definition;
                        ImGui::Text(localizedUiLabel("%s %s").c_str(), rulesetStudioText("Target version", "Zielversion"), draft.version.c_str());
                        if (drawUiButton(rulesetStudioText("Open draft", "Entwurf öffnen"),
                                         UiButtonRole::Primary,
                                         ImVec2(180.0f, 0.0f),
                                         rulesetStudioText("Continue editing this unpublished definition.",
                                                           "Diesen unveröffentlichten Entwurf weiter bearbeiten.")))
                            beginEditingRulesetDefinition(draft);
                    }
                    else
                    {
                        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), rulesetStudioText("No draft. Published versions remain read-only.",
                                                                     "Kein Entwurf. Veröffentlichte Versionen bleiben schreibgeschützt."));
                    }

                    ImGui::SeparatorText(rulesetStudioText("Version history", "Versionsverlauf"));
                    if (family.publishedVersions.empty())
                    {
                        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), rulesetStudioText("Nothing published yet.", "Noch keine Version veröffentlicht."));
                    }
                    else
                    {
                        _selectedPublishedRulesetIndex = std::clamp(
                            _selectedPublishedRulesetIndex, 0, static_cast<int>(family.publishedVersions.size()) - 1);
                        for (int index = 0; index < static_cast<int>(family.publishedVersions.size()); ++index)
                        {
                            const RulesetStudioVersionEntry &entry = family.publishedVersions[static_cast<std::size_t>(index)];
                            const std::string label = entry.definition.version + "  ·  " +
                                                      std::to_string(entry.campaignUseCount) +
                                                      rulesetStudioText(" campaigns##Published", " Kampagnen##Published") +
                                                      std::to_string(index);
                            if (ImGui::Selectable(label.c_str(), _selectedPublishedRulesetIndex == index))
                                _selectedPublishedRulesetIndex = index;
                        }

                        const RulesetStudioVersionEntry &selectedVersion =
                            family.publishedVersions[static_cast<std::size_t>(_selectedPublishedRulesetIndex)];
                        const RulesetDefinitionModel &published = selectedVersion.definition;
                        ImGui::Separator();
                        ImGui::Text(localizedUiLabel("%s %s").c_str(), rulesetStudioText("Published version", "Veröffentlichte Version"), published.version.c_str());
                        if (!published.publishedAtUtc.empty())
                            ImGui::TextDisabled(localizedUiLabel("%s").c_str(), published.publishedAtUtc.c_str());
                        ImGui::TextDisabled(localizedUiLabel("%zu %s · %zu %s · %zu %s").c_str(),
                                            published.abilityScores.size(), rulesetStudioText("abilities", "Attribute"),
                                            published.derivedStats.size(), rulesetStudioText("derived stats", "abgeleitete Werte"),
                                            published.resources.size(), rulesetStudioText("resources", "Ressourcen"));
                        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), rulesetStudioText(
                            "Read-only: publishing creates an immutable artifact.",
                            "Schreibgeschützt: Eine Veröffentlichung erzeugt ein unveränderliches Artefakt."));

                        const bool canCreateDraft = family.drafts.empty();
                        if (drawUiButton(rulesetStudioText("Create next version draft", "Entwurf für nächste Version"),
                                         canCreateDraft,
                                         rulesetStudioText("Open the existing draft before creating another version.",
                                                           "Öffne den vorhandenen Entwurf, bevor du eine weitere Version erstellst."),
                                         UiButtonRole::Advanced,
                                         ImVec2(250.0f, 0.0f),
                                         rulesetStudioText("Copy this version into a new editable patch-version draft.",
                                                           "Diese Version in einen neuen bearbeitbaren Patch-Entwurf kopieren.")))
                        {
                            beginEditingRulesetDefinition(_controller.createRulesetVersionDraft(published));
                            _statusMessage = rulesetStudioText("Created next-version ruleset draft.",
                                                               "Entwurf für die nächste Regelwerk-Version erstellt.");
                        }
                    }
                    ImGui::EndChild();
                    ImGui::EndTable();
                }
            }
}
