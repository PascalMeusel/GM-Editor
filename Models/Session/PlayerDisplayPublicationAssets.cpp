#include "Models/Session/PlayerDisplayPublicationAssets.hpp"
#include "Models/Project/AtomicProjectFile.hpp"

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace
{
constexpr std::uintmax_t maxAssetBytes = 256ull * 1024 * 1024;

std::string hashBytes(const std::filesystem::path &source,
                      const std::shared_ptr<std::atomic<bool>> &cancel,
                      std::ostream *copy = nullptr)
{
    std::ifstream input(mapFileIoPath(source), std::ios::binary);
    if (!input) return {};
    std::uint64_t hash = 14695981039346656037ull;
    std::uintmax_t bytesRead = 0;
    std::array<char, 65536> buffer{};
    while (input)
    {
        if (cancel && cancel->load()) return {};
        input.read(buffer.data(), buffer.size());
        const auto length = input.gcount();
        bytesRead += static_cast<std::uintmax_t>(length);
        if (bytesRead > maxAssetBytes) return {};
        if (copy) copy->write(buffer.data(), length);
        for (std::streamsize i = 0; i < length; ++i)
        { hash ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]); hash *= 1099511628211ull; }
    }
    if (input.bad() || (copy && !*copy)) return {};
    std::ostringstream result;
    result << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return result.str();
}
}

PlayerDisplayPublicationAssets::~PlayerDisplayPublicationAssets() { clear(); }

void PlayerDisplayPublicationAssets::clear()
{
    generation_.reset(); // Renderer retirement is deferred; no GL work here.
    if (cancel_) cancel_->store(true);
    // The worker touches only copied source descriptors, never campaign/UI
    // objects. Cancellation is checked after each bounded read before joining.
    if (verification_.valid()) { try { verification_.get(); } catch (...) {} }
    assets_.clear();
    // A decoder may retain storage, but never the revoked generation. The last
    // storage owner removes the private files after its bounded read finishes.
    storage_.reset();
    directory_.clear();
    cancel_.reset();
}

bool PlayerDisplayPublicationAssets::capture(const PlayerDisplayReviewSnapshot &review)
{
    try {return capture(PlayerDisplayReview::ownConfirmation(review));}
    catch(const std::exception &exception){clear();error_=exception.what();return false;}
}

bool PlayerDisplayPublicationAssets::capture(const PlayerDisplayReview::Confirmation &review)
{
    clear();
    error_.clear();
    try
    {
        if (!review.eligible) throw std::runtime_error("Publication requires an eligible review.");
        cancel_ = std::make_shared<std::atomic<bool>>(false);
        std::uintmax_t total = 0;
        const auto add = [&](const std::string &stored, const std::string &hash) {
            if (stored.empty()) return;
            if (hash.empty() || !MapImportedReferencePolicy::isSafeProjectRelativePath(stored))
                throw std::runtime_error("Publication asset is not bound to reviewed bytes.");
            for (const auto &asset : assets_)
                if (asset.projectPath == stored)
                {
                    if (asset.hash != hash) throw std::runtime_error("Conflicting publication asset hashes.");
                    return;
                }
            Asset asset;
            asset.projectPath = stored;
            asset.hash = hash;
            asset.source = ApplicationPaths::writableRoot() / std::filesystem::u8path(stored);
            asset.canonical = std::filesystem::canonical(asset.source);
            const auto relative = asset.canonical.lexically_relative(
                std::filesystem::canonical(ApplicationPaths::writableRoot()));
            if (relative.empty() || relative.is_absolute() ||
                *relative.begin() == "..")
                throw std::runtime_error("Publication asset escapes the project storage root.");
            if (!std::filesystem::is_regular_file(asset.canonical)) throw std::runtime_error("Publication asset is missing.");
            asset.size = std::filesystem::file_size(asset.canonical);
            asset.modified = std::filesystem::last_write_time(asset.canonical);
            constexpr std::uintmax_t maxPublication = 512ull * 1024 * 1024;
            if (asset.size > maxAssetBytes || total > maxPublication - asset.size)
                throw std::runtime_error("Publication assets exceed the safe snapshot size limit.");
            total += asset.size;
            if (directory_.empty())
            {
                storage_ = FrozenImageStorage::create();
                directory_ = storage_->directory();
            }
            asset.frozen = directory_ / (std::to_string(assets_.size()) + asset.source.extension().u8string());
            std::ofstream output(asset.frozen, std::ios::binary);
            if (!output || hashBytes(asset.canonical, cancel_, &output) != asset.hash)
                throw std::runtime_error("Publication asset changed after review.");
            output.close();
            if (!output) throw std::runtime_error("Could not finish publication asset snapshot.");
            assets_.push_back(std::move(asset));
        };
        for (const auto &image : review.images) add(image.projectRelativePath, image.contentHash);
        if (!sourcesCurrent(false)) throw std::runtime_error("Publication assets changed while being captured.");
        generation_ = std::make_shared<const FrozenImageAssetGeneration>();
        nextVerification_ = Clock::now();
        return true;
    }
    catch (const std::exception &exception)
    {
        error_ = exception.what();
        clear();
        return false;
    }
}

bool PlayerDisplayPublicationAssets::current(const std::vector<Asset> &assets, bool verifyBytes,
                                              const std::shared_ptr<std::atomic<bool>> &cancel)
{
    try
    {
        for (const auto &asset : assets)
        {
            if ((cancel && cancel->load()) || std::filesystem::canonical(asset.source) != asset.canonical ||
                !std::filesystem::is_regular_file(asset.canonical) ||
                std::filesystem::file_size(asset.canonical) != asset.size ||
                std::filesystem::last_write_time(asset.canonical) != asset.modified)
                return false;
            if (verifyBytes && hashBytes(asset.canonical, cancel) != asset.hash) return false;
        }
        return true;
    }
    catch (...) { return false; }
}

bool PlayerDisplayPublicationAssets::sourcesCurrent(bool verifyBytes) const
{
    return current(assets_, verifyBytes, cancel_);
}

bool PlayerDisplayPublicationAssets::pollSources(Clock::time_point now)
{
    const auto revoked = [this] { generation_.reset(); return false; };
    if (!sourcesCurrent(false)) return revoked();
    if (verification_.valid())
    {
        if (verification_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return true;
        try { if (!verification_.get()) return revoked(); }
        catch (...) { return revoked(); }
    }
    if (!assets_.empty() && now >= nextVerification_)
    {
        nextVerification_ = now + std::chrono::milliseconds(500);
        try
        {
            verification_ = std::async(std::launch::async, [assets = assets_, cancel = cancel_] {
                return current(assets, true, cancel);
            });
        }
        catch (...) { return revoked(); }
    }
    return true;
}

std::filesystem::path PlayerDisplayPublicationAssets::path(const std::string &projectPath, const std::string &hash) const
{
    for (const auto &asset : assets_)
        if (asset.projectPath == projectPath && asset.hash == hash) return asset.frozen;
    return {};
}

FrozenImageAsset PlayerDisplayPublicationAssets::image(const std::string &projectPath, const std::string &hash) const
{
    if (!generation_) return {};
    for (const auto &asset : assets_)
        if (asset.projectPath == projectPath && asset.hash == hash)
            return {asset.frozen, asset.hash, generation_, storage_};
    return {};
}
