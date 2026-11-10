#pragma once
#include "Models/Core/ProductInfo.hpp"
#include "Models/Persistence/DatabaseSchemaVersion.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include <sstream>

namespace DiagnosticReport
{
// An allowlist, not a redaction pass over logs. Never serialize IDs, names,
// paths, note text, environment variables or arbitrary exception messages.
inline std::string preview(const Campaign &campaign, const ProjectSaveState &state, const ProjectSaveConfig &config)
{
    std::ostringstream out;
    out << "GM Editor diagnostic summary v1\napplication_version=" << ProductInfo::version
        << "\nsupported_schema=" << DatabaseSchema::CurrentVersion
#ifdef _WIN32
        << "\nplatform=Windows"
#elif defined(__APPLE__)
        << "\nplatform=macOS"
#else
        << "\nplatform=Linux/Unix"
#endif
        << "\npointer_bits=" << sizeof(void *) * 8
        << "\nmaps=" << campaign.maps.size() << "\nsessions=" << campaign.sessions.size()
        << "\ncharacters=" << campaign.characters.size() << "\nnpcs=" << campaign.npcs.size()
        << "\nencounters=" << campaign.encounters.size() << "\nhandouts=" << campaign.handouts.size()
        << "\nobjectives=" << campaign.storyObjectives.size() << "\nclues=" << campaign.storyClues.size()
        << "\nruleset_available=" << static_cast<bool>(campaign.ruleSet)
        << "\nautosave_enabled=" << config.autosaveEnabled
        << "\nautosave_idle_seconds=" << config.autosaveIntervalSeconds
        << "\nautosave_max_dirty_seconds=60"
        << "\nbackup_enabled=" << config.backupSnapshotsEnabled
        << "\nrecovery_enabled=" << config.recoverySnapshotsEnabled
        << "\nsave_pending=" << state.dirty << "\nsave_in_progress=" << state.currentlySaving
        << "\nsave_error_present=" << !state.lastError.empty()
        << "\nbackup_error_present=" << !state.lastBackupError.empty()
        << "\nrecovery_error_present=" << !state.lastRecoveryError.empty()
        << "\nsave_warning_count=" << state.lastSaveWarnings.size()
        << "\n\nCampaign text, identifiers, filenames, paths, logs and attachments are excluded.\n";
    return out.str();
}
}
