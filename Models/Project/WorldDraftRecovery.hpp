#pragma once
#include "Models/Project/WorldInspectorDrafts.hpp"
#include <string_view>

// Private checkpoint transport only. Reading never restores or applies drafts.
namespace WorldDraftRecovery
{
inline constexpr const char *Key = "world-inspector-v1";
inline constexpr std::size_t MaxPayloadBytes = 7U * 1024U * 1024U;
inline constexpr std::size_t MaxTextBytes = 2U * 1024U * 1024U;
inline constexpr std::size_t MaxIdentityBytes = 512;
inline constexpr std::size_t MaxParents = 1024, MaxChildren = 8192, MaxPoints = 65536;
using Store = WorldInspectorDrafts::Store;
struct EncodeResult { bool success=false; std::string payload,error; };
struct DecodeResult { bool success=false; Store drafts; std::string error; };
enum class ReadStatus { Absent, Ready, Invalid, Unavailable };
struct ReadResult {
    ReadStatus status=ReadStatus::Unavailable;
    Store drafts;
    std::string rawCheckpoint,error;
    bool rawAvailable=false;
};
struct WriteResult { bool success=false; std::string error; };
struct CopyResult { bool success=false; std::string text,error; };
EncodeResult encode(const EntityId &campaignId,const Store &);
DecodeResult decode(const EntityId &campaignId,std::string_view payload);
ReadResult read(const Campaign &);
// Caller reconciles pending recovery first. Empty Store removes only this key.
// Failure never acknowledges RAM or replaces a previous usable checkpoint.
WriteResult save(const Campaign &,const Store &);
WriteResult remove(const Campaign &);
CopyResult copyCheckpoint(const ReadResult &);
}
