#pragma once
#include "Models/Map/MapModels.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

// Derived serialization only: a cache hit never implies a committed database row.
class MapCellSerializationCache {
public:
    explicit MapCellSerializationCache(std::size_t budget = 64U * 1024U * 1024U) : budget_(budget) {}
    std::string serialize(const std::string &id, const std::vector<MapCellModel> &cells);
    std::size_t bytes() const { return bytes_; }
private:
    struct Entry { std::vector<MapCellModel> cells; std::string encoded; std::uint64_t used = 0; std::size_t bytes = 0; };
    std::unordered_map<std::string, Entry> entries_;
    std::size_t budget_, bytes_ = 0;
    std::uint64_t clock_ = 0;
};
