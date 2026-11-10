#pragma once
#include <cctype>
#include "CampaignMarkdownRoundTrip.hpp"
#include <algorithm>
#include <map>

namespace CampaignMarkdownExchange::detail
{
inline bool archived(const EntityMetadata &metadata)
{
    for(const auto &tag:metadata.tags){std::size_t first=0,last=tag.size();
        while(first<last&&std::isspace(static_cast<unsigned char>(tag[first])))++first;
        while(last>first&&std::isspace(static_cast<unsigned char>(tag[last-1])))--last;
        if(last-first!=8)continue;
        constexpr auto word="archived";bool match=true;for(std::size_t i=0;i<8;++i)match=match&&std::tolower(static_cast<unsigned char>(tag[first+i]))==word[i];
        if(match)return true;
    }return false;
}
inline std::map<EntityId,const StoryThreadModel *> noteIndex(const Campaign &c)
{
    std::map<EntityId,const StoryThreadModel *> result;
    for(const auto &note:c.storyThreads){const auto inserted=result.emplace(note.metadata.id,&note);if(!inserted.second)inserted.first->second=nullptr;}
    for(auto &[id,note]:result)if(note&&archived(note->metadata))note=nullptr;
    return result;
}
inline const StoryThreadModel *indexed(const std::map<EntityId,const StoryThreadModel *> &index,const EntityId &id)
{const auto found=index.find(id);return found==index.end()?nullptr:found->second;}
}
