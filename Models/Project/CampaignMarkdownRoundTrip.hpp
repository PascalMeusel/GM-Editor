#pragma once
#include "CampaignMarkdownExchange.hpp"
#include "ProjectSaveService.hpp"
#include <atomic>
#include <functional>
#include <memory>

namespace CampaignMarkdownExchange
{
struct Limits {
    std::size_t documents=20000, notes=10000, noteBytes=4u*1024u*1024u;
    std::size_t documentBytes=64u*1024u*1024u, rawBytes=64u*1024u*1024u, manifestBytes=8u*1024u*1024u;
};
struct EditableNote { EntityId id; std::string title,text; };
struct ExportInput {
    EntityId campaignId;
    std::vector<Document> documents;
    std::vector<EditableNote> editableNotes;
    std::string editableError;
    std::filesystem::path assetsRoot,storageRoot;
};
struct UpdateEntry { EntityId id; std::string title,baseText,editedText,baseHash; };
struct ReviewedFile { std::string relativePath,bytes; };
struct UpdatePreview {
    bool success=false;
    std::string error;
    EntityId campaignId;
    std::filesystem::path sourceDirectory;
    std::vector<UpdateEntry> entries;
    std::vector<ReviewedFile> files;
};
enum class UpdateState { Unchanged,AlreadyApplied,Eligible,Conflict,Unavailable };
struct UpdateRow { EntityId id; std::string title,currentText,reason; UpdateState state=UpdateState::Unavailable; };
enum class ApplyState { NotCommitted,NoChange,Applied,CommittedContextChanged };
struct ApplyResult {
    ApplyState state=ApplyState::NotCommitted;
    std::string error;
    std::vector<EntityId> noteIds;
    bool committed() const {return state==ApplyState::Applied||state==ApplyState::CommittedContextChanged;}
};
using DurableSave=std::function<ProjectSaveResult(Campaign &)>;
using CurrentContext=std::function<bool()>;
// Pure main-thread capture/classification. Limits are checked before copies.
ExportInput captureExport(const Campaign &,const Limits & = {});
bool sameExport(const ExportInput &,const ExportInput &);
std::vector<UpdateRow> classifyUpdates(const Campaign &,const UpdatePreview &);
// Caller has revalidated reviewed files; these functions do no filesystem I/O.
// The context callback is mandatory and is checked before and after durability.
ApplyResult applyUpdates(Campaign &,const UpdatePreview &,const std::vector<EntityId> &,
                         const DurableSave &,const CurrentContext &);
ApplyResult applyImportedNote(Campaign &,const ImportPreview &,const DurableSave &,const CurrentContext &);
}
