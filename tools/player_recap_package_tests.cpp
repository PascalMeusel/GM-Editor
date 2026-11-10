#include "Models/Session/PlayerRecapPackage.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
namespace
{
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
std::string bytes(const fs::path &path) { std::ifstream file(path, std::ios::binary); return {std::istreambuf_iterator<char>(file), {}}; }
void write(const fs::path &path, const std::string &value)
{ fs::create_directories(path.parent_path()); AtomicProjectFile::write(path, value); }
void isolate(const fs::path &path)
{
#ifdef _WIN32
    _putenv_s("TTRPG_DATA_ROOT", path.u8string().c_str());
#else
    setenv("TTRPG_DATA_ROOT", path.u8string().c_str(), 1);
#endif
}
}
int main()
{
    try
    {
        const auto root = fs::current_path() / ("recap-fixture-" + AtomicProjectFile::uniqueSuffix());
        fs::create_directories(root); isolate(root);
        Campaign campaign; campaign.metadata.id = "recap-test"; campaign.name = "PRIVATE_CAMPAIGN_CANARY";
        SessionModel session; session.title = "PRIVATE_SESSION_CANARY"; session.roughNotes = "PRIVATE_NOTES_CANARY";
        campaign.sessions.push_back(session);
        const SessionPlan::Persist persist = [](Campaign &) { return true; };
        SessionOutcomes::Outcome outcome; outcome.privateText = "PRIVATE_OUTCOME_CANARY";
        outcome.publicText = "Bridge destroyed. <script>bad()</script> [x](https://bad.invalid)"; outcome.shareWithPlayers = true;
        require(SessionOutcomes::capture(campaign, session.metadata.id, outcome, persist).success, "Capture failed");
        const auto source = root / "Resources/Campaigns/recap-test/Assets/Handouts/public.png";
        write(source, "REVIEWED_IMAGE_BYTES");
        HandoutModel handout; handout.title = "Public image"; handout.body = "A public map"; handout.gmNotes = "PRIVATE_HANDOUT_CANARY";
        handout.filePath = fs::relative(source, root).generic_u8string(); campaign.handouts.push_back(handout);
        campaign.localPlayerDisplay.publicHandoutIds = {handout.metadata.id};
        PlayerRecapPackage review;
        require(review.review(campaign, session.metadata.id, {handout.metadata.id}), "Review failed");
        const auto firstImage = review.image(review.attachments().front());
        require(firstImage.valid() && firstImage.lifetime().use_count() == 1, "Recap descriptor retained ownership");
        require(review.exportTo(campaign, root / "out"), "Export failed");
        const auto old = review.outputDirectory();
        const auto md = bytes(old / "recap.md"), html = bytes(old / "recap.html");
        require(md.find("PRIVATE_") == std::string::npos && html.find("PRIVATE_") == std::string::npos, "Private canary leaked");
        require(html.find("<script>") == std::string::npos && html.find("&lt;script&gt;") != std::string::npos, "Raw user HTML executed");
        require(bytes(old / "attachment-1.png") == "REVIEWED_IMAGE_BYTES", "Wrong attachment bytes");
        require(!fs::exists(old / "public.png"), "Original private filename leaked");
        write(review.imagePath(review.attachments().front()), "UNREVIEWED_FROZEN_COPY");
        require(!review.exportTo(campaign, root / "tampered-frozen"), "Modified frozen attachment was exported");
        require(bytes(old / "attachment-1.png") == "REVIEWED_IMAGE_BYTES", "Frozen-copy failure modified an earlier export");
        require(review.review(campaign, session.metadata.id, {handout.metadata.id}), "Re-review did not restore frozen bytes");
        require(!firstImage.valid(), "Recap re-review retained old descriptor lifetime");
        const auto nextImage = review.image(review.attachments().front());
        campaign.localPlayerDisplay.publicHandoutIds.clear();
        require(!review.exportTo(campaign, root / "revoked"), "Revoked attachment exported");
        require(!fs::exists(root / "revoked"), "Rejected export created a package");
        campaign.localPlayerDisplay.publicHandoutIds.push_back(handout.metadata.id);
        const auto timestamp = fs::last_write_time(source); write(source, "REPLACED_IMAGE_DATA!"); fs::last_write_time(source, timestamp);
        require(bytes(review.imagePath(review.attachments().front())) == "REVIEWED_IMAGE_BYTES", "Mutable replacement reached preview");
        require(!review.exportTo(campaign, root / "changed"), "Changed asset exported without re-review");
        require(bytes(old / "recap.md") == md && bytes(old / "attachment-1.png") == "REVIEWED_IMAGE_BYTES", "Failure damaged old package");
        require(review.review(campaign, session.metadata.id, {}), "Text-only review failed");
        require(!nextImage.valid(), "Text-only review retained prior image textures");
        require(review.exportTo(campaign, root / "text"), "Text-only export failed");
        require(std::distance(fs::directory_iterator(review.outputDirectory()), fs::directory_iterator{}) == 2, "Unselected files exported");
        require(SessionOutcomes::setPlayerText(campaign, session.metadata.id, outcome.id, outcome.publicText, true, "New text", true, persist).success, "Public text update failed");
        require(!review.current(campaign), "Stale projection retained approval");
        FrozenImageAsset destroyedOwner;
        {
            PlayerRecapPackage temporary;
            require(temporary.review(campaign, session.metadata.id, {handout.metadata.id}), "Temporary recap review failed");
            destroyedOwner = temporary.image(temporary.attachments().front());
            require(destroyedOwner.valid(), "Temporary recap descriptor was unavailable");
        }
        require(!destroyedOwner.valid(), "Recap destruction left descriptor alive");
        require(review.review(campaign, session.metadata.id, {handout.metadata.id}), "Pre-failure recap review failed");
        const auto failedReviewImage = review.image(review.attachments().front());
        auto &stored = campaign.handouts.front(); stored.filePath = "../outside.png";
        require(!review.review(campaign, session.metadata.id, {handout.metadata.id}), "Unmanaged traversal attachment accepted");
        require(!failedReviewImage.valid(), "Failed recap review retained previous descriptor lifetime");
        std::cout << "Player recap package: private projection, escaping, explicit selection, frozen bytes, revocation, failure preservation passed\n";
        return 0;
    }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
