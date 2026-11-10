#pragma once

#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using EntityId = std::string;

inline std::string currentUtcTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);

    std::tm utcTime{};
#if defined(_WIN32)
    gmtime_s(&utcTime, &time);
#else
    gmtime_r(&time, &utcTime);
#endif

    std::ostringstream stream;
    stream << std::put_time(&utcTime, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

inline EntityId createEntityId(const std::string &prefix)
{
    static std::atomic<unsigned long long> counter{0};
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();

    std::ostringstream stream;
    stream << prefix << "_" << std::hex << now << "_" << counter.fetch_add(1);
    return stream.str();
}

struct EntityMetadata
{
    EntityId id;
    std::string createdAtUtc;
    std::string updatedAtUtc;
    std::vector<std::string> tags;
    std::map<std::string, std::string> customProperties;

    explicit EntityMetadata(const std::string &idPrefix = "entity")
        : id(createEntityId(idPrefix)), createdAtUtc(currentUtcTimestamp()), updatedAtUtc(createdAtUtc)
    {
    }

    void touch()
    {
        updatedAtUtc = currentUtcTimestamp();
    }
};
