#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectAssetImportService.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectManifestService.hpp"
#include "Models/Project/ProjectPackageExportService.hpp"
#include "Models/Project/ProjectPackageExportOperations.hpp"
#include "Models/Project/ProjectPackageImportService.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/WorldDraftRecovery.hpp"
#include "Models/Project/EditorDraftRecovery.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "Models/Rulesets/DefinitionBackedRuleSet.hpp"

#include <SQLiteCpp/SQLiteCpp.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
namespace fs = std::filesystem;
using FileTree = std::map<std::string, std::string>;
const std::string otherSecret = "EXPORT_PRIVATE_OTHER_CAMPAIGN_487236";
const std::string excludedSecret = "EXPORT_PRIVATE_NONPORTABLE_FILE_963852";
const std::string handoutBytes = "Selected campaign handout bytes 159753";
const std::string genericBytes = "Selected campaign generic attachment bytes 951357";

void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

void setEnvironment(const char *name, const std::string &value)
{
#if defined(_WIN32)
    require(_putenv_s(name, value.c_str()) == 0, "Could not isolate test environment");
#else
    require(setenv(name, value.c_str(), 1) == 0, "Could not isolate test environment");
#endif
}

std::string readFile(const fs::path &path)
{
    std::ifstream file(path, std::ios::binary);
    require(file.good(), "Cannot read generated fixture: " + path.string());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void writeFile(const fs::path &path, const std::string &contents)
{
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << contents;
    require(file.good(), "Cannot write generated fixture: " + path.string());
}

FileTree filesIn(const fs::path &root)
{
    FileTree result;
    for (const auto &entry : fs::recursive_directory_iterator(root))
        if (entry.is_regular_file())
            result.emplace(fs::relative(entry.path(), root).generic_string(), readFile(entry.path()));
    return result;
}

#include "campaign_export_world_recovery.hpp"

Campaign makeCampaign(const std::string &suffix)
{
    Campaign campaign;
    campaign.metadata.id = "export_fixture_" + suffix;
    campaign.name = "Generated export fixture " + suffix;
    campaign.description = suffix == "other" ? otherSecret : "Selected campaign description";
    campaign.ruleSet = std::make_shared<DND5e>();
    CharacterModel character;
    character.metadata.id = "export_character_" + suffix;
    std::snprintf(character.nameBuffer, sizeof(character.nameBuffer), "Hero %s", suffix.c_str());
    character.gmNotes = suffix == "other" ? otherSecret : "Selected GM-only character notes";
    campaign.characters.push_back(character);
    NPCModel npc;
    npc.metadata.id = "export_npc_" + suffix;
    npc.name = "Generated NPC " + suffix;
    npc.secrets = suffix == "other" ? otherSecret : "Selected GM-only NPC notes";
    campaign.npcs.push_back(npc);
    return campaign;
}

void installReferencedRules(Campaign &selected)
{
    RulesetDefinitionModel base;
    base.metadata.id = "export-base@1.0.0";
    base.rulesetId = "export-base";
    base.name = "Selected export rules";
    base.version = "1.0.0";
    base.published = true;
    base.installedGlobally = true;
    base.abilityScores.push_back({"strength", "Strength", "STR", "Fixture ability"});
    RulesetDefinitionModel overlay;
    overlay.metadata.id = "export-overlay@1.0.0";
    overlay.rulesetId = "export-overlay";
    overlay.name = "Selected export overlay";
    overlay.version = "1.0.0";
    overlay.baseRulesetId = base.rulesetId;
    overlay.published = true;
    overlay.installedGlobally = true;
    auto unusedVersion = base;
    unusedVersion.metadata.id = "export-base@2.0.0";
    unusedVersion.version = "2.0.0";
    unusedVersion.name = otherSecret;
    CampaignRepository::saveInstalledRulesetDefinition(base);
    CampaignRepository::saveInstalledRulesetDefinition(overlay);
    CampaignRepository::saveInstalledRulesetDefinition(unusedVersion);
    selected.rulesetReference = {base.rulesetId, base.version, overlay.rulesetId, overlay.version};
    selected.ruleSet = std::make_shared<DefinitionBackedRuleSet>(base, overlay);
    require(selected.rulesetDefinitions.empty(), "Fixture must resolve pins from the installed library");
}

void inspectExport(const ProjectPackageExport::ExportResult &result, const Campaign &selected)
{
    require(result.success, "Campaign export failed: " + result.errorMessage);
    SQLite::Database database(result.databasePath.string(), SQLite::OPEN_READONLY);
    require(database.execAndGet("SELECT COUNT(*) FROM campaigns;").getInt() == 1,
            "Single-campaign export contains another campaign");
    require(database.execAndGet("SELECT stable_id FROM campaigns;").getString() == selected.metadata.id,
            "Export changed selected campaign identity");
    require(database.execAndGet("SELECT description FROM campaigns;").getString() == selected.description,
            "Export did not contain the selected campaign's current saved content");
    require(database.execAndGet("SELECT COUNT(*) FROM characters;").getInt() == 1 &&
                database.execAndGet("SELECT gm_notes FROM characters;").getString() == selected.characters[0].gmNotes,
            "Selected character content did not round trip in the portable database");
    require(database.execAndGet("SELECT COUNT(*) FROM npcs;").getInt() == 1 &&
                database.execAndGet("SELECT secrets FROM npcs;").getString() == selected.npcs[0].secrets,
            "Selected NPC content did not round trip in the portable database");
    require(database.execAndGet("SELECT COUNT(*) FROM world_places;").getInt() == 1 &&
                database.execAndGet("SELECT notes FROM world_places WHERE stable_id='export_world_recovery_place';").getString() ==
                    "Canonical place note",
            "Export lost or replaced canonical World content while excluding private recovery drafts");
    require(database.execAndGet("SELECT COUNT(*) FROM ruleset_definitions;").getInt() == 2 &&
                database.execAndGet("SELECT COUNT(*) FROM ruleset_definitions WHERE stable_id IN "
                                    "('export-base@1.0.0', 'export-overlay@1.0.0');").getInt() == 2,
            "Export omitted an exact base/overlay pin or included an unrelated installed version");
    bool handoutPresent = false, genericPresent = false;
    const FileTree exported = filesIn(result.exportPath);
    for (const auto &[name, contents] : exported)
    {
        // Raw bytes catch residual SQLite pages as well as nested backups and files.
        require(contents.find(otherSecret) == std::string::npos,
                "Other campaign private canary escaped in " + name);
        require(contents.find(excludedSecret) == std::string::npos,
                "A backup, recovery file or unknown package file escaped in " + name);
        require(contents.find(worldDraftSecret) == std::string::npos,
                "Uncommitted World recovery text escaped in " + name);
        require(contents.find(editorDraftSecret)==std::string::npos,
                "Uncommitted item/notes recovery escaped in "+name);
        handoutPresent |= contents == handoutBytes;
        genericPresent |= contents == genericBytes;
    }
    require(handoutPresent, "Referenced handout bytes were not exported");
    require(genericPresent, "Supported generic attachment was lost without a model reference");
    require(result.fileCount == exported.size() && result.byteCount > 0,
            "Export file accounting does not describe the published package");
    std::string error;
    const auto manifest = ProjectManifest::loadManifest(result.manifestPath, error);
    require(error.empty() && manifest.campaignId == selected.metadata.id,
            "Portable manifest does not identify the selected campaign: " + error);
}

void expectPreservedFailure(const Campaign &campaign, const ProjectPackageExport::ExportRequest &request,
                            const fs::path &existing, const FileTree &before, const std::string &operation)
{
    const auto failed = ProjectPackageExport::exportCampaignPackage(campaign, request);
    require(!failed.success && !failed.errorMessage.empty(), operation + " unexpectedly succeeded");
    require(filesIn(existing) == before, operation + " changed the previous usable export");
}

void testRestartRecovery(const Campaign &campaign, const ProjectPackageExport::ExportRequest &request,
                         const fs::path &destination, const FileTree &before)
{
    auto previous = destination;
    previous += ".previous";
    // Simulate interruption after moving the old package aside, before publication.
    fs::rename(destination, previous);
    expectPreservedFailure(campaign, request, destination, before, "Recovered export without overwrite consent");
    require(!fs::exists(previous), "Restart recovery left the displaced package behind");

    // Simulate interruption after publication but before deleting the old package.
    fs::copy(destination, previous, fs::copy_options::recursive);
    expectPreservedFailure(campaign, request, destination, before, "Completed publication cleanup");
    require(!fs::exists(previous), "Restart recovery did not clean a validated previous package");

    const auto assertRetired = [&](const FileTree &residue, const std::string &operation) {
        expectPreservedFailure(campaign, request, destination, before, operation);
        require(!fs::exists(previous), operation + " left cleanup residue blocking the reserved previous path");
        const auto prefix = previous.filename().string() + ".retired_";
        bool retained = false;
        for (const auto &entry : fs::directory_iterator(previous.parent_path()))
            if (entry.is_directory() && entry.path().filename().string().find(prefix) == 0 && filesIn(entry.path()) == residue)
                retained = true;
        require(retained, operation + " discarded unrecognized residue instead of retaining it intact");
    };
    // Interrupted cleanup can remove either identity file before the process dies.
    for (const auto *missing : {"project.json", "campaign.db"})
    {
        fs::copy(destination, previous, fs::copy_options::recursive);
        fs::remove(previous / missing);
        assertRetired(filesIn(previous), std::string("Partial cleanup missing ") + missing);
    }
    fs::copy(destination, previous, fs::copy_options::recursive);
    writeFile(previous / "campaign.db", "corrupt previous snapshot");
    assertRetired(filesIn(previous), "Corrupt previous snapshot beside valid current package");

    // A reserved name alone never authorizes deletion of arbitrary owner files.
    writeFile(previous / "owner-file.txt", excludedSecret);
    const FileTree unknownBefore = filesIn(previous);
    assertRetired(unknownBefore, "Unknown previous package");

    // A corrupt current package must not make us retire a valid recovery copy.
    fs::copy(destination, previous, fs::copy_options::recursive);
    writeFile(destination / "campaign.db", "corrupt current snapshot");
    const auto corruptCurrent = filesIn(destination);
    expectPreservedFailure(campaign, request, destination, corruptCurrent, "Corrupt current package");
    require(filesIn(previous) == before, "Corrupt current package lost its valid previous recovery copy");
    writeFile(destination / "campaign.db", before.at("campaign.db"));
    expectPreservedFailure(campaign, request, destination, before, "Retry after current fixture restoration");
    require(!fs::exists(previous), "Retry did not resume cleanup after current fixture restoration");

    // With no current package, invalid previous data is retained and never restored.
    auto saved = destination; saved += ".test-held";
    fs::rename(destination, saved);
    writeFile(previous / "owner-file.txt", excludedSecret);
    const auto absentCurrent = ProjectPackageExport::exportCampaignPackage(campaign, request);
    require(!absentCurrent.success && !fs::exists(destination) && filesIn(previous) == unknownBefore && filesIn(saved) == before,
        "Missing current package accepted or discarded invalid previous data");
    fs::remove(previous / "owner-file.txt");
    fs::remove(previous);
    fs::rename(saved, destination);
}

#include "campaign_export_failure_tests.hpp"

void testRoundTrip(const ProjectPackageExport::ExportResult &exported, const Campaign &selected)
{
    ProjectPackageImport::ImportRequest request;
    request.packagePath = exported.exportPath;
    request.conflictResolution = ProjectPackageImport::ConflictResolution::CloneAsCopy;
    request.cloneCampaignName = "Generated independent exported copy";
    const auto imported = ProjectPackageImport::importCampaignPackage(request);
    require(imported.success && imported.importedAsCopy,
            "Portable campaign could not be imported as an independent copy: " + imported.errorMessage);
    const auto campaigns = CampaignRepository::loadAllCampaigns();
    const auto copy = std::find_if(campaigns.begin(), campaigns.end(), [&](const Campaign &value) {
        return value.name == request.cloneCampaignName;
    });
    require(copy != campaigns.end() && campaigns.size() == 3 && copy->metadata.id != selected.metadata.id,
            "Import lost an original campaign or failed to remap the copied campaign identity");
    require(copy->characters.size() == 1 && copy->characters[0].gmNotes == selected.characters[0].gmNotes &&
                copy->characters[0].metadata.id != selected.characters[0].metadata.id && copy->handouts.size() == 1,
            "Import lost character/handout content or independent identities");
    require(readFile(ProjectAssets::resolveProjectPath(copy->handouts[0].filePath)) == handoutBytes,
            "Imported handout reference does not resolve to packaged bytes");
    require(copy->rulesetReference.rulesetId == selected.rulesetReference.rulesetId &&
                copy->rulesetReference.version == selected.rulesetReference.version &&
                copy->rulesetReference.overlayId == selected.rulesetReference.overlayId &&
                copy->rulesetReference.overlayVersion == selected.rulesetReference.overlayVersion &&
                std::dynamic_pointer_cast<DefinitionBackedRuleSet>(copy->ruleSet) != nullptr,
            "Imported campaign lost its definition-backed base or overlay pin");
}
}

