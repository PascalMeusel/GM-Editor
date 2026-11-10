#include "Controllers/Editor/Widgets/ItemCreator/ItemCreatorController.hpp"
#include "Controllers/Editor/Widgets/AudioManager/AudioManagerController.hpp"
#include "Models/Character/PortraitRecipe.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectAssetCollectionService.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectPackageExportService.hpp"
#include "Models/Project/ProjectPackageImportService.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "fixtures/DeterministicSessionFixture.hpp"
#include "workflow_test_database.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool b, const char *message)
{
    if (!b)
        throw std::runtime_error(message);
}
Campaign read(const EntityId &id)
{
    for (auto c : CampaignRepository::loadAllCampaigns())
        if (c.metadata.id == id)
            return c;
    throw std::runtime_error("Campaign disappeared");
}
void bitmap(const std::filesystem::path &path)
{
    unsigned char bytes[58] = {};
    bytes[0] = 'B';
    bytes[1] = 'M';
    bytes[2] = 58;
    bytes[10] = 54;
    bytes[14] = 40;
    bytes[18] = 1;
    bytes[22] = 1;
    bytes[26] = 1;
    bytes[28] = 24;
    bytes[34] = 4;
    bytes[54] = 143;
    bytes[55] = 187;
    bytes[56] = 206;
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<char *>(bytes), sizeof(bytes));
}

