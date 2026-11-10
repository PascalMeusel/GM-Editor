#pragma once
#include "Models/Session/FrozenImageStorage.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

// Only the reviewed publication owns a strong generation. Descriptors and
// renderer caches observe its lifetime without prolonging the review.
struct FrozenImageAssetGeneration final {};

class FrozenImageAsset
{
public:
    FrozenImageAsset() = default;
    FrozenImageAsset(std::filesystem::path path, std::string hash,
                     std::weak_ptr<const FrozenImageAssetGeneration> lifetime,
                     std::weak_ptr<const FrozenImageStorage> storage = {})
        : path_(std::move(path)), hash_(std::move(hash)), lifetime_(std::move(lifetime)),
          storage_(std::move(storage)) {}
    const std::filesystem::path &path() const { return path_; }
    const std::string &contentHash() const { return hash_; }
    const std::weak_ptr<const FrozenImageAssetGeneration> &lifetime() const { return lifetime_; }
    bool valid() const { return !path_.empty() && !hash_.empty() && !lifetime_.expired(); }
    std::shared_ptr<const FrozenImageStorage> leaseStorage() const
    {
        if (!valid()) return {};
        auto storage=storage_.lock();
        return storage && storage->contains(path_) ? storage : nullptr;
    }
private:
    std::filesystem::path path_;
    std::string hash_;
    std::weak_ptr<const FrozenImageAssetGeneration> lifetime_;
    std::weak_ptr<const FrozenImageStorage> storage_;
};
