#pragma once

#include "Models/Campaign/Campaign.hpp"
#include "Models/Rulesets/RulesetDefinitionModel.hpp"
#include "Models/Rulesets/RulesetDefinitionValidator.hpp"
#include "Models/Rulesets/RulesetMessageTypes.hpp"

#include <string>
#include <vector>

struct RulesetPublishResult
{
    bool success = false;
    RulesetDefinitionModel version;
    std::vector<RulesetValidationDiagnostic> validationDiagnostics;
    std::vector<std::string> errors;
};

struct RulesetUpgradePreview
{
    bool valid = true;
    // Presentation-neutral changes for localized UI. `changes` remains the
    // legacy English representation used by existing integrations and logs.
    std::vector<RulesetUpgradeChange> structuredChanges;
    std::vector<std::string> changes;
    std::vector<std::string> affectedStableIds;
    std::vector<RulesetValidationDiagnostic> validationDiagnostics;
    std::vector<std::string> errors;
    int affectedCharacterCount = 0;
    int affectedCharacterValueCount = 0;
    bool overlayApplied = false;
    std::string overlayId;
    std::string overlayVersion;
};

RulesetPublishResult publishRulesetVersion(const RulesetDefinitionModel &draft,
                                           const std::string &publishedAtUtc);
RulesetUpgradePreview createRulesetUpgradePreview(const RulesetDefinitionModel &current,
                                                  const RulesetDefinitionModel &next,
                                                  const RulesetDefinitionModel *overlay = nullptr);
bool migrateCampaignToRuleset(Campaign &campaign,
                              const RulesetDefinitionModel &next,
                              const RulesetDefinitionModel *overlay,
                              std::vector<std::string> &errors);
