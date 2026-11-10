#pragma once
// Included inside campaign_export_tests.cpp's fixture namespace. These cases
// inject error codes at one filesystem operation, never change a real volume,
// quota or ACL, and keep actual staging, SQLite, validation and cleanup paths.

void requireNoExportCanaries(const fs::path &path)
{
    for (const auto &[name, content] : filesIn(path))
        require(content.find(otherSecret) == std::string::npos && content.find(excludedSecret) == std::string::npos,
                "Unrelated/private package canary appeared in " + name);
}
void requireNoExportStaging(const fs::path &destination)
{
    const auto prefix = destination.filename().string() + ".staging_";
    for (const auto &entry : fs::directory_iterator(destination.parent_path()))
        require(entry.path().filename().string().find(prefix) != 0, "Failed export left an incomplete staging directory");
}
void testInjectedFilesystemFailures(const Campaign &campaign, const fs::path &root)
{
    using namespace ProjectPackageExport;
    const auto real = ProjectPackageExport::detail::systemFileOperations();
    for (const auto code : {std::errc::no_space_on_device, std::errc::permission_denied})
    {
        ExportRequest request;
        request.destinationDirectory = root / (code == std::errc::no_space_on_device ? "copy-no-space" : "copy-denied");
        const auto baseline = exportCampaignPackage(campaign, request); inspectExport(baseline, campaign);
        const auto before = filesIn(baseline.exportPath);
        request.overwriteExisting = true;
        auto operations = real; int copies = 0; bool injected = false;
        operations.copyFile = [&](const fs::path &source, const fs::path &target) {
            if (copies == 1)
            { injected = true; throw fs::filesystem_error("Injected asset-copy failure", source, target, std::make_error_code(code)); }
            real.copyFile(source, target); ++copies;
            require(readFile(target).find(otherSecret) == std::string::npos, "Copied unrelated campaign bytes before injection");
        };
        const auto failed = ProjectPackageExport::detail::exportWithFileOperations(campaign, request, operations);
        require(!failed.success && !failed.errorMessage.empty() && copies == 1 && injected,
                "Injected copy error did not fail after exactly one real copied asset");
        require(filesIn(baseline.exportPath) == before, "Injected copy failure changed the old package bytes");
        requireNoExportStaging(baseline.exportPath); requireNoExportCanaries(baseline.exportPath);
        const auto retry = exportCampaignPackage(campaign, request); inspectExport(retry, campaign);
    }
    for (const bool failRollback : {false, true})
    {
        ExportRequest request;
        request.destinationDirectory = root / (failRollback ? "rollback-denied" : "publication-denied");
        const auto baseline = exportCampaignPackage(campaign, request); inspectExport(baseline, campaign);
        const auto before = filesIn(baseline.exportPath);
        auto previous = baseline.exportPath; previous += ".previous";
        request.overwriteExisting = true;
        auto operations = real; int renames = 0; bool stagedDatabaseChecked = false;
        operations.renameDirectory = [&](const fs::path &source, const fs::path &target) {
            ++renames;
            if (renames == 2)
            {
                // First rename really displaced the prior package. The second
                // receives the real fully built, validated SQLite/files staging.
                require(target == baseline.exportPath && filesIn(previous) == before,
                        "Publication failure did not occur after real displacement");
                SQLite::Database snapshot((source / "campaign.db").string(), SQLite::OPEN_READONLY);
                require(snapshot.execAndGet("SELECT count(*) FROM campaigns;").getInt() == 1 &&
                        snapshot.execAndGet("SELECT stable_id FROM campaigns;").getString() == campaign.metadata.id,
                        "Injected publication did not exercise a real isolated snapshot");
                requireNoExportCanaries(source); stagedDatabaseChecked = true;
                throw fs::filesystem_error("Injected publication rename failure", source, target,
                                           std::make_error_code(std::errc::permission_denied));
            }
            if (renames == 3 && failRollback)
                throw fs::filesystem_error("Injected rollback rename failure", source, target,
                                           std::make_error_code(std::errc::permission_denied));
            real.renameDirectory(source, target);
        };
        const auto failed = ProjectPackageExport::detail::exportWithFileOperations(campaign, request, operations);
        require(!failed.success && !failed.errorMessage.empty() && renames == 3 && stagedDatabaseChecked,
                "Injected publication/rollback path was not exercised");
        requireNoExportStaging(baseline.exportPath);
        if (!failRollback)
        {
            require(filesIn(baseline.exportPath) == before && !fs::exists(previous),
                    "Publication failure did not restore exact old bytes");
            requireNoExportCanaries(baseline.exportPath);
        }
        else
        {
            require(!fs::exists(baseline.exportPath) && filesIn(previous) == before &&
                    failed.errorMessage.find(previous.u8string()) != std::string::npos,
                    "Failed rollback lost the recoverable prior package or its reported path");
            requireNoExportCanaries(previous);
            // Simulate the next invocation with ordinary system operations.
            // Without overwrite consent, restart must restore exact old bytes.
            auto restart = request; restart.overwriteExisting = false;
            const auto recovered = exportCampaignPackage(campaign, restart);
            require(!recovered.success && filesIn(baseline.exportPath) == before && !fs::exists(previous),
                    "Restart failed to restore the retained prior package");
        }
        const auto retry = exportCampaignPackage(campaign, request); inspectExport(retry, campaign);
        requireNoExportStaging(retry.exportPath); requireNoExportCanaries(retry.exportPath);
    }
}
