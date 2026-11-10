#include "Models/Project/ProjectDirectoryPublish.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        const auto root=std::filesystem::temp_directory_path() / ("gm-directory-publish-"+AtomicProjectFile::uniqueSuffix());
        const auto source=root/"staged", destination=root/"published";
        std::filesystem::create_directories(source);
        std::ofstream(source/"content.txt") << "generated content";
#if defined(_WIN32)
        HANDLE held=CreateFileW(source.c_str(),FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE,
            nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
        if (held==INVALID_HANDLE_VALUE) throw std::runtime_error("Could not hold generated directory fixture");
        std::error_code denied;
        std::filesystem::rename(source,destination,denied);
        if (!denied) { CloseHandle(held);throw std::runtime_error("Fixture did not establish a real rename lock"); }
        std::thread release([held] { std::this_thread::sleep_for(std::chrono::milliseconds(100));CloseHandle(held); });
        try { ProjectDirectoryPublish::rename(source,destination); }
        catch (...) { release.join();throw; }
        release.join();
#else
        ProjectDirectoryPublish::rename(source,destination);
#endif
        if (!std::filesystem::exists(destination/"content.txt") || std::filesystem::exists(source))
            throw std::runtime_error("Published directory content was lost");
        std::filesystem::create_directories(source);
        std::ofstream(source/"original.txt") << "retain";
        bool rejected=false;
        try { ProjectDirectoryPublish::rename(source,destination); } catch (const std::filesystem::filesystem_error &) { rejected=true; }
        if (!rejected || !std::filesystem::exists(source/"original.txt") || !std::filesystem::exists(destination/"content.txt"))
            throw std::runtime_error("Destination conflict did not preserve both directories");
        std::cout << "Directory publication lock/retry and collision tests passed\n";return 0;
    } catch (const std::exception &error) { std::cerr<<error.what()<<'\n';return 1; }
}