int main()
{
    try
    {
        // Set overrides before any persistence API is called, including standalone runs.
        const fs::path root = fs::temp_directory_path() / ("gm-campaign-export-" + AtomicProjectFile::uniqueSuffix());
        fs::create_directories(root);
        setEnvironment("TTRPG_DATA_ROOT", root.string());
        setEnvironment("TTRPG_DB_PATH", (root / "TTRPG.db").string());
        setEnvironment("TTRPG_SETTINGS_PATH", (root / "GMEditor.settings").string());
        Campaign selected = makeCampaign("selected"), other = makeCampaign("other");
        installReferencedRules(selected);
        ProjectPackageLayout::ensureCampaignPackageLayout(selected);
        const auto layout = ProjectPackageLayout::layoutForCampaign(selected);
        const fs::path handoutSource = root / "inputs" / "handout.txt";
        writeFile(handoutSource, handoutBytes);
        ProjectAssetImport::AssetImportRequest asset;
        asset.kind = ProjectAssetImport::AssetKind::Handout;
        asset.sourcePath = handoutSource;
        asset.assetId = "export_fixture_handout";
        const auto imported = ProjectAssetImport::importAsset(selected, asset);
        require(imported.success, "Could not create the managed handout fixture: " + imported.errorMessage);
        HandoutModel handout;
        handout.metadata.id = asset.assetId;
        handout.title = "Selected handout";
        handout.filePath = imported.projectPath;
        selected.handouts.push_back(handout);
        seedWorldRecoveryCanary(selected);
        writeFile(layout.assetsDirectory / "Files" / "generic.txt", genericBytes);
        CampaignRepository::saveCampaign(other);
        CampaignRepository::saveCampaign(selected);
        CampaignRepository::writeSnapshot(layout.databasePath.string());
        for (const auto &relative : {"owner-notes.txt", "Recovery/private.db", "Backups/private.db",
                                     "Exports/private.txt", "Assets/Unknown/private.txt"})
            writeFile(layout.rootDirectory / relative, excludedSecret);
        const std::vector<fs::path> nestedExcluded{
            "Files/nested/ReCoVeRy/private.txt", "Images/nested/BACKUPS/private.txt", "Handouts/Exports/private.txt",
            "Files/old.TTRPGPKG/private.txt", "Files/archive.ttrpgpkg.previous/private.txt",
            "Files/archive.ttrpgpkg.staging_123/private.txt", "Files/transfer.previous/private.txt",
            "Files/transfer.staging_123/private.txt", "Files/archive.ttrpgpkg",
            "Files/nested/store.DB", "Files/store.db3", "Files/store.SQLITE", "Files/store.Sqlite3",
            "Files/store.db-WAL", "Files/store.db-SHM", "Images/cache.sqlite-journal",
            "Tokens/token.BAK", "Maps/map.backup", "Portraits/portrait.bkp", "Files/note.old",
            "Files/note.orig", "Files/note.txt~", "Files/renamed.data", "Files/renamed-wal.data",
            "Files/renamed-journal.data"};
        for (const auto &relative : nestedExcluded) writeFile(layout.assetsDirectory / relative, excludedSecret);
        // A renamed real repository snapshot must be rejected by its SQLite
        // header, even though its extension is otherwise a generic attachment.
        fs::copy_file(layout.databasePath, layout.assetsDirectory / "Files/renamed.data", fs::copy_options::overwrite_existing);
        writeFile(layout.assetsDirectory / "Files/renamed-wal.data", std::string("\x37\x7f\x06\x82", 4) + excludedSecret);
        writeFile(layout.assetsDirectory / "Files/renamed-journal.data", std::string("\xd9\xd5\x05\xf9\x20\xa1\x63\xd7", 8) + excludedSecret);
        std::string manifestError;
        require(ProjectManifest::writeManifest(selected, {}, manifestError), "Fixture manifest failed: " + manifestError);
        const FileTree sourceBefore = filesIn(layout.rootDirectory);
        const std::string repositoryBefore = readFile(ApplicationPaths::databasePath());

        ProjectPackageExport::ExportRequest request;
        request.destinationDirectory = root / "portable";
        auto exported = ProjectPackageExport::exportCampaignPackage(selected, request);
        inspectExport(exported, selected);
        require(readFile(ApplicationPaths::databasePath()) == repositoryBefore && filesIn(layout.rootDirectory) == sourceBefore,
                "Export changed the live database or original campaign package");
        const FileTree firstExport = filesIn(exported.exportPath);
        expectPreservedFailure(selected, request, exported.exportPath, firstExport, "Unapproved overwrite");
        testRestartRecovery(selected, request, exported.exportPath, firstExport);
        testInjectedFilesystemFailures(selected, request.destinationDirectory / "injected-failures");
        require(readFile(ApplicationPaths::databasePath()) == repositoryBefore && filesIn(layout.rootDirectory) == sourceBefore,
                "Injected export failures changed the live repository or source package");

        request.overwriteExisting = true;
        Campaign missingAsset = selected;
        missingAsset.handouts[0].filePath = (layout.assetsDirectory / "Handouts" / "missing.txt").string();
        expectPreservedFailure(missingAsset, request, exported.exportPath, firstExport, "Missing referenced handout");
        Campaign excludedReference = selected;
        excludedReference.handouts[0].filePath = (layout.recoveryDirectory / "private.db").string();
        expectPreservedFailure(excludedReference, request, exported.exportPath, firstExport, "Reference into excluded recovery storage");
        for (const auto &relative : nestedExcluded)
        {
            excludedReference.handouts[0].filePath = (layout.assetsDirectory / relative).string();
            const auto rejected = ProjectPackageExport::exportCampaignPackage(excludedReference, request);
            require(!rejected.success && rejected.errorMessage.find("excluded") != std::string::npos,
                    "Referenced nested backup/database was not explicitly refused: " + relative.generic_string());
            require(filesIn(exported.exportPath) == firstExport,
                    "Rejected nested backup/database reference changed the previous export");
        }
        require(filesIn(layout.rootDirectory) == sourceBefore,
                "Nested asset inventory filtering changed source files");
        const auto otherAsset = ProjectPackageLayout::layoutForCampaign(other).assetsDirectory / "Handouts/private.txt";
        writeFile(otherAsset, otherSecret);
        excludedReference.handouts[0].filePath = otherAsset.string();
        expectPreservedFailure(excludedReference, request, exported.exportPath, firstExport, "Reference into another campaign");
        Campaign conflictingPin = selected;
        RulesetDefinitionModel collision; collision.metadata.id = "export-base@1.0.0";
        collision.rulesetId = "export-base"; collision.version = "unrelated-version";
        conflictingPin.rulesetDefinitions.push_back(collision);
        expectPreservedFailure(conflictingPin, request, exported.exportPath, firstExport, "Conflicting required ruleset identity");

        for (const auto &destination : {layout.rootDirectory, layout.assetsDirectory, layout.exportsDirectory})
        {
            auto overlap = request;
            overlap.destinationDirectory = destination;
            const auto rejected = ProjectPackageExport::exportCampaignPackage(selected, overlap);
            require(!rejected.success && !rejected.errorMessage.empty(), "Destination inside source package was accepted");
            require(filesIn(layout.rootDirectory) == sourceBefore, "Rejected overlap changed the source package");
        }

        const auto external = root / "external-private.txt";
        const auto linked = layout.assetsDirectory / "Files" / "linked.txt";
        writeFile(external, otherSecret);
        std::error_code linkError;
        fs::create_symlink(external, linked, linkError);
        if (!linkError)
        {
            expectPreservedFailure(selected, request, exported.exportPath, firstExport, "Linked asset");
            fs::remove(linked);
            require(readFile(external) == otherSecret, "Linked asset rejection modified its target");
        }
        else std::cout << "Symbolic-link fixture skipped: host cannot create symbolic links.\n";

#if defined(_WIN32)
        // A real directory-sharing failure exercises publication rollback, not a mocked return value.
        const HANDLE held = CreateFileW(exported.exportPath.c_str(), FILE_LIST_DIRECTORY,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                       FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        require(held != INVALID_HANDLE_VALUE, "Could not lock generated export directory");
        const auto failedPublish = ProjectPackageExport::exportCampaignPackage(selected, request);
        CloseHandle(held);
        require(!failedPublish.success && !failedPublish.errorMessage.empty(), "Locked publication was reported successful");
        require(filesIn(exported.exportPath) == firstExport, "Failed publication damaged the previous usable export");
#endif

        selected.description = "Selected campaign replacement content";
        CampaignRepository::saveCampaign(selected);
        exported = ProjectPackageExport::exportCampaignPackage(selected, request);
        inspectExport(exported, selected);
        require(filesIn(layout.rootDirectory) == sourceBefore, "Replacement export changed source campaign files");
        testRoundTrip(exported, selected);
        std::cout << "Campaign export isolation, portable assets, replacement safety and clone round trip passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
