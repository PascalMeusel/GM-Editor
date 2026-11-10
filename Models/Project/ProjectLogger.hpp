#pragma once

#include <string>
#include <cstdint>
#include <vector>

enum class ProjectLogLevel
{
    Debug,
    Info,
    Warning,
    Error
};

struct ProjectLogEntry
{
    ProjectLogLevel level = ProjectLogLevel::Info;
    std::string source;
    std::string message;
    std::string occurredAtUtc;
    std::uint64_t sequence = 0;
};

class ProjectLogger
{
public:
    static ProjectLogger &getInstance();

    void log(ProjectLogLevel level, const std::string &source, const std::string &message);
    void debug(const std::string &source, const std::string &message) { log(ProjectLogLevel::Debug, source, message); }
    void info(const std::string &source, const std::string &message) { log(ProjectLogLevel::Info, source, message); }
    void warning(const std::string &source, const std::string &message) { log(ProjectLogLevel::Warning, source, message); }
    void error(const std::string &source, const std::string &message) { log(ProjectLogLevel::Error, source, message); }

    const std::vector<ProjectLogEntry> &history() const { return _history; }
    std::vector<ProjectLogEntry> recentProblems(std::size_t limit) const;

private:
    std::vector<ProjectLogEntry> _history;
    std::uint64_t _nextSequence = 1;
};

const char *projectLogLevelLabel(ProjectLogLevel level);
