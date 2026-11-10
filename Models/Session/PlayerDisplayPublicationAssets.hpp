#pragma once
#include "Models/Session/PlayerDisplayReviewTypes.hpp"
#include "Models/Session/PlayerDisplayReviewConfirmation.hpp"
#include "Models/Session/FrozenImageAsset.hpp"
#include <atomic>
#include <chrono>
#include <future>
#include <memory>

// Owns private, byte-verified publication files. Renderers use image() descriptors;
// path() remains available for non-rendering exports, never mutable originals.
// Source revocation is checked cheaply on
// every poll. A background byte check is scheduled every 500 ms, with at most
// one check in flight; detection also depends on scan duration. Even during
// that delay, rendering can consume only the originally approved bytes.
class PlayerDisplayPublicationAssets
{
public:
    using Clock = std::chrono::steady_clock;
    PlayerDisplayPublicationAssets() = default;
    PlayerDisplayPublicationAssets(const PlayerDisplayPublicationAssets &) = delete;
    PlayerDisplayPublicationAssets &operator=(const PlayerDisplayPublicationAssets &) = delete;
    PlayerDisplayPublicationAssets(PlayerDisplayPublicationAssets &&) = delete;
    PlayerDisplayPublicationAssets &operator=(PlayerDisplayPublicationAssets &&) = delete;
    ~PlayerDisplayPublicationAssets();
    bool capture(const PlayerDisplayReviewSnapshot &review);
    bool capture(const PlayerDisplayReview::Confirmation &review);
    void clear();
    bool pollSources(Clock::time_point now = Clock::now());
    bool sourcesCurrent(bool verifyBytes) const;
    std::filesystem::path path(const std::string &projectPath, const std::string &contentHash) const;
    FrozenImageAsset image(const std::string &projectPath, const std::string &contentHash) const;
    const std::string &error() const { return error_; }

private:
    struct Asset
    {
        std::string projectPath, hash;
        std::filesystem::path source, canonical, frozen;
        std::uintmax_t size = 0;
        std::filesystem::file_time_type modified;
    };
    static bool current(const std::vector<Asset> &assets, bool verifyBytes,
                        const std::shared_ptr<std::atomic<bool>> &cancel);
    std::vector<Asset> assets_;
    std::shared_ptr<const FrozenImageAssetGeneration> generation_;
    std::shared_ptr<const FrozenImageStorage> storage_;
    std::filesystem::path directory_;
    std::shared_ptr<std::atomic<bool>> cancel_;
    std::future<bool> verification_;
    Clock::time_point nextVerification_{};
    std::string error_;
};
