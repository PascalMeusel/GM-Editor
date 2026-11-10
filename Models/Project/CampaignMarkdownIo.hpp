#pragma once
#include "CampaignMarkdownTask.hpp"
#include <stdexcept>

namespace CampaignMarkdownExchange::detail
{
struct Cancelled : std::runtime_error {Cancelled():std::runtime_error("Note exchange cancelled.") {}};
struct Io {
    Limits limits;
    std::function<bool()> cancelled;
    std::function<void(TaskPhase,std::uint64_t,std::uint64_t)> progress;
    TaskPhase phase=TaskPhase::Reading;
    void check() const;
    void report(std::uint64_t bytes=0,std::uint64_t files=0) const;
    std::string read(const std::filesystem::path &,std::size_t max,bool text=true) const;
    std::string hash(const std::filesystem::path &,std::uintmax_t expected) const;
    void write(const std::filesystem::path &,const std::string &) const;
    void copy(const std::filesystem::path &,const std::filesystem::path &,const Attachment &) const;
};
void safePath(const std::filesystem::path &);
std::string digest(const std::string &);
inline constexpr auto ManifestName="EDITABLE_NOTES.v1";
std::string editablePath(const EntityId &);
std::string baselinePath(const EntityId &);
void writeEditable(const ExportInput &,const std::filesystem::path &,const Io &);
UpdatePreview readUpdates(const EntityId &,const std::filesystem::path &,const Io &);
void checkUpdates(const UpdatePreview &,const Io &);
void runTask(const TaskInput &,TaskResult &,Io &);
void cleanupStage(TaskResult &) noexcept;
}
