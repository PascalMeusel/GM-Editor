#pragma once
#include "CampaignMarkdownRoundTrip.hpp"
#include "CampaignMarkdownDocuments.hpp"
#include "CampaignMarkdownFiles.hpp"
#include "CampaignTagService.hpp"

namespace CampaignMarkdownExchange::detail
{
// Conservative formatted-size pass over references, before any prose is copied.
// A factor of four covers escaping/code fences; fixed overhead covers headings.
inline void captureBudget(const Campaign &c,const Limits &limits)
{
    std::size_t bytes=0,count=0;
    const auto add=[&](std::size_t n){require(n<=limits.documentBytes-bytes,"GM export exceeds the estimated 64 MiB formatted-text budget.");bytes+=n;};
    const auto text=[&](const std::string &s){require(s.size()<=(limits.documentBytes-bytes)/4,"GM export exceeds the estimated 64 MiB formatted-text budget.");add(s.size()*4);};
    const auto row=[&](std::initializer_list<const std::string *> fields){require(count++<limits.documents,"GM export exceeds 20,000 documents.");add(1024);for(auto *s:fields)text(*s);};
    const auto links=[&](const auto &entries){for(const auto &v:entries){add(128);text(v.label);text(v.targetEntityType);text(v.targetEntityId);}};
    row({&c.metadata.id,&c.name,&c.description});
    for(const auto &v:c.storyThreads) {
        row({&v.metadata.id,&v.title,&v.summary,&v.stakes,&v.gmNotes});links(v.links);
        for(const auto &b:v.beats){row({&b.metadata.id,&b.title,&v.metadata.id,&b.summary,&b.playerSummary,&b.readAloudText,&b.clues,&b.consequences,&b.gmNotes});links(b.links);}
    }
    for(const auto &v:c.sessions) {
        row({&v.metadata.id,&v.title,&v.roughNotes,&v.summary,&v.recap,&v.unresolvedThreads,&v.openHooks,&v.lootAwarded,&v.xpMilestones,&v.npcsMet,&v.locationsVisited});
        for(const auto &p:v.planEntries){add(128);text(p.storyBeatId);text(p.prepNote);}
        for(const auto &n:v.notes)row({&n.metadata.id,&n.timestampUtc,&v.metadata.id,&n.body,&n.linkedEntityId});
        // The outcome codec is bounded, but reject oversized encoded values
        // before decoding/copying any ledger used by the document builder.
        for(const auto &[key,value]:v.metadata.customProperties) {add(key.size());add(value.size());}
        const auto outcomes=SessionOutcomes::read(v);require(outcomes.success,"Session outcomes cannot be exported.");
        for(const auto &f:outcomes.ledger.followUps){add(128);text(f.privateText);}
        for(const auto &o:outcomes.ledger.outcomes)row({&o.id,&v.metadata.id,&o.privateText,&o.publicText,&o.sourceSceneId,&o.sourceNoteId,&o.target.id,&o.changeText});
    }
    for(const auto &v:c.npcs)row({&v.metadata.id,&v.name,&v.description,&v.role,&v.attitudeTowardParty,&v.secrets,&v.goals,&v.relationships,&v.dialogueNotes});
    for(const auto &v:c.worldPlaces)row({&v.metadata.id,&v.name,&v.description,&v.notes});
    for(const auto &v:c.storyObjectives)row({&v.metadata.id,&v.title,&v.playerSummary,&v.gmNotes});
    for(const auto &v:c.storyClues)row({&v.metadata.id,&v.title,&v.playerText,&v.gmTruth});
    for(const auto &v:c.handouts)row({&v.metadata.id,&v.title,&v.body,&v.gmNotes});
    for(const auto &v:c.timelineEvents)row({&v.metadata.id,&v.title,&v.inWorldDate,&v.description});
    for(const auto &v:c.factions)row({&v.metadata.id,&v.name,&v.summary,&v.goals,&v.gmNotes});
    for(const auto &v:c.deities)row({&v.metadata.id,&v.name,&v.domains,&v.tenets,&v.gmNotes});
    for(const auto &v:c.nations)row({&v.metadata.id,&v.name,&v.description,&v.culture,&v.relationships});
}
}
