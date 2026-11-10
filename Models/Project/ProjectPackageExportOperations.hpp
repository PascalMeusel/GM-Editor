#pragma once
// Internal filesystem boundary for focused fault-injection tests. The public
// export API always uses systemFileOperations; no process-global hooks exist.
#include "Models/Project/ProjectPackageExportService.hpp"
#include <functional>

namespace ProjectPackageExport::detail
{
struct FileOperations
{
    using Operation = std::function<void(const std::filesystem::path &, const std::filesystem::path &)>;
    Operation copyFile;
    Operation renameDirectory;
};
FileOperations systemFileOperations();
ExportResult exportWithFileOperations(const Campaign &, const ExportRequest &, const FileOperations &);
}
