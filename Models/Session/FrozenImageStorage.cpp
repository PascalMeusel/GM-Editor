#include "Models/Session/FrozenImageStorage.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include <stdexcept>

std::shared_ptr<const FrozenImageStorage> FrozenImageStorage::create()
{
    auto storage=std::shared_ptr<FrozenImageStorage>(new FrozenImageStorage(mapFileIoPath(
        std::filesystem::temp_directory_path()/("gm-player-publication-"+AtomicProjectFile::uniqueSuffix()))));
    if (!std::filesystem::create_directory(storage->directory_))
        throw std::runtime_error("Could not reserve publication storage.");
    storage->created_=true;
#if !defined(_WIN32)
    std::filesystem::permissions(storage->directory_,std::filesystem::perms::owner_all,
                                std::filesystem::perm_options::replace);
#endif
    return storage;
}

FrozenImageStorage::~FrozenImageStorage()
{
    if (!created_) return;
    std::error_code ignored;
    std::filesystem::remove_all(directory_,ignored);
}

bool FrozenImageStorage::contains(const std::filesystem::path &path) const
{
    // Publication capture creates flat, private files. No arbitrary root may
    // be leased, and no disk lookup is needed on the rendering thread.
    const auto normalized=path.lexically_normal();
    return normalized==path && normalized.is_absolute() && normalized.parent_path()==directory_.lexically_normal() &&
           !normalized.filename().empty() && normalized.filename()!="." && normalized.filename()!="..";
}
