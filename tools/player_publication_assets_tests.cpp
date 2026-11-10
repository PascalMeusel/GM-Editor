#include "Models/Session/PlayerDisplayPublicationAssets.hpp"
#include "Models/Session/PlayerDisplayReviewService.hpp"
#include "Models/Session/PlayerDisplayReviewConfirmation.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "frozen_image_decode_test_support.hpp"
#include "frozen_image_decoder_parity_cases.hpp"

#include <cstdlib>
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <thread>

namespace fs = std::filesystem;
namespace
{
void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}
void write(const fs::path &path, const std::string &data)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << data;
    require(static_cast<bool>(output), "Could not write generated fixture");
}
std::string bytes(const fs::path &path)
{
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "Could not read frozen fixture");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void isolate(const fs::path &root)
{
#if defined(_WIN32)
    require(_putenv_s("TTRPG_DATA_ROOT", root.u8string().c_str()) == 0, "Could not isolate fixture");
#else
    require(setenv("TTRPG_DATA_ROOT", root.u8string().c_str(), 1) == 0, "Could not isolate fixture");
#endif
}
void handoutPublication(const fs::path &root)
{
    Campaign campaign;
    campaign.metadata.id = "frozen_publication";
    campaign.name = "Reviewed campaign title";
    const auto source = root / "Resources/Campaigns/frozen_publication/Assets/Handouts/approved.png";
    write(source, "APPROVED_IMAGE_BYTES");
    HandoutModel handout;
    handout.metadata.id = "approved_handout";
    handout.title = "Reviewed handout";
    handout.filePath = fs::relative(source, root).generic_u8string();
    campaign.handouts.push_back(handout);
    auto &display = campaign.localPlayerDisplay;
    display.mapOnly = false;
    display.showMap = display.showSharedArt = display.showInitiative = display.showVisibleNpcs = false;
    display.showPublicHandouts = true;
    display.publicHandoutIds = {handout.metadata.id};
    auto review = PlayerDisplayReview::build(campaign, "en");
    require(review.eligible && review.playerHandouts.size() == 1 && review.campaignName == campaign.name,
            "Managed handout review was not complete");
    PlayerDisplayPublicationAssets publication;
    require(publication.capture(review), "Could not freeze approved asset");
    const auto &approved = review.playerHandouts.front();
    const auto frozen = publication.path(approved.projectRelativePath, approved.contentHash);
    const auto descriptor = publication.image(approved.projectRelativePath, approved.contentHash);
    const auto descriptorCopy = descriptor;
    require(descriptor.valid() && descriptor.path() == frozen && descriptor.contentHash() == approved.contentHash &&
        descriptor.lifetime().use_count() == 1, "Frozen descriptor did not bind a weak publication lifetime");
    require(!publication.image(approved.projectRelativePath, "wrong-hash").valid(), "Unreviewed descriptor was exposed");
    require(!frozen.empty() && frozen != source && bytes(frozen) == "APPROVED_IMAGE_BYTES",
            "Renderer path did not contain independent approved bytes");
    require(publication.path(approved.projectRelativePath, "wrong-hash").empty(),
            "Unreviewed hash resolved to render bytes");
    const auto cheapKey = PlayerDisplayReview::sourceFingerprint(campaign, "en", false, false);
    const auto fullKey = PlayerDisplayReview::sourceFingerprint(campaign, "en");
    const auto modified = fs::last_write_time(source);
    write(source, "REPLACED_IMAGE_DATA!");
    fs::last_write_time(source, modified);
    require(fs::file_size(source) == fs::file_size(frozen), "Replacement fixture must preserve size");
    // The runtime key intentionally reads metadata, not file content. It stays
    // stable here; the background verifier revokes while the render copy stays safe.
    for (int frame = 0; frame < 40; ++frame)
        require(PlayerDisplayReview::sourceFingerprint(campaign, "en", false, false) == cheapKey,
                "Runtime key unexpectedly consumed replacement bytes");
    require(PlayerDisplayReview::sourceFingerprint(campaign, "en") != fullKey &&
                !PlayerDisplayReview::isCurrent(campaign, review), "Explicit review accepted changed bytes");
    require(bytes(frozen) == "APPROVED_IMAGE_BYTES" && !publication.sourcesCurrent(true),
            "Source replacement reached frozen render bytes");
    bool revoked = !publication.pollSources();
    for (int attempt = 0; !revoked && attempt < 2000; ++attempt)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        revoked = !publication.pollSources();
    }
    require(revoked, "Background verification did not revoke same-metadata replacement");
    require(!descriptor.valid() && !descriptorCopy.valid(), "Revocation left render descriptors alive");
    require(bytes(frozen) == "APPROVED_IMAGE_BYTES", "Background check altered approved render bytes");
    require(!publication.capture(review) && publication.path(approved.projectRelativePath, approved.contentHash).empty(),
            "Stale review captured changed bytes");
    require(!fs::exists(frozen), "Failed capture retained previous publication storage");

    review = PlayerDisplayReview::build(campaign, "en");
    require(publication.capture(review), "New explicit review could not approve replacement");
    const auto fresh = publication.path(review.playerHandouts[0].projectRelativePath, review.playerHandouts[0].contentHash);
    require(bytes(fresh) == "REPLACED_IMAGE_DATA!", "New approval did not freeze the replacement");
    const auto beforeRevocation = PlayerDisplayReview::sourceFingerprint(campaign, "en", false, false);
    display.publicHandoutIds.clear();
    require(PlayerDisplayReview::sourceFingerprint(campaign, "en", false, false) != beforeRevocation &&
                !PlayerDisplayReview::isCurrent(campaign, review), "Revoked selection retained runtime approval");
    fs::remove(source);
    require(!publication.pollSources() && bytes(fresh) == "REPLACED_IMAGE_DATA!",
            "Missing original changed frozen content or escaped immediate revocation");
    publication.clear();
    require(!fs::exists(fresh), "Cleared publication retained its private file");
}
void mapAndBounds(const fs::path &root)
{
    const auto original = root / "Resources/Campaigns/map/Assets/Maps/original.png";
    const auto proxy = root / "Resources/Campaigns/map/Assets/Maps/proxy.png";
    write(original, "ORIGINAL_BYTES");
    write(proxy, "PROXY_BYTES");
    PlayerDisplayReviewSnapshot review;
    review.eligible = review.hasMapPreview = review.playerMapPlan.backgroundVisible = true;
    auto &plan = review.playerMapPlan;
    plan.backgroundInteractiveProjectRelativePath = fs::relative(proxy, root).generic_u8string();
    plan.backgroundInteractiveContentHash = PlayerDisplayReview::handoutContentHash(proxy);
    plan.backgroundExportProjectRelativePath = fs::relative(original, root).generic_u8string();
    plan.backgroundExportContentHash = PlayerDisplayReview::handoutContentHash(original);
    PlayerDisplayPublicationAssets publication;
    require(publication.capture(review), "Reviewed map pair could not be frozen");
    const auto frozenProxy = publication.path(plan.backgroundInteractiveProjectRelativePath, plan.backgroundInteractiveContentHash);
    const auto frozenOriginal = publication.path(plan.backgroundExportProjectRelativePath, plan.backgroundExportContentHash);
    auto previousDescriptor = publication.image(plan.backgroundInteractiveProjectRelativePath, plan.backgroundInteractiveContentHash);
    require(publication.capture(review) && !previousDescriptor.valid(), "Re-review retained previous generation lifetime");
    auto currentDescriptor = publication.image(plan.backgroundInteractiveProjectRelativePath, plan.backgroundInteractiveContentHash);
    require(currentDescriptor.valid(), "Re-review did not create a current descriptor");
    // Paths change at each capture; retain the current pair for byte checks.
    const auto currentProxy = publication.path(plan.backgroundInteractiveProjectRelativePath, plan.backgroundInteractiveContentHash);
    const auto currentOriginal = publication.path(plan.backgroundExportProjectRelativePath, plan.backgroundExportContentHash);
    FrozenImageAsset destroyedOwner;
    {
        PlayerDisplayPublicationAssets temporary;
        require(temporary.capture(review), "Temporary publication capture failed");
        destroyedOwner = temporary.image(plan.backgroundInteractiveProjectRelativePath, plan.backgroundInteractiveContentHash);
        require(destroyedOwner.valid() && destroyedOwner.lifetime().use_count() == 1, "Descriptor owns temporary publication");
        auto invalid = review; invalid.eligible = false;
        require(!temporary.capture(invalid) && !destroyedOwner.valid(), "Failed capture retained previous render lifetime");
        require(temporary.capture(review), "Capture after failure did not recover");
        destroyedOwner = temporary.image(plan.backgroundInteractiveProjectRelativePath, plan.backgroundInteractiveContentHash);
        temporary.clear();
        require(!destroyedOwner.valid(), "Explicit clear retained render lifetime");
        require(temporary.capture(review), "Capture after clear did not recover");
        destroyedOwner = temporary.image(plan.backgroundInteractiveProjectRelativePath, plan.backgroundInteractiveContentHash);
    }
    require(!destroyedOwner.valid(), "Descriptor copy prolonged destroyed publication");
    require(bytes(currentProxy) == "PROXY_BYTES" && bytes(currentOriginal) == "ORIGINAL_BYTES",
            "Map snapshot omitted original or working proxy");
    write(original, "CHANGED_ORIGINAL");
    require(!publication.pollSources() && bytes(currentOriginal) == "ORIGINAL_BYTES" && !currentDescriptor.valid(),
            "Original map change was not isolated from rendering");
    publication.clear();
    require(!fs::exists(frozenOriginal) && !fs::exists(frozenProxy) && !fs::exists(currentOriginal) &&
        !fs::exists(currentProxy), "Map snapshot cleanup failed");

    const auto oversized = root / "Resources/oversized.png";
    write(oversized, "");
    fs::resize_file(oversized, 256ull * 1024 * 1024 + 1);
    plan.backgroundInteractiveProjectRelativePath = fs::relative(oversized, root).generic_u8string();
    plan.backgroundInteractiveContentHash = "fnv1a64:0000000000000001";
    require(!publication.capture(review), "Oversized publication was accepted");
    fs::remove(oversized);
    plan.backgroundInteractiveProjectRelativePath = "../outside.png";
    require(!publication.capture(review), "Traversal entered publication storage");
    const auto outside = root.parent_path() / (root.filename().string() + "-outside.png");
    write(outside, "OUTSIDE");
    const auto link = root / "Resources/outside-link.png";
    std::error_code error;
    fs::create_symlink(outside, link, error);
    if (!error)
    {
        plan.backgroundInteractiveProjectRelativePath = "Resources/outside-link.png";
        plan.backgroundInteractiveContentHash = PlayerDisplayReview::handoutContentHash(outside);
        require(!publication.capture(review), "Symlink escape entered publication storage");
    }
    else std::cout<<"SKIP: Player publication symlink fixture unavailable: "<<error.message()<<'\n';
    fs::remove(outside);
}
#include "player_confirmation_asset_cases.hpp"
#include "frozen_image_decode_worker_cases.hpp"
}
int main()
{
    const auto root = fs::temp_directory_path() / ("gm-player-assets-test-" + AtomicProjectFile::uniqueSuffix());
    try
    {
        fs::create_directory(root);
        isolate(root);
        handoutPublication(root);
        mapAndBounds(root);
        compactConfirmationAssets(root);
        frozenWorkerCreditAndQueue();frozenWorkerCancellationAndStorage();frozenWorkerValidationAndParity();
        frozen_decode_test::jpegAndAllocationParity();frozen_decode_test::exactBytesAndEncodedBound();
        fs::remove_all(root);
        std::cout << "Player publication immutable assets checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\nGenerated fixtures: " << root.u8string() << '\n';
        return 1;
    }
}
