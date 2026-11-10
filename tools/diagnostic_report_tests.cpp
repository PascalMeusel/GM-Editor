#include "Models/Project/DiagnosticReport.hpp"
#include <iostream>
int main()
{
    Campaign campaign; campaign.metadata.id = "PRIVATE_ID_CANARY"; campaign.name = "PRIVATE_NAME_CANARY";
    campaign.description = "PRIVATE_TEXT_CANARY"; campaign.rulesetReference.version = "PRIVATE_PIN_CANARY";
    HandoutModel handout; handout.filePath = "C:/PRIVATE_PATH_CANARY"; handout.body = "PRIVATE_HANDOUT_CANARY";
    campaign.handouts.push_back(handout);
    ProjectSaveState state; state.lastError = "PRIVATE_ERROR_CANARY"; state.lastRecoverySnapshotPath = "PRIVATE_BACKUP_CANARY";
    state.lastSaveWarnings = {"PRIVATE_WARNING_CANARY"}; state.dirty = true;
    const auto report = DiagnosticReport::preview(campaign, state, {});
    if (report.find("PRIVATE_") != std::string::npos || report.find("handouts=1") == std::string::npos ||
        report.find("save_error_present=1") == std::string::npos || report.find("save_pending=1") == std::string::npos)
    { std::cerr << "Diagnostic allowlist leaked private strings or omitted state\n"; return 1; }
    std::cout << "Diagnostic preview contains only allowlisted counts, versions and flags\n";
}
