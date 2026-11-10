#include "Models/Project/WorldDraftRecoveryFields.hpp"
#include "Models/Project/TextDraftRecovery.hpp"

namespace WorldDraftRecovery
{
namespace { constexpr const char *Magic="GM_WORLD_DRAFT_1"; }
EncodeResult encode(const EntityId &campaignId,const Store &drafts)
{
    try {
        detail::identity(campaignId,true);detail::Writer writer;
        writer.token(Magic);writer.id(campaignId);detail::store(writer,drafts);
        return {true,std::move(writer.bytes),{}};
    }catch(const std::exception &e){return {false,{},e.what()};}
}
DecodeResult decode(const EntityId &campaignId,std::string_view payload)
{
    try {
        detail::identity(campaignId,true);detail::Reader reader(payload);
        detail::check(reader.token()==Magic,"Unknown World recovery version.");
        detail::check(reader.token()==campaignId,"World recovery campaign identity does not match.");
        Store drafts;detail::store(reader,drafts);
        detail::check(reader.offset==payload.size(),"Trailing World recovery data.");
        return {true,std::move(drafts),{}};
    }catch(const std::exception &e){return {false,{},e.what()};}
}
ReadResult read(const Campaign &campaign)
{
    ReadResult result;
    try {
        auto outer=TextDraftRecovery::read(campaign,Key,true);
        result.rawCheckpoint=std::move(outer.rawBytes);
        result.rawAvailable=outer.rawAvailable;
        if(!outer.success) {
            result.status=result.rawAvailable?ReadStatus::Invalid:ReadStatus::Unavailable;
            result.error=std::move(outer.error);return result;
        }
        if(!outer.found){result.status=ReadStatus::Absent;return result;}
        result.status=ReadStatus::Invalid;
        if(outer.fields.size()!=1||outer.fields.count("world")!=1)
        {result.error="World recovery fields do not match the supported version.";return result;}
        auto decoded=decode(campaign.metadata.id,outer.fields.at("world"));
        if(!decoded.success){result.error=std::move(decoded.error);return result;}
        result.drafts=std::move(decoded.drafts);result.status=ReadStatus::Ready;
    }catch(const std::exception &e){result.error=e.what();}
    return result;
}
WriteResult save(const Campaign &campaign,const Store &drafts)
{
    if(drafts.empty())return remove(campaign);
    auto encoded=encode(campaign.metadata.id,drafts);
    if(!encoded.success)return {false,std::move(encoded.error)};
    const auto error=TextDraftRecovery::save(campaign,Key,{{"world",std::move(encoded.payload)}});
    return {error.empty(),error};
}
WriteResult remove(const Campaign &campaign)
{const auto error=TextDraftRecovery::remove(campaign,Key);return {error.empty(),error};}
CopyResult copyCheckpoint(const ReadResult &pending)
{
    try {
        detail::check(pending.rawAvailable,"Complete recovery bytes are unavailable for copying.");
        detail::check(pending.rawCheckpoint.size()<=8U*1024U*1024U,"Recovery copy exceeds the size limit.");
        constexpr const char *alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out="GM Editor private World recovery checkpoint (Base64; not an automatic Restore format)\n";
        for(std::size_t i=0;i<pending.rawCheckpoint.size();i+=3) {
            const auto remaining=pending.rawCheckpoint.size()-i;
            const auto a=static_cast<unsigned char>(pending.rawCheckpoint[i]);
            const auto b=remaining>1?static_cast<unsigned char>(pending.rawCheckpoint[i+1]):0u;
            const auto c=remaining>2?static_cast<unsigned char>(pending.rawCheckpoint[i+2]):0u;
            out+=alphabet[a>>2];out+=alphabet[((a&3u)<<4)|(b>>4)];
            out+=remaining>1?alphabet[((b&15u)<<2)|(c>>6)]:'=';out+=remaining>2?alphabet[c&63u]:'=';
        }
        return {true,std::move(out),{}};
    }catch(const std::exception &e){return {false,{},e.what()};}
}
}
