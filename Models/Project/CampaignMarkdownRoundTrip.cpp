#include "CampaignMarkdownCapture.hpp"
#include "CampaignMarkdownNotes.hpp"
#include "CampaignMarkdownIo.hpp"
#include <set>

namespace CampaignMarkdownExchange
{
namespace {
const StoryThreadModel *uniqueNote(const Campaign &c,const EntityId &id)
{
    const StoryThreadModel *found=nullptr;
    for(const auto &n:c.storyThreads)if(n.metadata.id==id){if(found)return nullptr;found=&n;}
    return found&&!detail::archived(found->metadata)?found:nullptr;
}
}
ExportInput captureExport(const Campaign &c,const Limits &limits)
{
    detail::captureBudget(c,limits);
    ExportInput result;result.campaignId=c.metadata.id;detail::safeId(result.campaignId);
    // Editable companions have stricter raw-text limits than reading copies.
    // Refuse that option explicitly without rejecting the existing export.
    try {
    std::size_t raw=0,notes=0;
    std::set<EntityId> ids;
    for(const auto &n:c.storyThreads) {
        detail::require(ids.insert(n.metadata.id).second,"Duplicate story-thread IDs prevent note exchange.");
        detail::safeId(n.metadata.id);
        if(detail::archived(n.metadata))continue;
        detail::require(++notes<=limits.notes&&n.title.size()<=4096&&n.gmNotes.size()<=limits.noteBytes,"Editable notes are limited to 10,000 rows, 4 KiB titles and 4 MiB per note.");
        detail::require(n.gmNotes.size()<=(limits.rawBytes-raw)/2,"Editable base and edited text exceed the 64 MiB aggregate limit.");raw+=n.gmNotes.size()*2;
        detail::require(detail::utf8(n.gmNotes),"Editable notes require UTF-8 without NUL bytes.");
    }
    }catch(const std::exception &e){result.editableError=e.what();}
    result.assetsRoot=std::filesystem::absolute(ProjectPackageLayout::layoutForCampaign(c).assetsDirectory).lexically_normal();
    result.storageRoot=std::filesystem::absolute(ProjectPackageLayout::campaignsDirectory()).lexically_normal();
    result.documents=detail::documents(c);
    if(result.editableError.empty())for(const auto &n:c.storyThreads)if(!detail::archived(n.metadata))result.editableNotes.push_back({n.metadata.id,n.title,n.gmNotes});
    return result;
}
bool sameExport(const ExportInput &a,const ExportInput &b)
{
    if(a.campaignId!=b.campaignId||a.assetsRoot!=b.assetsRoot||a.storageRoot!=b.storageRoot||a.editableError!=b.editableError||
       !detail::sameDocuments(a.documents,b.documents)||a.editableNotes.size()!=b.editableNotes.size())return false;
    for(std::size_t i=0;i<a.editableNotes.size();++i) {
        const auto &x=a.editableNotes[i],&y=b.editableNotes[i];if(x.id!=y.id||x.title!=y.title||x.text!=y.text)return false;
    }return true;
}
std::vector<UpdateRow> classifyUpdates(const Campaign &c,const UpdatePreview &preview)
{
    const Limits limits;detail::require(preview.entries.size()<=limits.notes,"Update preview exceeds 10,000 notes.");
    std::size_t currentBytes=0;
    std::vector<UpdateRow> result;const auto notes=detail::noteIndex(c);std::map<EntityId,std::size_t> counts;
    for(const auto &entry:preview.entries)++counts[entry.id];
    for(const auto &entry:preview.entries) {
        UpdateRow row;row.id=entry.id;row.title=entry.title;
        const auto *note=detail::indexed(notes,entry.id);
        const bool duplicate=counts[entry.id]!=1;
        if(!preview.success||preview.campaignId!=c.metadata.id||entry.id.empty()||duplicate||!note)
            row.reason="The campaign or unique unarchived story thread is unavailable.";
        else if(note->gmNotes.size()>limits.noteBytes||note->gmNotes.size()>limits.rawBytes-currentBytes||note->title.size()>4096)
            row.reason="Current note text exceeds the review limit (4 MiB per note, 64 MiB total, 4 KiB title). No current text was copied.";
        else {
            currentBytes+=note->gmNotes.size();
            row.title=note->title;row.currentText=note->gmNotes;
            if(entry.editedText==entry.baseText)row.state=UpdateState::Unchanged;
            else if(entry.editedText==note->gmNotes)row.state=UpdateState::AlreadyApplied;
            else if(entry.baseText==note->gmNotes)row.state=UpdateState::Eligible;
            else {row.state=UpdateState::Conflict;row.reason="The current GM note differs from the exported baseline.";}
        }
        result.push_back(std::move(row));
    }return result;
}
ApplyResult applyUpdates(Campaign &c,const UpdatePreview &preview,const std::vector<EntityId> &selected,
                         const DurableSave &persist,const CurrentContext &current)
{
    ApplyResult result;bool durable=false;
    try {
        detail::require(current&&current()&&persist,"The note update context is no longer available.");
        detail::require(preview.success&&preview.campaignId==c.metadata.id,"Review updates for this campaign first.");
        auto rows=classifyUpdates(c,preview);std::set<EntityId> ids;
        struct Change {EntityId id;std::string before,after,stamp,oldStamp;std::map<std::string,std::string> provenance,prior,merged;};std::vector<Change> changes;
        std::map<EntityId,const UpdateRow *> byId;std::map<EntityId,const UpdateEntry *> entries;
        for(const auto &row:rows)byId[row.id]=&row;for(const auto &entry:preview.entries)entries[entry.id]=&entry;
        for(const auto &id:selected) {
            detail::require(ids.insert(id).second,"A note was selected more than once.");
            const auto found=byId.find(id);const auto *r=found==byId.end()?nullptr:found->second;
            detail::require(r&&(r->state==UpdateState::Eligible||r->state==UpdateState::AlreadyApplied),"A selected note conflicts or is unavailable. Review again.");
            if(r->state==UpdateState::AlreadyApplied)continue;
            const auto *n=entries.at(id);Change change;change.id=id;change.before=r->currentText;change.after=n->editedText;
            change.provenance={{"gm.markdown-update.base-hash",detail::digest(n->baseText)},{"gm.markdown-update.edited-hash",detail::digest(n->editedText)},
                {"gm.markdown-update.source-directory",preview.sourceDirectory.u8string()},{"gm.markdown-update.format","raw-story-thread-v1"}};
            changes.push_back(std::move(change));result.noteIds.push_back(id);
        }
        if(changes.empty()){result.state=ApplyState::NoChange;return result;}
        const auto oldCampaignStamp=c.metadata.updatedAtUtc;Campaign staged=c;const auto stagedIndex=detail::noteIndex(staged);
        for(auto &change:changes){auto *n=const_cast<StoryThreadModel *>(detail::indexed(stagedIndex,change.id));change.oldStamp=n->metadata.updatedAtUtc;n->gmNotes=change.after;
            for(const auto &[key,value]:change.provenance){const auto old=n->metadata.customProperties.find(key);
                if(old!=n->metadata.customProperties.end())change.prior.emplace(*old);n->metadata.customProperties[key]=value;}
            n->metadata.touch();change.stamp=n->metadata.updatedAtUtc;}
        detail::require(current(),"The note update context changed.");
        const auto saved=persist(staged);durable=saved.committed();
        detail::require(durable,"Selected notes could not be saved. The reviewed edits are retained.");
        if(!current())throw std::runtime_error("Saved, but the campaign context changed. Review the current campaign before continuing.");
        const auto live=detail::noteIndex(c);
        for(auto &change:changes){const auto *n=detail::indexed(live,change.id);if(!n||(n->gmNotes!=change.before&&n->gmNotes!=change.after))throw std::runtime_error("Saved, but a selected note changed during save. Newer live text was retained.");
            for(const auto &[key,value]:change.provenance){const auto now=n->metadata.customProperties.find(key);const auto before=change.prior.find(key);
                const bool unchanged=before==change.prior.end()?now==n->metadata.customProperties.end():now!=n->metadata.customProperties.end()&&now->second==before->second;
                if(!unchanged&&(now==n->metadata.customProperties.end()||now->second!=value))throw std::runtime_error("Saved, but note update provenance changed during save. Newer live state was retained.");}
            change.merged=n->metadata.customProperties;for(const auto &[key,value]:change.provenance)change.merged[key]=value;}
        for(auto &change:changes){auto *n=const_cast<StoryThreadModel *>(detail::indexed(live,change.id));n->gmNotes.swap(change.after);n->metadata.customProperties.swap(change.merged);
            if(n->metadata.updatedAtUtc==change.oldStamp)n->metadata.updatedAtUtc.swap(change.stamp);}
        if(c.metadata.updatedAtUtc==oldCampaignStamp)c.metadata.updatedAtUtc.swap(staged.metadata.updatedAtUtc);
        result.state=ApplyState::Applied;return result;
    }catch(const std::exception &e){result.error=e.what();result.state=durable?ApplyState::CommittedContextChanged:ApplyState::NotCommitted;return result;}
}
ApplyResult applyImportedNote(Campaign &c,const ImportPreview &preview,const DurableSave &persist,const CurrentContext &current)
{
    ApplyResult result;bool durable=false;
    try {
        detail::require(current&&current()&&persist&&preview.success&&preview.campaignId==c.metadata.id&&!preview.noteId.empty(),"Review a file for this campaign first.");
        constexpr auto provenance="gm.markdown-import.original-text";
        for(const auto &n:c.storyThreads)if(n.metadata.id==preview.noteId) {
            const auto p=n.metadata.customProperties.find(provenance);
            detail::require(uniqueNote(c,n.metadata.id)==&n&&p!=n.metadata.customProperties.end()&&p->second==preview.text,"The import note identity is unavailable or already used.");
            result.state=ApplyState::NoChange;return result;
        }
        detail::require(!CampaignTags::findEntityMetadata(c,preview.noteId),"The import identity is already used.");
        const auto oldCampaignStamp=c.metadata.updatedAtUtc;Campaign staged=c;StoryThreadModel note;note.metadata.id=preview.noteId;note.title=preview.title;note.type=StoryThreadType::WorldStory;note.status=StoryStatus::Idea;note.gmNotes=preview.text;
        note.metadata.customProperties={{provenance,preview.text},{"gm.markdown-import.source-name",preview.sourceFileName},{"gm.markdown-import.source-fingerprint",preview.contentHash},{"gm.markdown-import.format","utf8-markdown-v1"}};
        staged.storyThreads.push_back(note);result.noteIds={note.metadata.id};
        const auto saved=persist(staged);durable=saved.committed();detail::require(durable,"The new note could not be saved. Preview retained.");
        if(!current()||CampaignTags::findEntityMetadata(c,note.metadata.id))throw std::runtime_error("Saved, but the campaign or note identity changed during save. Newer state was retained.");
        c.storyThreads.push_back(std::move(note));
        if(c.metadata.updatedAtUtc==oldCampaignStamp)c.metadata.updatedAtUtc.swap(staged.metadata.updatedAtUtc);
        result.state=ApplyState::Applied;return result;
    }catch(const std::exception &e){result.error=e.what();result.state=durable?ApplyState::CommittedContextChanged:ApplyState::NotCommitted;return result;}
}
}
