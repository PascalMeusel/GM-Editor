#include "WorldInspectorHistoryFields.hpp"

namespace WorldInspectorHistory
{
namespace
{
template<class T> void captureRows(const std::vector<T> &before,const std::vector<T> &after,
                                  const WorldInspectorDrafts::Scope &scope,std::vector<Change<T>> &out)
{
    if(before.size()!=after.size())throw std::runtime_error("World history cannot create or delete entries.");
    for(std::size_t i=0;i<before.size();++i) {
        const auto &a=before[i],&b=after[i];
        if(a.metadata.id!=b.metadata.id)throw std::runtime_error("World save changed collection identity or order.");
        if(!WorldInspectorDrafts::detail::selected(scope,a.metadata.id))continue;
        if(detail::fields(a,b).keys.empty())continue;
        detail::unique(before,a.metadata.id,detail::MetadataId{});detail::unique(after,a.metadata.id,detail::MetadataId{});
        detail::available(a);detail::available(b);
        out.push_back({a,b});
    }
}
template<class T> void applyRows(std::vector<T> &rows,const std::vector<Change<T>> &changes,bool forward)
{
    std::set<EntityId> seen;
    for(const auto &change:changes) {
        const auto &expected=forward?change.before:change.after;
        const auto &desired=forward?change.after:change.before;
        if(expected.metadata.id!=desired.metadata.id || !seen.insert(expected.metadata.id).second)
            throw std::runtime_error("World history contains inconsistent identities.");
        auto &value=detail::unique(rows,expected.metadata.id,detail::MetadataId{});
        detail::available(value);detail::available(expected);detail::available(desired);
        detail::Strict visitor;detail::visit(expected,desired,value,visitor);
    }
}
template<class T> bool rowOverlap(const std::vector<Change<T>> &changes,
                                 const std::map<EntityId,WorldInspectorDrafts::Draft<T>> &drafts)
{
    for(const auto &change:changes) {
        const auto found=drafts.find(change.before.metadata.id);if(found==drafts.end())continue;
        const auto touched=detail::fields(change.before,change.after).keys;
        const auto edited=detail::fields(found->second.base,found->second.edited).keys;
        for(const auto &key:touched)if(edited.count(key))return true;
    }
    return false;
}
template<class T> void acceptRows(std::map<EntityId,WorldInspectorDrafts::Draft<T>> &current,
                                  const std::map<EntityId,WorldInspectorDrafts::Draft<T>> &submitted,
                                  const WorldInspectorDrafts::Scope &scope)
{
    for(const auto &[id,draft]:submitted) {
        if(!WorldInspectorDrafts::detail::selected(scope,id) || detail::fields(draft.base,detail::scopedEdited(draft,scope)).keys.empty())continue;
        const auto found=current.find(id);
        if(found==current.end())throw std::runtime_error("A submitted World draft changed during save.");
        detail::acknowledge(found->second,draft,scope);
        if(!WorldInspectorDrafts::changed(found->second))current.erase(found);
    }
}
}
CaptureResult capture(const Campaign &before,const Campaign &after,const WorldInspectorDrafts::Scope &scope)
{
    CaptureResult result;
    try {
        if(before.metadata.id.empty() || before.metadata.id!=after.metadata.id)
            throw std::runtime_error("World history requires the same campaign identity.");
        captureRows(before.worldPlaces,after.worldPlaces,scope,result.patch.places);
        captureRows(before.nations,after.nations,scope,result.patch.nations);
        captureRows(before.shops,after.shops,scope,result.patch.shops);
        detail::captureVisibility(before,after,result.patch);
        // Refuse unrepresentable originals (including NaN comparison) before a
        // caller commits a change that could not safely be undone later.
        Campaign proof=before;const auto applied=apply(proof,result.patch,true);
        if(!applied.applied())throw std::runtime_error(applied.message);
        result.success=true;
    } catch(const std::exception &e){result.error=e.what();result.patch={};}
    return result;
}
WorldInspector::MutationResult apply(Campaign &campaign,const Patch &patch,bool forward)
{
    try {
        Campaign staged=campaign;
        detail::validateVisibility(staged,patch,forward,false);
        applyRows(staged.worldPlaces,patch.places,forward);
        applyRows(staged.nations,patch.nations,forward);
        applyRows(staged.shops,patch.shops,forward);
        detail::validateVisibility(staged,patch,forward,true);
        detail::applyVisibility(staged,patch,forward);
        campaign=std::move(staged);
        return {WorldInspector::MutationStatus::Applied,{},{}};
    } catch(const std::exception &e){return {WorldInspector::MutationStatus::Conflict,e.what(),{}};}
}
bool overlaps(const Patch &patch,const WorldInspectorDrafts::Store &store)
{
    try {return rowOverlap(patch.places,store.places)||rowOverlap(patch.nations,store.nations)||rowOverlap(patch.shops,store.shops);}
    catch(...){return true;}
}
bool sameDrafts(const WorldInspectorDrafts::Store &a,const WorldInspectorDrafts::Store &b,
                const WorldInspectorDrafts::Scope &scope)
{
    try {auto current=b;return acknowledgeSubmitted(current,a,scope);}
    catch(...){return false;}
}
bool acknowledgeSubmitted(WorldInspectorDrafts::Store &current,const WorldInspectorDrafts::Store &submitted,
                           const WorldInspectorDrafts::Scope &scope)
{
    try {
        auto remaining=current;
        acceptRows(remaining.places,submitted.places,scope);acceptRows(remaining.nations,submitted.nations,scope);
        acceptRows(remaining.shops,submitted.shops,scope);current=std::move(remaining);return true;
    } catch(...){return false;}
}
}
