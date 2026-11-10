#include "WorldInspectorHistoryFields.hpp"
#include "Models/Map/MapStructuredCreation.hpp"
#include <tuple>

namespace WorldInspectorHistory::detail
{
namespace
{
template<class Rows,class Match> const typename Rows::value_type *matched(const Rows &rows,Match match)
{
    const typename Rows::value_type *result=nullptr;
    for(const auto &row:rows)if(match(row)) {
        if(result)throw std::runtime_error("A generated World visibility binding is ambiguous.");
        unique(rows,row.metadata.id,MetadataId{});available(row);result=&row;
    }
    return result;
}
VisibilityBinding binding(const Campaign &campaign,const MapModel &map,const MapGeneratedFeature &feature)
{
    const auto bound=mapSetupBindingForFeature(map,feature);
    if(bound.present && bound.entityId.empty())
        throw std::runtime_error("A generated World visibility binding is ambiguous.");
    VisibilityBinding value;value.kind=feature.kind;value.name=feature.name;value.materialId=feature.materialId;
    value.bound=bound.present;value.boundId=bound.entityId;
    if(feature.kind==MapGeneratedFeatureKind::Nation ||
       (feature.kind==MapGeneratedFeatureKind::TerrainRegion && feature.materialId.find("territory.")!=std::string::npos)) {
        const auto *nation=matched(campaign.nations,[&](const auto &n){return bound.present?n.metadata.id==bound.entityId:n.name==feature.name;});
        if(nation){value.nationId=nation->metadata.id;value.nationVisibility=nation->visibility;}
    }
    const auto *place=matched(campaign.worldPlaces,[&](const auto &p){
        if(bound.present)return p.metadata.id==bound.entityId;
        return p.linkedMapId==map.metadata.id && ((!p.linkedFeatureId.empty()&&p.linkedFeatureId==feature.id)||
            (p.linkedFeatureId.empty()&&p.name==feature.name));
    });
    if(place){value.placeId=place->metadata.id;value.placeVisibility=place->visibility;}
    return value;
}
bool same(const VisibilityBinding &a,const VisibilityBinding &b)
{
    return std::tie(a.kind,a.name,a.materialId,a.bound,a.boundId,a.nationId,a.placeId,a.nationVisibility,a.placeVisibility)==
           std::tie(b.kind,b.name,b.materialId,b.bound,b.boundId,b.nationId,b.placeId,b.nationVisibility,b.placeVisibility);
}
}
void captureVisibility(const Campaign &before,const Campaign &after,Patch &patch)
{
    if(before.maps.size()!=after.maps.size())throw std::runtime_error("World save changed map membership.");
    for(std::size_t i=0;i<before.maps.size();++i) {
        const auto &map=before.maps[i],&next=after.maps[i];
        if(map.metadata.id!=next.metadata.id || map.generation.graph.features.size()!=next.generation.graph.features.size())
            throw std::runtime_error("World save changed map or feature membership.");
        for(std::size_t j=0;j<map.generation.graph.features.size();++j) {
            const auto &feature=map.generation.graph.features[j],&newFeature=next.generation.graph.features[j];
            if(feature.id!=newFeature.id)throw std::runtime_error("World save changed feature identity or order.");
            if(feature.gmOnly==newFeature.gmOnly && feature.discovered==newFeature.discovered)continue;
            unique(before.maps,map.metadata.id,MetadataId{});unique(after.maps,map.metadata.id,MetadataId{});available(map);available(next);
            unique(map.generation.graph.features,feature.id,FeatureId{});
            unique(next.generation.graph.features,feature.id,FeatureId{});
            patch.visibility.push_back({map.metadata.id,feature.id,feature.gmOnly,feature.discovered,
                newFeature.gmOnly,newFeature.discovered,binding(before,map,feature),binding(after,next,newFeature)});
        }
    }
}
void validateVisibility(const Campaign &campaign,const Patch &patch,bool forward,bool desired)
{
    std::set<std::pair<EntityId,EntityId>> seen;
    for(const auto &change:patch.visibility) {
        if(!seen.emplace(change.mapId,change.featureId).second)
            throw std::runtime_error("World history repeats a generated feature identity.");
        const auto &map=unique(campaign.maps,change.mapId,MetadataId{});available(map);
        const auto &feature=unique(map.generation.graph.features,change.featureId,FeatureId{});
        const bool after=desired?forward:!forward;
        if(!same(binding(campaign,map,feature),after?change.afterBinding:change.beforeBinding))
            throw std::runtime_error("A generated World feature was rebound or its source changed.");
        if(!desired && (feature.gmOnly!=(after?change.afterGmOnly:change.beforeGmOnly)||
                       feature.discovered!=(after?change.afterDiscovered:change.beforeDiscovered)))
            throw std::runtime_error("Generated World visibility changed after this edit.");
    }
}
void applyVisibility(Campaign &campaign,const Patch &patch,bool forward)
{
    for(const auto &change:patch.visibility) {
        auto &map=unique(campaign.maps,change.mapId,MetadataId{});
        auto &feature=unique(map.generation.graph.features,change.featureId,FeatureId{});
        feature.gmOnly=forward?change.afterGmOnly:change.beforeGmOnly;
        feature.discovered=forward?change.afterDiscovered:change.beforeDiscovered;
    }
}
}
