#pragma once
#include "Models/Project/WorldInspectorDrafts.hpp"
#include <string>
#include <vector>

namespace WorldInspectorHistory
{
template<class T> struct Change { T before, after; };
struct VisibilityBinding
{
    MapGeneratedFeatureKind kind{};
    std::string name, materialId;
    bool bound = false;
    EntityId boundId, nationId, placeId;
    WorldObjectVisibility nationVisibility{}, placeVisibility{};
};
struct VisibilityChange
{
    EntityId mapId, featureId;
    bool beforeGmOnly = false, beforeDiscovered = false;
    bool afterGmOnly = false, afterDiscovered = false;
    VisibilityBinding beforeBinding, afterBinding;
};
struct Patch
{
    std::vector<Change<WorldPlaceModel>> places;
    std::vector<Change<NationModel>> nations;
    std::vector<Change<ShopModel>> shops;
    std::vector<VisibilityChange> visibility;
    bool empty() const { return places.empty() && nations.empty() && shops.empty() && visibility.empty(); }
};
struct CaptureResult
{
    bool success = false;
    Patch patch;
    std::string error;
};
// Both inputs are canonical staged values, immediately before/after the scoped
// World save. Dungeon changes have already occurred in the before input.
CaptureResult capture(const Campaign &before,const Campaign &after,const WorldInspectorDrafts::Scope &);
// Caller owns the staging and persistence transaction. No service normalization,
// global synchronization, record creation or deletion occurs here.
WorldInspector::MutationResult apply(Campaign &,const Patch &,bool forward);
bool overlaps(const Patch &,const WorldInspectorDrafts::Store &);
bool sameDrafts(const WorldInspectorDrafts::Store &,const WorldInspectorDrafts::Store &,
                const WorldInspectorDrafts::Scope &);
bool acknowledgeSubmitted(WorldInspectorDrafts::Store &current,const WorldInspectorDrafts::Store &submitted,
                           const WorldInspectorDrafts::Scope &);
}
