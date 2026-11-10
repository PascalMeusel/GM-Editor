#pragma once

#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Views/Editor/Widgets/Magic/SpellLibrary/SpellLibraryView.hpp"

#include <vector>
#include <map>

class SpellLibraryController : public IWidgetController
{
public:
    explicit SpellLibraryController(Campaign &currentCampaign);

    void update(std::shared_ptr<GLFWwindow> window) override;
    void configure();
    bool selectSpellById(const EntityId &spellId);
    void resetDrafts();
    void markDraftEdited() { _draftEdited = true; }
    bool hasUnsavedDrafts() const;
    bool flushDrafts();

private:
    friend class SpellLibraryView;
    friend struct SpellWorkflowProbe;

    Campaign &_currentCampaign;
    std::unique_ptr<SpellLibraryView> _view;
    SpellModel _draftSpell;
    EntityId _selectedSpellId, _draftCampaignId;
    bool _draftEdited = false;
    struct RetainedDraft { SpellModel spell; EntityId sourceId; bool dirty = false; };
    std::map<EntityId, RetainedDraft> _retainedDrafts;
    void retainDraft();
    bool restoreDraft(const EntityId &id);
    int _selectedSpellIndex = -1;
    int _selectedCatalogIndex = 0;
    int _selectedCatalogLevelIndex = 0;
    int _selectedCatalogSourceIndex = 0;
    std::string _catalogSearch;
    std::string _statusMessage;
    std::string _importText;
    std::string _importSource = "SRD-compatible import";
    std::string _importLicense;
    std::string _importPreviewMessage;
    std::string _importReceiptMessage;
    bool _importPreviewReady = false;

    struct CatalogEntry
    {
        SpellModel spell;
        std::string source;
        bool campaignCustom = false;
        int customSpellIndex = -1;
    };

    struct ImportPreviewRow
    {
        SpellModel spell;
        std::string action;
        std::vector<std::string> warnings;
        bool updatesExisting = false;
    };

    std::vector<ImportPreviewRow> _importPreviewRows;

    SpellModel *getSelectedSpell();
    void createNewDraft();
    void selectSpell(int spellIndex);
    void selectCatalogCustomSpell(const CatalogEntry &entry);
    void copyCatalogSpellToCampaign(const CatalogEntry &entry);
    void previewImportFromText();
    void applyImportPreview();
    std::string importRollbackNotice() const;
    std::string importPreviewRollbackPlan() const;
    bool saveDraft();
    void deleteSelectedSpell();
    std::vector<std::string> validateSpell(const SpellModel &spell) const;
    std::vector<std::string> validateDraftSpell() const;
    bool draftHasBlockingValidationErrors() const;
    std::string resolutionGuidance(SpellResolutionType resolution) const;
    std::string resolutionSummary(const SpellModel &spell) const;
    std::vector<CatalogEntry> getCatalogEntries() const;
    std::vector<std::string> getCatalogLevels(const std::vector<CatalogEntry> &entries) const;
    std::vector<CatalogEntry> getVisibleCatalogEntries(const std::vector<CatalogEntry> &entries, const std::vector<std::string> &levels) const;
    void saveCampaign(const std::string &reason = "");
    void onShow() override;
};
