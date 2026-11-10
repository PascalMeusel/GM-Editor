#include "Models/Session/PlayerRecapPackage.hpp"
#include "Models/Session/PlayerDisplayReviewService.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Project/ProjectAssetImportService.hpp"
#include "Models/Project/ProjectDirectoryPublish.hpp"
#include "Models/Project/ProjectPackageExportPaths.hpp"
#include <set>

namespace
{
std::string imageExtension(const std::string &path)
{
    auto ext = std::filesystem::u8path(path).extension().u8string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".bmp" && ext != ".webp")
        throw std::runtime_error("Recap attachments support PNG, JPEG, BMP and WebP images only.");
    return ext;
}
void plainDirectory(const std::filesystem::path &path)
{
    for (auto p = std::filesystem::absolute(path); !p.empty(); p = p.parent_path())
    {
        if (std::filesystem::is_symlink(std::filesystem::symlink_status(p))) throw std::runtime_error("Export paths cannot contain symbolic links.");
#ifdef _WIN32
        const auto attr = GetFileAttributesW(p.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT)) throw std::runtime_error("Export paths cannot contain reparse points.");
#endif
        if (p == p.root_path()) break;
    }
}
}
bool PlayerRecapPackage::review(const Campaign &campaign, const EntityId &sessionId, const std::vector<EntityId> &ids)
{
    assets_.clear(); handouts_.clear(); document_ = {}; recap_ = {}; error_.clear(); output_.clear();
    try
    {
        auto recap = SessionOutcomes::reviewRecap(campaign, sessionId);
        if (!recap.success) throw std::runtime_error(recap.error);
        PlayerDisplayReviewSnapshot snapshot; snapshot.eligible = true;
        std::set<EntityId> seen;
        for (const auto &id : ids)
        {
            if (!seen.insert(id).second || !playerSafeContainsId(campaign.localPlayerDisplay.publicHandoutIds, id))
                throw std::runtime_error("Select each publicly allowed handout at most once.");
            const auto source = std::find_if(campaign.handouts.begin(), campaign.handouts.end(), [&](const auto &v) { return v.metadata.id == id; });
            if (source == campaign.handouts.end()) throw std::runtime_error("Selected handout is unavailable.");
            PlayerSafeHandout safe{id, source->title, source->body, {}, {}};
            if (!PlayerDisplayReview::bindManagedHandout(campaign, *source, safe)) throw std::runtime_error("Selected handout image is missing or unmanaged.");
            if (!safe.projectRelativePath.empty()) (void)imageExtension(safe.projectRelativePath);
            snapshot.playerHandouts.push_back(std::move(safe));
        }
        if (!assets_.capture(snapshot)) throw std::runtime_error(assets_.error());
        recap_ = std::move(recap); handouts_ = std::move(snapshot.playerHandouts); document_ = recap_.document;
        for (const auto &handout : handouts_)
            document_.items.push_back({handout.assetId, handout.title + (handout.body.empty() ? "" : "\n\n" + handout.body)});
        return true;
    }
    catch (const std::exception &e) { error_ = e.what(); assets_.clear(); return false; }
}
bool PlayerRecapPackage::current(const Campaign &campaign, bool verifyBytes) const
{
    if (!SessionOutcomes::isRecapCurrent(campaign, recap_) || !assets_.sourcesCurrent(verifyBytes)) return false;
    for (const auto &handout : handouts_)
    {
        if (!playerSafeContainsId(campaign.localPlayerDisplay.publicHandoutIds, handout.assetId)) return false;
        const auto source = std::find_if(campaign.handouts.begin(), campaign.handouts.end(), [&](const auto &v) { return v.metadata.id == handout.assetId; });
        if (source == campaign.handouts.end() || source->title != handout.title || source->body != handout.body) return false;
        if (verifyBytes)
        {
            PlayerSafeHandout now{handout.assetId, source->title, source->body, {}, {}};
            if (!PlayerDisplayReview::bindManagedHandout(campaign, *source, now) || now.projectRelativePath != handout.projectRelativePath || now.contentHash != handout.contentHash) return false;
        }
        else if (source->filePath.empty() != handout.projectRelativePath.empty()) return false;
    }
    return true;
}
std::filesystem::path PlayerRecapPackage::imagePath(const PlayerSafeHandout &handout) const
{ return assets_.path(handout.projectRelativePath, handout.contentHash); }
FrozenImageAsset PlayerRecapPackage::image(const PlayerSafeHandout &handout) const
{ return assets_.image(handout.projectRelativePath, handout.contentHash); }
bool PlayerRecapPackage::exportTo(const Campaign &campaign, const std::filesystem::path &parent)
{
    error_.clear(); output_.clear(); std::filesystem::path stage;
    try
    {
        if (!current(campaign, true)) throw std::runtime_error("Recap content or attachment consent changed. Review again before exporting.");
        plainDirectory(parent); std::filesystem::create_directories(parent); plainDirectory(parent);
        const auto suffix = AtomicProjectFile::uniqueSuffix();
        const auto candidate = parent / ("recap_" + suffix + ".staging");
        if (!std::filesystem::create_directory(candidate)) throw std::runtime_error("Cannot reserve recap staging folder.");
        stage = candidate;
        std::string md = SessionOutcomes::markdown(document_), html = SessionOutcomes::html(document_), images;
        std::size_t number = 0;
        for (const auto &handout : handouts_)
        {
            if (handout.projectRelativePath.empty()) continue;
            const auto name = "attachment-" + std::to_string(++number) + imageExtension(handout.projectRelativePath);
            std::filesystem::copy_file(imagePath(handout), stage / name);
            // Temp ownership does not make a file immutable to external edits.
            // Verify published bytes, not only the still-current source asset.
            if (ProjectAssetImport::computeContentHash(stage / name) != handout.contentHash)
                throw std::runtime_error("A frozen recap attachment changed. Review again before exporting.");
            md += "![Attachment " + std::to_string(number) + "](" + name + ")\n\n";
            images += "<figure><img style=\"max-width:100%\" alt=\"Attachment " + std::to_string(number) + "\" src=\"" + name + "\"></figure>";
        }
        html.insert(html.rfind("</body>"), images);
        AtomicProjectFile::write(stage / "recap.md", md); AtomicProjectFile::write(stage / "recap.html", html);
        // Check source/consent again after I/O; only frozen bytes were copied.
        if (!current(campaign, true)) throw std::runtime_error("Recap changed during export. Review again.");
        const auto destination = parent / ("recap_" + suffix);
        ProjectDirectoryPublish::rename(stage, destination); stage.clear(); output_ = destination; return true;
    }
    catch (const std::exception &e)
    {
        error_ = e.what();
        if (!stage.empty())
        {
            try { ProjectPackageExportPaths::rejectLinks(stage); std::filesystem::remove_all(stage); }
            catch (...) { error_ += " Incomplete staging folder retained after cleanup failure."; }
        }
        return false;
    }
}
