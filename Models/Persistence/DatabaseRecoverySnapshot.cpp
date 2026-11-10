#include "Models/DataBaseManager.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include <SQLiteCpp/Backup.h>
#include <sqlite3.h>

// Private recovery backups retain SQLite pages without the compaction work
// required by distribution/package snapshots. The original compact API remains.
void DatabaseManager::writeRecoverySnapshot(const std::string &snapshotPath)
{
    const auto destination = mapFileIoPath(std::filesystem::u8path(snapshotPath));
    auto temporary = destination;
    temporary += ".pending_" + AtomicProjectFile::uniqueSuffix();
#if defined(_WIN32)
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::system_error(GetLastError(),std::system_category(),"Create recovery staging file");
    CloseHandle(file);
#else
    const int file = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (file < 0) throw std::system_error(errno,std::generic_category(),"Create recovery staging file");
    ::close(file);
#endif
    try {
        {
            SQLite::Database target(temporary.u8string(), SQLite::OPEN_READWRITE);
            // Only this new, unpublished staging database uses an in-memory
            // rollback journal. It contains no previous recovery to protect.
            // FULL still flushes the completed database before atomic publication;
            // a crash before publication can leave only an unreferenced temporary.
            // Never change the live database's journal or synchronization mode.
            target.exec("PRAGMA journal_mode=MEMORY; PRAGMA synchronous=FULL;");
            SQLite::Backup backup(target, *m_db);
            if (backup.executeStep(-1) != SQLITE_DONE) throw std::runtime_error("Recovery backup could not acquire the database snapshot.");
        }
#if defined(_WIN32)
        // No REPLACE_EXISTING: an older recovery file must survive a collision.
        if (!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH))
            throw std::system_error(GetLastError(),std::system_category(),"Publish recovery snapshot");
#else
        if (::link(temporary.c_str(),destination.c_str()) != 0)
            throw std::system_error(errno,std::generic_category(),"Publish recovery snapshot");
        ::unlink(temporary.c_str());
#endif
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary,ignored);
        auto journal = temporary; journal += "-journal";
        std::filesystem::remove(journal,ignored);
        throw;
    }
}
