#include "workflow_test_database.hpp"
#include "Models/Project/CampaignMarkdownExchange.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/CampaignTagService.hpp"
#include "Models/Project/ProjectDirectoryPublishExclusive.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "markdown_task_test_support.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
using namespace CampaignMarkdownExchange;
void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
std::string bytes(const std::filesystem::path &path)
{ std::ifstream stream(mapFileIoPath(path), std::ios::binary); return {std::istreambuf_iterator<char>(stream), {}}; }
void write(const std::filesystem::path &path, const std::string &text)
{ std::filesystem::create_directories(path.parent_path()); AtomicProjectFile::write(path, text); }
#include "markdown_directory_publish_cases.hpp"
Campaign reload(const EntityId &id)
{
    for (auto c : CampaignRepository::loadAllCampaigns()) if (c.metadata.id == id) return c;
    throw std::runtime_error("Saved campaign not found.");
}
Campaign fixture()
{
    Campaign c; c.name = "Private campaign"; c.ruleSet = std::make_shared<DND5e>(); c.description = "PRIVATE_DESCRIPTION";
    StoryThreadModel t; t.title = "A [title] | new\nline"; t.gmNotes = "PRIVATE_THREAD\nSecond paragraph.";
    StoryBeatModel b; b.title = "Scene"; b.gmNotes = "PRIVATE_SCENE"; t.beats.push_back(b); c.storyThreads.push_back(t);
    NPCModel n; n.name = "Mara"; n.secrets = "PRIVATE_SECRET"; c.npcs.push_back(n);
    StoryLinkModel link; link.label = "Mara knows the scene"; link.targetEntityId = n.metadata.id; link.targetEntityType = "npc";
    c.storyThreads[0].links.push_back(link);
    HandoutModel h; h.title = "Handout"; h.body = "VISIBLE_BODY"; h.gmNotes = "PRIVATE_HANDOUT";
    h.filePath = "C:/external-do-not-copy/secret.pdf"; c.handouts.push_back(h);
    return c;
}
void exportChecks(const std::filesystem::path &root)
{
    auto c = fixture(); const auto layout = ProjectPackageLayout::layoutForCampaign(c);
    write(layout.assetsDirectory / "Handouts" / "chosen [one].txt", "Selected exact bytes\r\n");
    write(layout.assetsDirectory / "Handouts" / "unselected.txt", "NOT_SELECTED");
    write(layout.assetsDirectory / "Files" / "do-not-copy.db", "PRIVATE_DATABASE");
    write(layout.backupsDirectory / "backup.md", "PRIVATE_BACKUP");
    write(layout.recoveryDirectory / "draft.md", "PRIVATE_RECOVERY");
    write(layout.rootDirectory / "campaign.db", "PRIVATE_PACKAGE_DB");
    const auto preview = previewExport(c);
    check(preview.success && preview.documents.size() == 5 && preview.attachments.size() == 2,
        "Preview did not scope note/attachment inventory correctly");
    check(!preview.warnings.empty(), "Unsupported managed file omitted without warning");
    const auto destination = root / "markdown-out";
    const auto result = exportNotes(c, preview, destination, {"Handouts/chosen [one].txt"});
    check(result.success && bytes(destination / "attachments/1.txt") == "Selected exact bytes\r\n", "Selected attachment bytes changed");
    const auto index = bytes(destination / "INDEX.md");
    check(index.find("| `" + c.storyThreads[0].metadata.id + "` | story-thread |") != std::string::npos,
        "Stable ID index must preserve literal IDs inside code spans");
    check(index.find("Contains private GM information. Do not use as a player recap.") != std::string::npos,
        "Privacy notice absent");
    check(index.find("NOT_SELECTED") == std::string::npos && !std::filesystem::exists(destination / "Backups") &&
        !std::filesystem::exists(destination / "Recovery") && !std::filesystem::exists(destination / "campaign.db") &&
        std::distance(std::filesystem::directory_iterator(destination / "attachments"), std::filesystem::directory_iterator{}) == 1,
        "Export copied non-selected or package data");
    const auto npc = std::find_if(preview.documents.begin(), preview.documents.end(), [](const auto &d) { return d.type == "npc"; });
    check(npc != preview.documents.end() && bytes(destination / npc->relativePath).find("PRIVATE_SECRET") != std::string::npos,
        "GM notes export silently omitted secrets");
    check(preview.documents[1].markdown.find("Stable ID: `" + c.storyThreads[0].metadata.id + "`") != std::string::npos &&
        preview.documents[1].markdown.find("npc `" + c.npcs[0].metadata.id + "`") != std::string::npos &&
        preview.documents[1].markdown.find("Mara knows the scene") != std::string::npos, "Linked stable reference was lost");
    const auto renamedPath = preview.documents[1].relativePath; c.storyThreads[0].title = "Renamed";
    check(previewExport(c).documents[1].relativePath == renamedPath, "Renaming a note changed its stable path");
    check(!exportNotes(c, preview, root / "stale").success && !std::filesystem::exists(root / "stale"), "Stale campaign preview was exported");
    c.storyThreads[0].title = "A [title] | new\nline";
    check(!exportNotes(c, preview, destination).success && bytes(destination / "INDEX.md") == index, "Existing destination was overwritten");
    check(!exportNotes(c, preview, layout.rootDirectory / "export").success, "Export into campaign package accepted");
    check(!exportNotes(c, preview, ProjectPackageLayout::campaignsDirectory().parent_path()).success, "Ancestor destination accepted");
    check(!exportNotes(c, preview, root / "unknown", {"Handouts/../Files/do-not-copy.db"}).success, "Unreviewed traversal attachment accepted");
    auto forged = preview; forged.documents[0].relativePath = "../escape.md";
    check(!exportNotes(c, forged, root / "forged").success && !std::filesystem::exists(root / "escape.md"), "Forged note path accepted");
    write(layout.assetsDirectory / "Handouts" / "chosen [one].txt", "Changed source");
    check(!exportNotes(c, preview, root / "changed", {"Handouts/chosen [one].txt"}).success, "Changed attachment accepted");
    auto none = exportNotes(c, preview, root / "notes-only");
    check(none.success && !std::filesystem::exists(root / "notes-only/attachments"), "Text-only export implicitly copied files");
    // Symlinks may need privileges on Windows. Exercise rejection when supported.
    std::error_code ec; const auto link = layout.assetsDirectory / "Handouts/link.txt";
    std::filesystem::create_symlink(root / "external.txt", link, ec);
    if (!ec)
    {
        write(root / "external.txt", "OUTSIDE"); const auto safe = previewExport(c);
        check(safe.success && std::none_of(safe.attachments.begin(), safe.attachments.end(), [](const auto &a) { return a.relativePath == "Handouts/link.txt"; }),
            "Linked external attachment entered inventory");
    }
    else std::cout << "SKIP: Markdown symlink fixture unavailable: " << ec.message() << '\n';
    c.storyThreads[0].metadata.id = "../../unsafe\\stable:id";
    const auto encoded = previewExport(c); check(encoded.success, "Unusual stable ID could not be encoded");
    check(encoded.documents[1].relativePath.find("..") == std::string::npos && encoded.documents[1].relativePath.find('\\') == std::string::npos,
        "Stable ID escaped the notes directory");
    c.storyThreads[0].metadata.id = "`literal_id`|[raw]";
    c.storyThreads[0].links[0].targetEntityId = c.storyThreads[0].metadata.id;
    const auto special = previewExport(c); const auto specialRoot = root / "special-ids";
    check(special.success && exportNotes(c, special, specialRoot).success, "Special stable ID export failed");
    check(special.documents[1].markdown.find("Stable ID: `` `literal_id`|[raw] ``") != std::string::npos &&
        special.documents[1].markdown.find("npc `` `literal_id`|[raw] ``") != std::string::npos &&
        bytes(specialRoot / "INDEX.md").find("| `` `literal_id`\\|[raw] `` | story-thread |") != std::string::npos,
        "Backtick/pipe ID code spans are ambiguous or escaped as ordinary prose");
}
void importChecks(const std::filesystem::path &root)
{
    auto c = fixture(); CampaignRepository::saveCampaign(c);
    int saves = 0; const SessionPlan::Persist persist = [&](Campaign &next) { ++saves; CampaignRepository::saveCampaign(next); return true; };
    const auto source = root / std::filesystem::u8path("Überblick.md");
    const std::string original = "\xef\xbb\xbf# Ruß 🕯 第二場\r\n\r\n---\r\nprivate: true\r\n---\r\n![local](secret.png)\r\n[link](https://example.invalid)\r\n<script>text only</script>\r\n";
    write(source, original); const auto preview = previewImport(c, source);
    check(preview.success && preview.text == original && preview.title == "Überblick", "Import preview altered Unicode/BOM/CRLF or title");
    check(c.storyThreads.size() == 1 && saves == 0, "Preview mutated campaign");
    const auto failed = importNote(c, preview, [](Campaign &) { return false; });
    check(!failed.success && c.storyThreads.size() == 1, "Failed save leaked imported note");
    auto other = fixture(); check(!importNote(other, preview, persist).success && saves == 0, "Cross-campaign preview accepted");
    auto collision = preview; collision.noteId = c.npcs[0].metadata.id;
    check(!importNote(c, collision, persist).success && saves == 0, "Existing non-note identity collision accepted");
    auto altered = preview; altered.text += "unreviewed";
    check(!importNote(c, altered, persist).success && saves == 0, "Tampered import preview accepted");
    write(source, "changed"); check(!importNote(c, preview, persist).success && saves == 0, "Source changed after review was imported");
    write(source, original);
    SQLite::Database db(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
    db.exec("CREATE TRIGGER reject_markdown BEFORE INSERT ON campaign_entity_properties WHEN NEW.property_key = 'gm.markdown-import.original-text' BEGIN SELECT RAISE(ABORT,'injected markdown save failure'); END;");
    const auto injected = importNote(c, preview, persist);
    check(!injected.success && c.storyThreads.size() == 1 && reload(c.metadata.id).storyThreads.size() == 1,
        "SQL failure partially saved or published imported note");
    db.exec("DROP TRIGGER reject_markdown;");
    const auto imported = importNote(c, preview, persist);
    check(imported.success && imported.changed && imported.noteId == preview.noteId && c.storyThreads.size() == 2, "Reviewed import did not commit");
    c = reload(c.metadata.id);
    const auto found = std::find_if(c.storyThreads.begin(), c.storyThreads.end(),
        [&](const auto &entry) { return entry.metadata.id == imported.noteId; });
    check(found != c.storyThreads.end(), "Reload lost imported stable identity");
    const auto &note = *found;
    check(note.gmNotes == original && note.metadata.customProperties.at("gm.markdown-import.original-text") == original &&
        note.metadata.customProperties.at("gm.markdown-import.source-name") == "Überblick.md", "Reload lost original text/provenance");
    check(note.links.empty() && c.handouts.size() == 1, "Import resolved external links or made attachments automatically");
    const auto count = saves; const auto retry = importNote(c, preview, persist);
    check(retry.success && !retry.changed && saves == count && c.storyThreads.size() == 2, "Retry duplicated imported note");
    const auto exported = previewExport(c);
    const auto doc = std::find_if(exported.documents.begin(), exported.documents.end(), [&](const auto &d) { return d.id == note.metadata.id; });
    check(exported.success && doc != exported.documents.end() && doc->markdown.find(original) != std::string::npos, "Imported bytes disappeared from prose export");
    write(root / "invalid.md", std::string("\xc0\xaf", 2)); check(!previewImport(c, root / "invalid.md").success, "Overlong invalid UTF-8 accepted");
    write(root / "nul.md", std::string("a\0b", 3)); check(!previewImport(c, root / "nul.md").success, "NUL bytes accepted");
    write(root / "oversize.md", std::string(4U * 1024 * 1024 + 1, 'x')); check(!previewImport(c, root / "oversize.md").success, "Oversized text accepted");
    write(root / "empty.md", ""); check(previewImport(c, root / "empty.md").success, "Empty Markdown was altered or refused");
    write(root / "plain.txt", "text"); check(!previewImport(c, root / "plain.txt").success, "Unsupported extension accepted");
}
#include "markdown_round_trip_cases.hpp"
#include "markdown_round_trip_io_cases.hpp"
#include "markdown_task_cases.hpp"
}
int main()
{
    try
    {
        const auto root = std::filesystem::temp_directory_path() / createEntityId("markdown-exchange-tests");
        std::filesystem::create_directories(root);
#ifdef _WIN32
        _putenv_s("TTRPG_DATA_ROOT", (root / "data").string().c_str());
#else
        setenv("TTRPG_DATA_ROOT", (root / "data").string().c_str(), 1);
#endif
        isolateWorkflowDatabase(); markdownDirectoryPublish(root); exportChecks(root); importChecks(root);
        markdownClassification();markdownApplyPersistence();markdownPostSaveMerge();
        markdownRoundTripIo(root);
        markdownWorkerCases(root);
        std::cout << "Campaign Markdown exchange passed\n"; return 0;
    }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
