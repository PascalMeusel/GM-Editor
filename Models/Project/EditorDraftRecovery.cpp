#include "Models/Project/EditorDraftRecoveryCodec.hpp"
#include "Models/Project/TextDraftRecovery.hpp"

namespace EditorDraftRecovery
{
EncodeResult encode(const EntityId &campaignId,const Store &drafts)
{
    try { detail::Writer w;std::string magic="GM_EDITOR_DRAFT_1",id=campaignId;w.text(magic);detail::identity(w,id);
        auto copy=drafts;detail::store(w,copy);return {true,std::move(w.bytes),{}};
    } catch(const std::exception &e){return {false,{},e.what()};}
}
DecodeResult decode(const EntityId &campaignId,std::string_view bytes)
{
    try { detail::Reader r(bytes);std::string magic,id;r.text(magic);detail::identity(r,id);
        detail::check(magic=="GM_EDITOR_DRAFT_1"&&id==campaignId,"Draft version or campaign identity differs.");
        Store value;detail::store(r,value);detail::check(r.offset==bytes.size(),"Trailing draft recovery bytes.");
        return {true,std::move(value),{}};
    } catch(const std::exception &e){return {false,{},e.what()};}
}
ReadResult read(const Campaign &campaign)
{
    ReadResult result;auto outer=TextDraftRecovery::read(campaign,Key,true);
    result.rawBytes=std::move(outer.rawBytes);result.rawAvailable=outer.rawAvailable;
    if(!outer.success) {result.status=result.rawAvailable?ReadStatus::Invalid:ReadStatus::Unavailable;result.error=outer.error;return result;}
    if(!outer.found){result.status=ReadStatus::Absent;return result;}
    result.status=ReadStatus::Invalid;
    if(outer.fields.size()!=1||outer.fields.count("drafts")!=1){result.error="Unsupported editor recovery fields.";return result;}
    auto decoded=decode(campaign.metadata.id,outer.fields.at("drafts"));
    if(!decoded.success){result.error=decoded.error;return result;}
    result.drafts=std::move(decoded.drafts);result.status=ReadStatus::Ready;return result;
}
std::string save(const Campaign &campaign,const Store &drafts)
{
    if(drafts.empty())return TextDraftRecovery::remove(campaign,Key);
    const auto encoded=encode(campaign.metadata.id,drafts);
    return encoded.success?TextDraftRecovery::save(campaign,Key,{{"drafts",encoded.payload}}):encoded.error;
}
bool sameItem(const ItemModel &a,const ItemModel &b)
{
    try {
        const auto bytes=[](ItemModel item) {
            item.metadata.createdAtUtc.clear();item.metadata.updatedAtUtc.clear();
            for(const char *key:{"gm.context.place","gm.context.faction","gm.context.shop"})item.metadata.customProperties.erase(key);
            detail::Writer w;detail::Budget budget;detail::item(w,item,budget);return w.bytes;
        };
        return bytes(a)==bytes(b);
    } catch(...){return false;}
}
std::string copyCheckpoint(const ReadResult &read)
{
    detail::check(read.rawAvailable,"Complete checkpoint bytes are unavailable.");
    detail::check(read.rawBytes.size()<=8u*1024u*1024u,"Checkpoint exceeds the copy limit.");
    constexpr const char *alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    if(read.status==ReadStatus::Ready) {
        const auto append=[&](const char *label,const std::string &value) {
            out+=label;out+='\n';for(char c:value){if(c=='\0')out+="\\0";else out+=c;}out+="\n\n";
        };
        out="GM Editor private unfinished drafts — readable excerpt; complete fields follow as Base64.\n\n";
        for(const auto &[id,draft]:read.drafts.items) {
            append("Item ID",id);append("Item name",draft.item.name);append("Description",draft.item.description);
            if(draft.powerDirty){append("Unfinished power name",draft.power.name);append("Unfinished power notes",draft.power.notes);}
        }
        if(read.drafts.notes) {const auto &n=*read.drafts.notes;append("Character ID",n.characterId);append("Backstory",n.backstory);
            append("Player notes",n.playerNotes);append("Private GM notes",n.gmNotes);}
    }
    out+="GM Editor private item/character-note checkpoint (Base64; not an automatic import format)\n";
    for(std::size_t i=0;i<read.rawBytes.size();i+=3) {
        const auto remaining=read.rawBytes.size()-i;
        const auto a=static_cast<unsigned char>(read.rawBytes[i]);
        const auto b=remaining>1?static_cast<unsigned char>(read.rawBytes[i+1]):0u;
        const auto c=remaining>2?static_cast<unsigned char>(read.rawBytes[i+2]):0u;
        out+=alphabet[a>>2];out+=alphabet[((a&3u)<<4)|(b>>4)];
        out+=remaining>1?alphabet[((b&15u)<<2)|(c>>6)]:'=';out+=remaining>2?alphabet[c&63u]:'=';
    }
    return out;
}
}
