#pragma once
#include <filesystem>
#include <memory>
#include <utility>

// Owns only an application-created temporary directory, never player consent.
// A decoder may keep this storage alive after its generation is revoked.
class FrozenImageStorage final
{
public:
    static std::shared_ptr<const FrozenImageStorage> create();
    ~FrozenImageStorage();
    const std::filesystem::path &directory() const {return directory_;}
    bool contains(const std::filesystem::path &) const;
    FrozenImageStorage(const FrozenImageStorage &)=delete;
    FrozenImageStorage &operator=(const FrozenImageStorage &)=delete;
private:
    explicit FrozenImageStorage(std::filesystem::path directory):directory_(std::move(directory)){}
    std::filesystem::path directory_;
    bool created_=false;
};
