#pragma once
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Core/EntityMetadata.hpp"
#include <cstdlib>
#include <filesystem>

// Each invocation owns a generated database, including when a previous
// fault-injection run terminated before its SQLite trigger could be removed.
inline void isolateWorkflowDatabase()
{
    const auto directory = ApplicationPaths::databasePath().parent_path() / "workflow-runs" / EntityMetadata{"test"}.id;
    std::filesystem::create_directories(directory);
    const auto path = (directory / "TTRPG.db").string();
#ifdef _WIN32
    _putenv_s("TTRPG_DB_PATH", path.c_str());
#else
    setenv("TTRPG_DB_PATH", path.c_str(), 1);
#endif
}