void verifyIncludedAudioUsesRuntimeResources(Campaign &campaign)
{
    const auto previousDirectory = std::filesystem::current_path();
    const auto runtime = ApplicationPaths::databasePath().parent_path() / "audio-runtime";
    const auto audioDirectory = runtime / "Resources" / "Audio";
    std::filesystem::create_directories(audioDirectory);
    {
        std::ofstream catalog(audioDirectory / "catalog.psv");
        catalog << "river|Included river|Ambience|River|Resources/Audio/river.wav|0.3|1|Fixture|QA|CC0|https://example.invalid\n";
        std::ofstream sound(audioDirectory / "river.wav", std::ios::binary);
        sound << "Generated audio import fixture";
    }
    std::filesystem::current_path(runtime);
    try
    {
        AudioManagerController audio(campaign, std::make_shared<AudioPlaybackService>());
        audio.configure();
        const auto previousCount = campaign.audioAssets.size();
        audio.addCatalogAsset(0);
        require(campaign.audioAssets.size() == previousCount + 1,
                "Included audio was searched for in writable campaign storage instead of runtime resources");
        const auto &asset = campaign.audioAssets.back();
        require(asset.name == "Included river" &&
                    asset.originalSourcePath == "Resources/Audio/river.wav" &&
                    std::filesystem::is_regular_file(ProjectAssets::resolveAudioPath(asset)) &&
                    ProjectAssets::resolveAudioPath(asset) != audioDirectory / "river.wav",
                "Included audio was not copied into independent campaign storage");
        require(audio.catalogAssetAlreadyAdded(0),
                "A bundled catalog entry already added to the campaign was not recognized");
        audio.addCatalogAsset(0);
        require(campaign.audioAssets.size() == previousCount + 1,
                "Adding the same included sound twice created a duplicate");
        require(read(campaign.metadata.id).audioAssets.size() == campaign.audioAssets.size(),
                "Included audio did not survive persistence");
    }
    catch (...) { std::filesystem::current_path(previousDirectory); throw; }
    std::filesystem::current_path(previousDirectory);
}
} // namespace
int main()
{
    try
    {
        isolateWorkflowDatabase();
        auto &save = ProjectSaveService::getInstance();
        save.setBackupSnapshotsEnabled(false);
        save.setRecoverySnapshotsEnabled(false);
        auto campaign = gm::qa::threeSceneCampaign();
        gm::portraits::Recipe r;
        // The first community bundle omits this legacy portrait. Its saved ID
        // must still survive persistence and transfers without being remapped.
        r.species = 8;
        r.expression = 2;
        r.features = 3;
        r.seed = 9127;
        gm::portraits::write(campaign.npcs.front().metadata, r);
        gm::portraits::write(campaign.characters.front().metadata, r);
        // Simulate an existing campaign created before collection IDs existed.
        campaign.characters.front().metadata.customProperties.erase("portrait.collection");
        require(save.saveCampaign(campaign), "Initial campaign save failed");
        verifyIncludedAudioUsesRuntimeResources(campaign);
        const auto loaded = read(campaign.metadata.id);
        require(gm::portraits::read(loaded.npcs.front().metadata).species == 8 &&
                    gm::portraits::read(loaded.characters.front().metadata).expression == 2 &&
                    gm::portraits::read(loaded.npcs.front().metadata).collection == gm::portraits::currentCollection &&
                    gm::portraits::read(loaded.characters.front().metadata).collection == 0,
                "Portrait identity/collection did not survive SQLite restart");
        EntityMetadata malformed;
        malformed.customProperties["portrait.species"] = "99999999999999999999999";
        require(gm::portraits::read(malformed).species == 0, "Malformed portrait recipe was not recoverable");
        auto a = r, b = r;
        gm::portraits::vary(a);
        gm::portraits::vary(b);
        require(a.seed == b.seed && a.hair == b.hair && a.seed != r.seed && a.species == r.species &&
                    a.expression == r.expression,
                "Portrait variation changed chosen species/expression or lost determinism");
        const auto image = ApplicationPaths::databasePath().parent_path() / "item-image.bmp";
        bitmap(image);
        ItemCreatorController item(campaign);
        item.configure();
        item.getModel().name = "Illustrated lantern";
        item.getModel().description = gm::qa::secretMarker;
        item.markDraftEdited();
        require(item.attachImage(image), "Valid image could not be attached");
        const auto itemId = item.getModel().metadata.id;
        const auto attached = item.getModel().metadata.customProperties.at("item.image");
        require(std::filesystem::exists(ProjectAssets::resolveProjectPath(attached)),
                "Image not copied into managed campaign storage");
        require(!item.attachImage(image.parent_path() / "missing.png") &&
                    item.getModel().metadata.customProperties.at("item.image") == attached,
                "Failed attachment destroyed the previous image");
        require(item.saveItem(), "Item image save failed");
        const auto before = campaign.handouts.size();
        SQLite::Database control(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
        control.exec("CREATE TRIGGER reject_media_handout BEFORE INSERT ON handouts BEGIN SELECT RAISE(ABORT,'injected "
                     "handout failure'); END;");
        require(!item.prepareImageHandout() && campaign.handouts.size() == before &&
                    read(campaign.metadata.id).handouts.size() == before,
                "Failed handout preparation modified committed state");
        control.exec("DROP TRIGGER reject_media_handout;");
        require(item.prepareImageHandout(), "Handout retry failed");
        const auto after = read(campaign.metadata.id);
        require(after.handouts.size() == before + 1 && after.handouts.back().body.empty() &&
                    after.handouts.back().gmNotes.empty() && after.handouts.back().filePath == attached,
                "Item handout leaked private description or lost its image");
        require(std::find(after.localPlayerDisplay.publicHandoutIds.begin(),
                          after.localPlayerDisplay.publicHandoutIds.end(),
                          after.handouts.back().metadata.id) == after.localPlayerDisplay.publicHandoutIds.end(),
                "Preparing an image published it without review");
        const auto found = std::find_if(after.customItems.begin(), after.customItems.end(),
                                        [&](const auto &candidate) { return candidate.metadata.id == itemId; });
        require(found != after.customItems.end() && found->metadata.customProperties.at("item.image") == attached,
                "Item image reference lost on restart");
        auto collectable = after;
        const auto collection = ProjectAssetCollection::collectAssetsIntoCampaignFolder(collectable);
        require(collection.errors.empty(), "Managed item media could not be collected for transfer");
        ProjectPackageExport::ExportRequest exportRequest;
        exportRequest.destinationDirectory = image.parent_path() / "media-transfer";
        const auto exported = ProjectPackageExport::exportCampaignPackage(after, exportRequest);
        require(exported.success, "Media package export failed");
        ProjectPackageImport::ImportRequest importRequest;
        importRequest.packagePath = exported.exportPath;
        importRequest.conflictResolution = ProjectPackageImport::ConflictResolution::CloneAsCopy;
        importRequest.cloneCampaignName = "Independent illustrated campaign";
        const auto imported = ProjectPackageImport::importCampaignPackage(importRequest);
        require(imported.success && imported.importedAsCopy, "Media package clone failed");
        const auto cloned = read(imported.manifest.campaignId);
        require(gm::portraits::read(cloned.npcs.front().metadata).collection == gm::portraits::currentCollection &&
                    gm::portraits::read(cloned.characters.front().metadata).collection == 0 &&
                    gm::portraits::read(cloned.npcs.front().metadata).seed == r.seed &&
                    gm::portraits::read(cloned.characters.front().metadata).species == r.species,
                "Campaign transfer changed current or legacy portrait identity");
        const auto clonedItem =
            std::find_if(cloned.customItems.begin(), cloned.customItems.end(),
                         [](const auto &candidate) { return candidate.name == "Illustrated lantern"; });
        require(clonedItem != cloned.customItems.end(), "Clone lost illustrated item");
        const auto clonedImage = clonedItem->metadata.customProperties.at("item.image");
        require(clonedImage != attached && std::filesystem::exists(ProjectAssets::resolveProjectPath(clonedImage)),
                "Clone still uses original item image");
        const auto originalPackage = ProjectPackageLayout::campaignPackageDirectory(after);
        const auto hiddenOriginal =
            originalPackage.parent_path() / (originalPackage.filename().string() + "-unavailable");
        std::filesystem::rename(originalPackage, hiddenOriginal);
        require(std::filesystem::exists(ProjectAssets::resolveProjectPath(clonedImage)) &&
                    cloned.handouts.back().filePath == clonedImage &&
                    std::filesystem::exists(ProjectAssets::resolveProjectPath(cloned.handouts.back().filePath)),
                "Cloned media depends on original campaign files");
        std::filesystem::rename(hiddenOriginal, originalPackage);
        std::cout << "Portrait recipes and item images survive restart; failed handout save retries without "
                     "publication or GM-secret leakage.\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
