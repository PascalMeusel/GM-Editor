#pragma once
#include "Models/Campaign/Campaign.hpp"
#include <optional>
#include <map>
#include <string_view>

// Private, uncommitted item drafts and character prose only. Never auto-applied.
namespace EditorDraftRecovery
{
inline constexpr const char *Key = "items-character-notes-v1";
inline constexpr std::size_t MaxPayload = 7U * 1024U * 1024U;
struct ItemDraft {
    ItemModel item;
    std::optional<ItemModel> base; // absent only for an explicitly new item
    MonsterActionModel power;
    bool powerDirty = false;
};
struct Notes {
    EntityId characterId;
    std::string backstory, playerNotes, gmNotes;
    std::string savedBackstory, savedPlayerNotes, savedGmNotes;
    bool dirty() const { return backstory!=savedBackstory || playerNotes!=savedPlayerNotes || gmNotes!=savedGmNotes; }
};
struct Store {
    std::map<EntityId, ItemDraft> items;
    std::optional<Notes> notes;
    bool empty() const { return items.empty() && !notes; }
};
struct EncodeResult { bool success=false; std::string payload,error; };
struct DecodeResult { bool success=false; Store drafts; std::string error; };
enum class ReadStatus { Absent, Ready, Invalid, Unavailable };
struct ReadResult {
    ReadStatus status=ReadStatus::Unavailable;
    Store drafts;
    std::string rawBytes,error;
    bool rawAvailable=false;
};
EncodeResult encode(const EntityId &,const Store &);
DecodeResult decode(const EntityId &,std::string_view);
ReadResult read(const Campaign &);
std::string save(const Campaign &,const Store &); // empty error acknowledges storage
std::string copyCheckpoint(const ReadResult &); // complete bytes, Base64; throws if unavailable
// Ignores bookkeeping timestamps and externally managed context links only.
bool sameItem(const ItemModel &,const ItemModel &);
}
