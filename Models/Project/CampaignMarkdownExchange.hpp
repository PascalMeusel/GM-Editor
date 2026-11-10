#pragma once
#include "Models/Session/SessionPlanService.hpp"
#include <filesystem>

namespace CampaignMarkdownExchange
{
struct Document
{
    EntityId id;
    std::string type, title, relativePath, markdown;
};
struct Attachment
{
    std::string relativePath, contentHash;
    std::uintmax_t byteSize = 0;
};
struct ExportPreview
{
    bool success = false;
    std::string error;
    std::string editableError;
    EntityId campaignId;
    std::vector<Document> documents;
    std::vector<Attachment> attachments;
    std::vector<std::string> warnings;
};
struct ImportPreview
{
    bool success = false;
    std::string error, title, text, sourceFileName, contentHash;
    std::filesystem::path sourcePath;
    EntityId campaignId, noteId = createEntityId("story_thread");
};
struct Result
{
    bool success = false, changed = false;
    std::string error;
    EntityId noteId;
    std::filesystem::path destination;
};

// GM/private export. Attachment inventory is confined to managed asset buckets;
// nothing is copied until the caller deliberately selects relative paths.
ExportPreview previewExport(const Campaign &);
Result exportNotes(const Campaign &, const ExportPreview &, const std::filesystem::path &destination,
                   const std::vector<std::string> &selectedAttachments = {});
// Exact UTF-8 bytes become a private story thread's GM notes. This is a prose
// import, not an entity/schema restore; links/frontmatter are retained as text.
ImportPreview previewImport(const Campaign &, const std::filesystem::path &);
Result importNote(Campaign &, const ImportPreview &, const SessionPlan::Persist &);
}
