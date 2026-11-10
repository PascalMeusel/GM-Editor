#include "ProjectLogger.hpp"
#include "Models/Core/EntityMetadata.hpp"

#include <algorithm>
#include <cstddef>

ProjectLogger &ProjectLogger::getInstance()
{
    static ProjectLogger logger;
    return logger;
}

void ProjectLogger::log(ProjectLogLevel level, const std::string &source, const std::string &message)
{
    _history.push_back(ProjectLogEntry{level, source, message, currentUtcTimestamp(), _nextSequence});
    ++_nextSequence;
    constexpr std::size_t maxHistory = 300;
    if (_history.size() > maxHistory)
        _history.erase(_history.begin(), _history.begin() + static_cast<std::ptrdiff_t>(_history.size() - maxHistory));
}

std::vector<ProjectLogEntry> ProjectLogger::recentProblems(std::size_t limit) const
{
    std::vector<ProjectLogEntry> problems;
    for (auto it = _history.rbegin(); it != _history.rend() && problems.size() < limit; ++it)
    {
        if (it->level == ProjectLogLevel::Warning || it->level == ProjectLogLevel::Error)
            problems.push_back(*it);
    }
    return problems;
}

const char *projectLogLevelLabel(ProjectLogLevel level)
{
    switch (level)
    {
    case ProjectLogLevel::Debug:
        return "Debug";
    case ProjectLogLevel::Info:
        return "Info";
    case ProjectLogLevel::Warning:
        return "Warning";
    case ProjectLogLevel::Error:
        return "Error";
    }
    return "Log";
}
