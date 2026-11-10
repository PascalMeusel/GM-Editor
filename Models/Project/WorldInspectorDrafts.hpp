#pragma once
#include "Models/Project/WorldInspectorService.hpp"
#include <map>

namespace WorldInspectorDrafts
{
template<class T> struct Draft { T base, edited; };
enum class Part { None, All, Entity, ShopDetails, Stock, NationDetails, Territory };
struct Scope
{
    Part part = Part::None;
    EntityId entityId, childId;
    static Scope all() { return {Part::All, {}, {}}; }
    static Scope entity(EntityId id) { return {Part::Entity, std::move(id), {}}; }
};
bool changed(const Draft<WorldPlaceModel> &);
bool changed(const Draft<NationModel> &);
bool changed(const Draft<ShopModel> &);
bool changed(const WorldPlaceModel &,const WorldPlaceModel &);
bool changed(const NationModel &,const NationModel &);
bool changed(const ShopModel &,const ShopModel &);
// Refresh untouched fields from live data; retain authored values and their
// original conflict baseline. Missing edited rows remain available for recovery.
void refresh(Draft<WorldPlaceModel> &, const WorldPlaceModel &);
void refresh(Draft<NationModel> &, const NationModel &);
void refresh(Draft<ShopModel> &, const ShopModel &);
void acknowledge(Draft<WorldPlaceModel> &, const Scope &);
void acknowledge(Draft<NationModel> &, const Scope &);
void acknowledge(Draft<ShopModel> &, const Scope &);
std::size_t uniqueStockIndex(const ShopModel &, const EntityId &);
inline constexpr std::size_t MissingStock = static_cast<std::size_t>(-1);
struct StockTransfer
{
    EntityId shopId,itemId;
    int quantity=0,shopBefore=0,partyBefore=0;
    bool purchase=true;
};
WorldInspector::MutationResult applyTransfer(Campaign &,const StockTransfer &);
WorldInspector::MutationResult removeTerritory(Campaign &,const EntityId &nationId,const EntityId &territoryId);

struct Store
{
    std::map<EntityId, Draft<WorldPlaceModel>> places;
    std::map<EntityId, Draft<NationModel>> nations;
    std::map<EntityId, Draft<ShopModel>> shops;
    void retain(Draft<WorldPlaceModel>);
    void retain(Draft<NationModel>);
    void retain(Draft<ShopModel>);
    bool empty() const { return places.empty() && nations.empty() && shops.empty(); }
    // Caller owns the rollback/persistence transaction. This never acknowledges
    // drafts; acknowledge only after that transaction has committed.
    WorldInspector::MutationResult apply(Campaign &, const Scope &) const;
    void acknowledge(const Scope &);
    void discard(const Scope &);
    void clear() { places.clear(); nations.clear(); shops.clear(); }
};
}
