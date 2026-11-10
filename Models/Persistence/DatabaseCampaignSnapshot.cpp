#include "Models/DataBaseManager.hpp"
#include "Models/Map/MapFileIoPath.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "Models/Rulesets/Clockwork/ClockworkBloodline.hpp"
#include "Models/Rulesets/RulesetResolution.hpp"
#include "Models/Rulesets/RulesetLibrary.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

void DatabaseManager::writeCampaignSnapshot(const Campaign &source, const std::string &snapshotPath)
{
    const auto path = mapFileIoPath(std::filesystem::u8path(snapshotPath));
    if (std::filesystem::exists(path))
        throw std::runtime_error("A campaign snapshot requires a new destination file.");

    Campaign campaign = source;
    if (!campaign.ruleSet) throw std::runtime_error("Cannot export a campaign without its ruleset.");
    if (noRulesetPins(campaign.rulesetReference)) campaign.rulesetReference = rulesetReferenceFor(*campaign.ruleSet);
    if(campaign.rulesetReference.rulesetId.empty() || campaign.rulesetReference.version.empty() ||
       campaign.rulesetReference.overlayId.empty()!=campaign.rulesetReference.overlayVersion.empty())
        throw std::runtime_error("A campaign snapshot requires complete ruleset pins. The existing pins were not changed.");
    // Published definitions live in the repository library. Copy only the exact
    // base/overlay pins needed to interpret this campaign on another machine.
    const auto installed = getInstance()->loadInstalledRulesetDefinitions();
    auto available = campaign.rulesetDefinitions;
    available.insert(available.end(), installed.begin(), installed.end());
    const auto includeDefinition = [&](const std::string &id, const std::string &version, bool overlay) {
        if (id.empty()) return;
        SpellcastingValues::Reason reason;
        const auto *found = findPinnedRulesetDefinition(available, id, version, overlay, reason);
        if (!found)
        {
            const DND5e dnd;
            const ClockworkBloodlineRuleset clockwork;
            const bool compiled = !overlay && reason == SpellcastingValues::Reason::MissingBase &&
                ((id == dnd.getId() && version == dnd.getVersionId()) ||
                 (id == clockwork.getId() && version == clockwork.getVersionId()));
            if (!compiled) throw std::runtime_error("Missing pinned ruleset definition: " + id + " " + version);
            return;
        }
        const auto duplicate = std::find_if(campaign.rulesetDefinitions.begin(), campaign.rulesetDefinitions.end(),
            [&](const auto &definition) { return definition.metadata.id == found->metadata.id; });
        if (findPinnedRulesetDefinition(campaign.rulesetDefinitions, id, version, overlay, reason)) return;
        if (duplicate != campaign.rulesetDefinitions.end())
            throw std::runtime_error("A campaign ruleset definition identity conflicts with the required pin: " + id + " " + version);
        campaign.rulesetDefinitions.push_back(*found);
    };
    includeDefinition(campaign.rulesetReference.rulesetId, campaign.rulesetReference.version, false);
    includeDefinition(campaign.rulesetReference.overlayId, campaign.rulesetReference.overlayVersion, true);
    try
    {
        // Never VACUUM the live repository then delete unrelated rows: deleted
        // bytes and nested backups can still disclose unrelated campaign data.
        DatabaseManager snapshot(path.u8string());
        snapshot.saveCampaign(campaign);
    }
    catch (...)
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        throw;
    }
}
