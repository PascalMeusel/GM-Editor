#include "Models/Project/TextDraftRecovery.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Session/SessionOutcomeDraftKeys.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
namespace fs = std::filesystem;
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
void isolate(const char *key, const std::string &value)
{
#ifdef _WIN32
    require(_putenv_s(key, value.c_str()) == 0, "Fixture environment failed");
#else
    require(setenv(key, value.c_str(), 1) == 0, "Fixture environment failed");
#endif
}
std::string bytes(const fs::path &path)
{
    std::ifstream file(path, std::ios::binary);
    return {(std::istreambuf_iterator<char>(file)), {}};
}
fs::path pathFor(const Campaign &campaign, const std::string &key)
{
    return ProjectPackageLayout::layoutForCampaign(campaign).recoveryDirectory / "Drafts" / (key + ".draft");
}
void roundTripAndIsolation(Campaign &campaign)
{
    auto missing = TextDraftRecovery::read(campaign, "session-workspace");
    require(missing.success && !missing.found, "Missing checkpoint should be a clean start");
    const TextDraftRecovery::Fields draft{{"session", "session-fixture"}, {"notes", "Private\nmultiline\n\xC3\xA4"},
        {"empty", ""}, {"binary", std::string("a\0b", 3)}};
    require(TextDraftRecovery::save(campaign, "session-workspace", draft).empty(), "Draft save failed");
    const auto read = TextDraftRecovery::read(campaign, "session-workspace");
    require(read.success && read.found && read.fields == draft, "Length-prefixed draft did not roundtrip");
    require(!read.rawAvailable && read.rawBytes.empty(), "Ordinary read retained unexpected raw bytes");
    const auto raw = TextDraftRecovery::read(campaign, "session-workspace", true);
    require(raw.success && raw.rawAvailable && raw.rawBytes == bytes(pathFor(campaign,"session-workspace")), "Opt-in raw read lost complete bytes");
    require(TextDraftRecovery::save(campaign, "live-dashboard", {{"recap", "Separate view draft"}}).empty(), "Second view checkpoint failed");
    require(TextDraftRecovery::read(campaign, "session-workspace").fields == draft, "View keys collided");
    Campaign other; other.metadata.id = "other-campaign"; other.name = "Other fixture";
    require(!TextDraftRecovery::read(other, "session-workspace").found, "Draft crossed campaign package");
    fs::create_directories(pathFor(other, "session-workspace").parent_path());
    fs::copy_file(pathFor(campaign, "session-workspace"), pathFor(other, "session-workspace"));
    const auto copied = TextDraftRecovery::read(other, "session-workspace");
    require(!copied.success && copied.found && copied.fields.empty(), "Copied foreign draft identity accepted");
    require(TextDraftRecovery::remove(campaign, "live-dashboard").empty() &&
        !TextDraftRecovery::read(campaign, "live-dashboard").found, "Explicit draft discard failed");
}
void malformedAndSizeBound(Campaign &campaign)
{
    const std::string key = "bounded";
    const auto path = pathFor(campaign, key);
    const TextDraftRecovery::Fields valid{{"notes", "Previous usable checkpoint"}};
    require(TextDraftRecovery::save(campaign, key, valid).empty(), "Initial bounded checkpoint failed");
    const auto original = bytes(path);
    require(!TextDraftRecovery::save(campaign, "../escape", valid).empty(), "Unsafe recovery key accepted");
    TextDraftRecovery::Fields tooMany;
    for (int i = 0; i < 129; ++i) tooMany[std::to_string(i)] = "x";
    require(!TextDraftRecovery::save(campaign, key, tooMany).empty() && bytes(path) == original, "Field limit overwrote prior checkpoint");
    require(!TextDraftRecovery::save(campaign, key, {{"x", std::string(8U * 1024U * 1024U, 'x')}}).empty() && bytes(path) == original,
        "Oversize recovery overwrote prior checkpoint");
    // Exercise the prefix overhead boundary: every successful save must be readable.
    constexpr std::size_t limit = 8U * 1024U * 1024U;
    const auto fieldBytes = [](std::size_t size) { return std::to_string(size).size() + 1 + size; };
    const auto header = fieldBytes(std::string("GM_TEXT_DRAFT_1").size()) + fieldBytes(campaign.metadata.id.size()) + fieldBytes(key.size()) + fieldBytes(1);
    require(!TextDraftRecovery::save(campaign, key, {{"x", std::string(limit - header, 'x')}}).empty() && bytes(path) == original,
        "Length prefix overhead produced an unreadable saved checkpoint");
    for (const auto &bad : {std::string{}, std::string("garbage"), original.substr(0, original.size() - 1), original + "1:x1:a1:x1:b"})
    {
        AtomicProjectFile::write(path, bad);
        const auto result = TextDraftRecovery::read(campaign, key);
        require(!result.success && result.found && result.fields.empty() && bytes(path) == bad, "Malformed checkpoint was accepted or modified");
        const auto raw = TextDraftRecovery::read(campaign, key, true);
        require(!raw.success && raw.rawAvailable && raw.rawBytes == bad && raw.fields.empty(), "Malformed complete raw copy was discarded");
    }
    AtomicProjectFile::write(path, std::string(limit + 1, 'x'));
    const auto oversized = TextDraftRecovery::read(campaign,key,true);
    require(!oversized.success && !oversized.rawAvailable && oversized.rawBytes.empty(), "Oversized raw read returned a partial copy");
    AtomicProjectFile::write(path, original);
    require(TextDraftRecovery::read(campaign, key).fields == valid, "Valid checkpoint could not be restored after rejected reads");
}
void independentOutcomeSurfaces(Campaign &campaign)
{
    using SessionOutcomeDraftKeys::Surface;
    const EntityId sessionId = "session-shared-between-prep-and-live";
    const auto prep = SessionOutcomeDraftKeys::composer(Surface::Preparation, sessionId);
    const auto live = SessionOutcomeDraftKeys::composer(Surface::Live, sessionId);
    const auto prepRecap = SessionOutcomeDraftKeys::recap(Surface::Preparation, sessionId);
    const auto liveRecap = SessionOutcomeDraftKeys::recap(Surface::Live, sessionId);
    const TextDraftRecovery::Fields legacy{{"session", sessionId}, {"text", "Existing preparation checkpoint"}};
    require(TextDraftRecovery::save(campaign, "outcome-" + sessionId, legacy).empty(), "Legacy checkpoint fixture failed");
    require(TextDraftRecovery::read(campaign, prep).fields == legacy, "Existing outcome checkpoint became unrecoverable");
    const TextDraftRecovery::Fields liveText{{"session", sessionId}, {"text", "New live-session draft"}};
    const TextDraftRecovery::Fields recapText{{"session", sessionId}, {"text", "New live recap draft"}};
    require(TextDraftRecovery::save(campaign, live, liveText).empty() &&
        TextDraftRecovery::save(campaign, liveRecap, recapText).empty(), "Live checkpoint failed");
    // Simulate the preparation panel being revisited with its older memory.
    require(TextDraftRecovery::save(campaign, prep, {{"text", "Older preparation composer"}}).empty() &&
        TextDraftRecovery::save(campaign, prepRecap, {{"text", "Older preparation recap"}}).empty(), "Preparation flush failed");
    require(TextDraftRecovery::read(campaign, live).fields == liveText &&
        TextDraftRecovery::read(campaign, liveRecap).fields == recapText, "Stale preparation flush overwrote Live recovery");
    require(TextDraftRecovery::remove(campaign, prep).empty() && TextDraftRecovery::remove(campaign, prepRecap).empty(),
        "Preparation discard failed");
    require(TextDraftRecovery::read(campaign, live).fields == liveText &&
        TextDraftRecovery::read(campaign, liveRecap).fields == recapText, "Preparation save/discard deleted Live checkpoints");
    require(!TextDraftRecovery::read(campaign, SessionOutcomeDraftKeys::composer(Surface::Live, "another-session")).found,
        "Live checkpoint crossed session identity");
    require(TextDraftRecovery::remove(campaign, live).empty() && TextDraftRecovery::read(campaign, liveRecap).fields == recapText,
        "Recording a Live outcome deleted its separate recap draft");
}
void failedPublicationAndLinks(Campaign &campaign, const fs::path &root)
{
    const std::string key = "locked";
    require(TextDraftRecovery::save(campaign, key, {{"text", "before"}}).empty(), "Initial locked checkpoint failed");
    const auto path = pathFor(campaign, key), original = pathFor(campaign, "session-workspace");
    const auto previous = bytes(path);
#ifdef _WIN32
    const HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(handle != INVALID_HANDLE_VALUE, "Could not create isolated destination lock");
    const auto saveError = TextDraftRecovery::save(campaign, key, {{"text", "after"}});
    const auto removeError = TextDraftRecovery::remove(campaign,key);
    CloseHandle(handle);
    require(!saveError.empty() && bytes(path) == previous, "Failed replacement destroyed old checkpoint");
    require(!removeError.empty() && bytes(path) == previous, "Locked checkpoint discard reported success or lost bytes");
#endif
    std::error_code error;
    fs::create_symlink(original, pathFor(campaign, "linked"), error);
    if (!error)
    {
        const auto before = bytes(original);
        require(!TextDraftRecovery::read(campaign, "linked").success, "Symlink recovery read accepted");
        require(!TextDraftRecovery::save(campaign, "linked", {{"text", "bad"}}).empty(), "Symlink recovery save accepted");
        require(!TextDraftRecovery::remove(campaign, "linked").empty() && bytes(original) == before, "Symlink discard affected source");
    }
    else std::cout << "SKIP symlink fixture: " << error.message() << '\n';
    require(!fs::exists(root / "escape.draft"), "Invalid key escaped recovery folder");
}
void absentVersusObstructed()
{
    Campaign campaign;campaign.metadata.id="ancestor-recovery-fixture";
    const std::string key="checkpoint";const auto path=pathFor(campaign,key);
    const auto missing=TextDraftRecovery::read(campaign,key);
    require(missing.success&&!missing.found&&TextDraftRecovery::remove(campaign,key).empty(),
        "Genuinely absent parent was not an idempotent clean start");
    require(TextDraftRecovery::save(campaign,key,{{"notes","PRESERVE_PREVIOUS_CHECKPOINT"}}).empty(),"Cannot seed ancestor fixture");
    const auto original=bytes(path);const auto parent=path.parent_path();const fs::path held(parent.string()+"-held");
    require(!fs::exists(held),"Ancestor fixture destination already exists");
    fs::rename(parent,held);
    struct RestoreParent {
        fs::path parent,held;
        ~RestoreParent(){if(!parent.empty()){std::error_code ignored;fs::remove(parent,ignored);fs::rename(held,parent,ignored);}}
    } restore{parent,held};
    AtomicProjectFile::write(parent,"REGULAR_FILE_BLOCKING_DIRECTORY");
    const auto obstructed=TextDraftRecovery::read(campaign,key,true);
    require(!obstructed.success&&!obstructed.rawAvailable&&!obstructed.error.empty(),"Obstructed read falsely reported clean absence");
    require(!TextDraftRecovery::save(campaign,key,{{"notes","NEW_RAM"}}).empty(),"Obstructed save falsely succeeded");
    require(!TextDraftRecovery::remove(campaign,key).empty(),"Obstructed discard falsely reported absent checkpoint");
    require(bytes(held/path.filename())==original&&bytes(parent)=="REGULAR_FILE_BLOCKING_DIRECTORY","Obstructed operations changed original or obstruction bytes");
    fs::remove(parent);fs::rename(held,parent);restore.parent.clear();restore.held.clear();
    require(TextDraftRecovery::read(campaign,key).fields.at("notes")=="PRESERVE_PREVIOUS_CHECKPOINT","Recovery retry lost prior checkpoint");
    require(TextDraftRecovery::remove(campaign,key).empty()&&TextDraftRecovery::remove(campaign,key).empty(),"Regular deletion/missing-leaf retry failed");
    fs::create_directory(path);
    require(!TextDraftRecovery::read(campaign,key).success&&!TextDraftRecovery::remove(campaign,key).empty()&&fs::is_directory(path),
        "Empty directory was accepted or removed as a checkpoint");
    fs::remove(path);
    require(TextDraftRecovery::save(campaign,key,{{"notes","RETRY"}}).empty()&&TextDraftRecovery::read(campaign,key).fields.at("notes")=="RETRY",
        "Restored directory did not allow a checked retry");
}
}
int main()
{
    try
    {
        const auto root = fs::temp_directory_path() / ("gm-draft-recovery-" + AtomicProjectFile::uniqueSuffix());
        isolate("TTRPG_DATA_ROOT", root.string()); isolate("TTRPG_DB_PATH", (root / "TTRPG.db").string());
        isolate("TTRPG_SETTINGS_PATH", (root / "GMEditor.settings").string());
        Campaign campaign; campaign.metadata.id = "recovery-campaign"; campaign.name = "Generated recovery fixture";
        roundTripAndIsolation(campaign); malformedAndSizeBound(campaign); failedPublicationAndLinks(campaign, root);
        independentOutcomeSurfaces(campaign);
        absentVersusObstructed();
        std::cout << "PASS text recovery: 5 groups; isolated files, no UI\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
