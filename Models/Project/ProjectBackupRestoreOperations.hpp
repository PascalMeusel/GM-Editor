#pragma once
#include "Models/Project/ProjectBackupRestoreService.hpp"
#include <functional>

// Internal deterministic filesystem fault boundary; the public service always
// uses systemOperations. Repository changes still use the real SQLite service.
namespace ProjectBackupRestore::detail {
struct Operations {
    using Pair=std::function<void(const std::filesystem::path &,const std::filesystem::path &)>;
    Pair copyFile,renameDirectory;
    std::function<void(const std::filesystem::path &)> removeDirectory;
};
Operations systemOperations();
RestoreResult restoreWithOperations(const BackupEntry &,const Operations &);
}
