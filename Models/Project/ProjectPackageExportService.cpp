#include "ProjectPackageExportService.hpp"
#include "ProjectPackageExportOperations.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectManifestService.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Project/ProjectDirectoryPublish.hpp"
#include "ProjectPackageExportPaths.hpp"
#include "SQLiteCpp/SQLiteCpp.h"
#include <stdexcept>

namespace
{
void validatePackage(const std::filesystem::path &directory, const Campaign &campaign, bool isolated = true)
{
    using namespace ProjectPackageExportPaths;
    rejectLinks(directory / "project.json");
    rejectLinks(directory / "campaign.db");
    std::string error;
    const auto manifest = ProjectManifest::loadManifest(directory / "project.json", error);
    if (!error.empty() || manifest.campaignId != campaign.metadata.id || manifest.databasePath != "campaign.db")
        throw std::runtime_error("The export destination is not a package for this campaign: " + error);
    SQLite::Database database(mapFileIoPath(directory / "campaign.db").u8string(), SQLite::OPEN_READONLY);
    if (database.execAndGet("PRAGMA integrity_check;").getString() != "ok" ||
        (isolated && database.execAndGet("SELECT COUNT(*) FROM campaigns;").getInt() != 1))
        throw std::runtime_error("Campaign export failed integrity or isolation validation.");
    SQLite::Statement row(database, "SELECT name FROM campaigns WHERE stable_id = ?;");
    row.bind(1, campaign.metadata.id);
    if (!row.executeStep()) throw std::runtime_error("Campaign export is missing the selected campaign.");
}

void recoverPrevious(const std::filesystem::path &destination,
                     const std::filesystem::path &previous, const Campaign &campaign,
                     const ProjectPackageExport::detail::FileOperations &operations)
{
    if (!std::filesystem::exists(previous)) return;
    if (!std::filesystem::exists(destination))
    {
        validatePackage(previous, campaign, false);
        operations.renameDirectory(previous, destination);
        return;
    }
    // A crash during old-directory cleanup can leave an incomplete .previous.
    // Trust the current isolated package first. Retain an unrecognizable residue
    // intact under a unique sibling so it cannot permanently block new exports.
    validatePackage(destination, campaign);
    try { validatePackage(previous, campaign, false); }
    catch (const std::exception &)
    {
        auto retired = previous;
        retired += ".retired_" + AtomicProjectFile::uniqueSuffix();
        ProjectPackageExportPaths::rejectLinks(retired);
        operations.renameDirectory(previous, retired);
        return;
    }
    std::filesystem::remove_all(previous);
}

void checkManagedReferences(const Campaign &campaign, const std::filesystem::path &source)
{
    const auto check = [&](const std::string &stored) {
        if (stored.empty()) return;
        const auto path = ProjectAssets::resolveProjectPath(stored);
        if (!ProjectPackageExportPaths::inside(path, source))
        {
            if (ProjectPackageExportPaths::inside(path, ProjectPackageLayout::campaignsDirectory()))
                throw std::runtime_error("An asset refers to a different campaign. Import a managed copy before exporting: " + stored);
            return;
        }
        ProjectPackageExportPaths::rejectLinks(path);
        const auto buckets = ProjectPackageLayout::assetFolderNames();
        const auto assets = source / "Assets";
        if (std::none_of(buckets.begin(), buckets.end(), [&](const auto &bucket) {
                return ProjectPackageExportPaths::inside(path, assets / bucket);
            }))
            throw std::runtime_error("A campaign asset is outside its transferable managed buckets: " + stored);
        if (!std::filesystem::is_regular_file(path))
            throw std::runtime_error("A managed campaign asset is missing: " + stored);
        if (!ProjectPackageExportPaths::transferableAsset(path, assets))
            throw std::runtime_error("A referenced managed asset is excluded from transfer because it is backup, export, or SQLite data: " + stored);
    };
    for (const auto &handout : campaign.handouts) check(handout.filePath);
    for (const auto &asset : campaign.audioAssets) check(asset.filePath);
    for (const auto &character : campaign.characters) check(character.portraitAssetId);
    for (const auto &map : campaign.maps)
    {
        check(map.backgroundImagePath);
        check(map.importedReference.projectRelativePath);
        check(map.importedReference.workingProxyProjectRelativePath);
    }
    for (const auto &item : campaign.customItems)
    {
        const auto found = item.metadata.customProperties.find("item.image");
        if (found != item.metadata.customProperties.end()) check(found->second);
    }
}

void copyAssets(const std::filesystem::path &source, const std::filesystem::path &staging,
                const ProjectPackageExport::detail::FileOperations &operations)
{
    using namespace ProjectPackageExportPaths;
    rejectLinks(source / "Assets");
    for (const auto &folder : ProjectPackageLayout::assetFolderNames())
    {
        const auto assets = source / "Assets" / folder;
        rejectLinks(assets);
        std::filesystem::create_directories(staging / "Assets" / folder);
        if (!std::filesystem::exists(assets)) continue;
        for (auto iterator = std::filesystem::recursive_directory_iterator(assets);
             iterator != std::filesystem::recursive_directory_iterator(); ++iterator)
        {
            const auto &entry = *iterator;
            rejectLinks(entry.path());
            if (!inside(entry.path(), assets)) throw std::runtime_error("Asset escapes the campaign folder.");
            if (!transferableAsset(entry.path(), source / "Assets"))
            {
                if (entry.is_directory()) iterator.disable_recursion_pending();
                continue;
            }
            const auto target = staging / entry.path().lexically_relative(source);
            if (entry.is_directory()) std::filesystem::create_directories(target);
            else if (entry.is_regular_file())
            {
                std::filesystem::create_directories(target.parent_path());
                operations.copyFile(entry.path(), target);
                if (!transferableAsset(target, staging / "Assets"))
                    throw std::runtime_error("A managed asset changed to excluded database data while exporting.");
            }
            else throw std::runtime_error("Unsupported campaign asset file type.");
        }
    }
}
}

