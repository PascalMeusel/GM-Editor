#include "MapCellSerializationCache.hpp"
#include "PersistenceSerialization.hpp"
#include <algorithm>
#include <tuple>

namespace {
bool same(const MapCellModel &a, const MapCellModel &b) {
    const auto fields = [](const MapCellModel &c) {
        return std::tie(c.kind,c.rotationQuarterTurns,c.rotationLocked,c.symbolVariantMode,c.wayKind,
            c.movementCost,c.difficultTerrain,c.blocksLineOfSight,c.lighting,c.fogged,c.elevation,
            c.movementBlockedMask,c.lineOfSightBlockedMask,c.movementOneWayBlockedMask,c.lineOfSightOneWayBlockedMask);
    };
    return fields(a) == fields(b);
}
}
std::string MapCellSerializationCache::serialize(const std::string &id, const std::vector<MapCellModel> &cells)
{
    auto found = entries_.find(id);
    if (found != entries_.end()) {
        if (found->second.cells.size() == cells.size() && std::equal(cells.begin(),cells.end(),found->second.cells.begin(),same)) {
            found->second.used = ++clock_; return found->second.encoded;
        }
        bytes_ -= found->second.bytes; entries_.erase(found);
    }
    auto encoded = PersistenceSerialization::serializeMapCells(cells);
    const auto required = cells.size() * sizeof(MapCellModel) + encoded.capacity() + id.size() + sizeof(Entry);
    if (required > budget_) return encoded;
    while (!entries_.empty() && (bytes_ + required > budget_ || entries_.size() >= 128)) {
        auto oldest = std::min_element(entries_.begin(),entries_.end(),[](const auto &a,const auto &b){return a.second.used < b.second.used;});
        bytes_ -= oldest->second.bytes; entries_.erase(oldest);
    }
    Entry entry{cells,std::move(encoded),++clock_,required};
    bytes_ += required;
    return entries_.emplace(id,std::move(entry)).first->second.encoded;
}