namespace ProjectPackageExport
{
std::filesystem::path defaultExportDirectory()
{
    return ProjectAssets::findProjectRoot() / "dist" / "CampaignPackages";
}

std::filesystem::path packageExportPath(const Campaign &campaign, const std::filesystem::path &destinationDirectory)
{
    return destinationDirectory / (ProjectAssets::sanitizePathSegment(campaign.metadata.id) + ".ttrpgpkg");
}

ExportResult exportCampaignPackage(const Campaign &campaign, const ExportRequest &request)
{
    return detail::exportWithFileOperations(campaign, request, detail::systemFileOperations());
}

namespace detail
{
FileOperations systemFileOperations()
{
    return {
        [](const auto &source, const auto &destination) { std::filesystem::copy_file(source, destination); },
        [](const auto &source, const auto &destination) { ProjectDirectoryPublish::rename(source, destination); }
    };
}
ExportResult exportWithFileOperations(const Campaign &campaign, const ExportRequest &request, const FileOperations &operations)
{
    ExportResult result;
    std::filesystem::path staging, previous;
    bool displaced = false, stagingCreated = false;
    try
    {
        using namespace ProjectPackageExportPaths;
        if (!operations.copyFile || !operations.renameDirectory)
            throw std::runtime_error("Campaign export filesystem operations are unavailable.");
        if (campaign.metadata.id.empty()) throw std::runtime_error("Campaign export requires a stable identity.");
        const auto source = std::filesystem::absolute(ProjectPackageLayout::layoutForCampaign(campaign).rootDirectory).lexically_normal();
        const auto destination = request.destinationDirectory.empty() ? defaultExportDirectory() : request.destinationDirectory;
        result.exportPath = std::filesystem::absolute(packageExportPath(campaign, destination)).lexically_normal();
        previous = result.exportPath; previous += ".previous";
        if (inside(result.exportPath, source) || inside(source, result.exportPath))
            throw std::runtime_error("The export destination overlaps the managed campaign folder.");
        rejectLinks(result.exportPath);
        rejectLinks(previous);
        recoverPrevious(result.exportPath, previous, campaign, operations);
        if (std::filesystem::exists(result.exportPath))
        {
            if (!request.overwriteExisting) throw std::runtime_error("Export package already exists.");
            validatePackage(result.exportPath, campaign, false);
        }
        checkManagedReferences(campaign, source);
        std::filesystem::create_directories(result.exportPath.parent_path());
        staging = result.exportPath; staging += ".staging_" + AtomicProjectFile::uniqueSuffix();
        if (!std::filesystem::create_directory(staging)) throw std::runtime_error("Export staging already exists.");
        stagingCreated = true;
        copyAssets(source, staging, operations);
        CampaignRepository::writeCampaignSnapshot(campaign, (staging / "campaign.db").u8string());
        AtomicProjectFile::write(staging / "project.json",
            ProjectManifest::serializeManifest(ProjectManifest::buildManifest(campaign)));
        validatePackage(staging, campaign);
        for (const auto &entry : std::filesystem::recursive_directory_iterator(staging))
            if (entry.is_regular_file()) { ++result.fileCount; result.byteCount += entry.file_size(); }
        if (std::filesystem::exists(result.exportPath))
        {
            operations.renameDirectory(result.exportPath, previous);
            displaced = true;
        }
        operations.renameDirectory(staging, result.exportPath);
        result.manifestPath = result.exportPath / "project.json";
        result.databasePath = result.exportPath / "campaign.db";
        result.success = true;
        // A crash between renames retains .previous for recovery on the next
        // export. Cleanup failure after publication does not negate success.
        if (displaced) { std::error_code ignored; std::filesystem::remove_all(previous, ignored); }
    }
    catch (const std::exception &e)
    {
        result.errorMessage = e.what();
        std::error_code existenceError;
        const bool destinationExists = std::filesystem::exists(result.exportPath, existenceError);
        if (displaced && !destinationExists && !existenceError)
        {
            try { operations.renameDirectory(previous, result.exportPath); }
            catch (...) { result.errorMessage += " Previous export remains at: " + previous.u8string(); }
        }
        if (stagingCreated) { std::error_code ignored; std::filesystem::remove_all(staging, ignored); }
    }
    return result;
}
}
}
