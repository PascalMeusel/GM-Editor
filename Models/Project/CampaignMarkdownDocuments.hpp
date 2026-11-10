#pragma once
#include "Models/Project/CampaignMarkdownExchange.hpp"
#include "Models/Session/SessionOutcomeService.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace CampaignMarkdownExchange::detail
{
inline std::string safeId(const std::string &id)
{
    if (id.empty() || id.size() > 96) throw std::runtime_error("A note has an empty or oversized stable ID.");
    static const char hex[] = "0123456789abcdef";
    std::string result;
    for (unsigned char ch : id) { result += hex[ch >> 4]; result += hex[ch & 15]; }
    return result;
}
inline std::string inlineText(const std::string &value)
{
    std::string result;
    for (char ch : value)
    {
        if (ch == '\r' || ch == '\n' || ch == '\t') result += ' ';
        else { if (std::string("\\`*_{}[]<>()!#|").find(ch) != std::string::npos) result += '\\'; result += ch; }
    }
    return result;
}
inline std::string section(const char *name, const std::string &text)
{ return text.empty() ? std::string{} : "\n## " + std::string(name) + "\n\n" + text + "\n"; }
inline std::string codeId(const std::string &id, bool tableCell = false)
{
    // Code spans already preserve underscores/backslashes literally. Applying
    // prose escaping here changes the displayed ID and breaks copy/paste.
    std::size_t longest = 0, run = 0;
    std::string content;
    for (const char ch : id)
    {
        run = ch == '`' ? run + 1 : 0; longest = std::max(longest, run);
        // GFM table parsing consumes this escape before parsing the code span.
        if (tableCell && ch == '|') content += '\\';
        content += ch;
    }
    const std::string fence(longest + 1, '`');
    const bool pad = !id.empty() && id.find_first_not_of(' ') != std::string::npos &&
        (id.front() == '`' || id.back() == '`' || id.front() == ' ' || id.back() == ' ');
    return fence + (pad ? " " : "") + content + (pad ? " " : "") + fence;
}
inline std::string links(const std::vector<StoryLinkModel> &values)
{
    std::string text;
    for (const auto &v : values) text += "- " + inlineText(v.label) + " (" + storyLinkTypeLabel(v.type) + ") → " +
        inlineText(v.targetEntityType) + " " + codeId(v.targetEntityId) + "\n";
    return section("Linked stable IDs (resolve in INDEX.md)", text);
}
inline std::vector<Document> documents(const Campaign &c)
{
    std::vector<Document> result;
    std::set<std::string> ids;
    std::size_t total = 0;
    auto add = [&](const EntityId &id, const char *type, const std::string &title, const std::string &body) {
        if (!ids.insert(id).second) throw std::runtime_error("Duplicate stable note ID; export stopped.");
        Document d{id, type, title, "notes/" + std::string(type) + "-" + safeId(id) + ".md", {}};
        d.markdown = "# " + inlineText(title) + "\n\nStable ID: " + codeId(id) + "\n\nType: " + type +
            "\n\nGM/private campaign notes.\n" + body;
        total += d.markdown.size();
        if (result.size() >= 20000 || total > 64U * 1024 * 1024) throw std::runtime_error("Campaign notes exceed the export preview limit.");
        result.push_back(std::move(d));
    };
    add(c.metadata.id, "campaign", c.name, section("Description", c.description));
    for (const auto &v : c.storyThreads)
    {
        add(v.metadata.id, "story-thread", v.title, section("Summary", v.summary) + section("Stakes", v.stakes) + section("GM notes", v.gmNotes) + links(v.links));
        for (const auto &b : v.beats)
            add(b.metadata.id, "scene", b.title, section("Story thread ID", v.metadata.id) + section("Summary", b.summary) +
                section("Player summary", b.playerSummary) + section("Read aloud", b.readAloudText) + section("Clues", b.clues) +
                section("Consequences", b.consequences) + section("GM notes", b.gmNotes) + links(b.links));
    }
    for (const auto &v : c.sessions)
    {
        std::string body = section("Preparation", v.roughNotes) + section("Summary", v.summary) + section("Recap", v.recap) +
            section("Unresolved threads", v.unresolvedThreads) + section("Open hooks", v.openHooks) + section("Loot", v.lootAwarded) +
            section("XP and milestones", v.xpMilestones) + section("NPCs met", v.npcsMet) + section("Locations visited", v.locationsVisited);
        for (const auto &entry : v.planEntries) body += section("Planned scene ID", entry.storyBeatId) + section("Scene preparation", entry.prepNote);
        const auto outcomes = SessionOutcomes::read(v);
        if (!outcomes.success) throw std::runtime_error("Session outcomes cannot be exported: " + outcomes.error);
        for (const auto &follow : outcomes.ledger.followUps) body += section("Carried follow-up", follow.privateText);
        add(v.metadata.id, "session", v.title, body);
        for (const auto &n : v.notes) add(n.metadata.id, "session-note", n.timestampUtc,
            section("Session ID", v.metadata.id) + section("Text", n.body) + section("Linked entity ID", n.linkedEntityId));
        for (const auto &o : outcomes.ledger.outcomes) add(o.id, "outcome", "Session outcome",
            section("Review state", o.state == SessionOutcomes::State::Applied ? "Applied" : o.state == SessionOutcomes::State::Rejected ? "Rejected" : "Pending review") +
            section("Session ID", v.metadata.id) + section("Private text", o.privateText) + section("Explicit player text", o.publicText) +
            section("Source scene ID", o.sourceSceneId) + section("Source note ID", o.sourceNoteId) +
            section("Target ID", o.target.id) + section("Proposed change", o.changeText));
    }
    for (const auto &v : c.npcs) add(v.metadata.id, "npc", v.name, section("Description", v.description) + section("Role", v.role) +
        section("Attitude", v.attitudeTowardParty) + section("Secrets", v.secrets) + section("Goals", v.goals) +
        section("Relationships", v.relationships) + section("Dialogue notes", v.dialogueNotes));
    for (const auto &v : c.worldPlaces) add(v.metadata.id, "place", v.name, section("Description", v.description) + section("GM notes", v.notes));
    for (const auto &v : c.storyObjectives) add(v.metadata.id, "objective", v.title,
        section("Player summary", v.playerSummary) + section("GM notes", v.gmNotes) + section("Status", std::to_string(static_cast<int>(v.status))));
    for (const auto &v : c.storyClues) add(v.metadata.id, "clue", v.title,
        section("Player text", v.playerText) + section("GM truth", v.gmTruth) + section("Status", std::to_string(static_cast<int>(v.status))));
    for (const auto &v : c.handouts) add(v.metadata.id, "handout", v.title, section("Body", v.body) + section("GM notes", v.gmNotes));
    for (const auto &v : c.timelineEvents) add(v.metadata.id, "timeline", v.title,
        section("Date", v.inWorldDate) + section("Description", v.description));
    for (const auto &v : c.factions) add(v.metadata.id, "faction", v.name,
        section("Summary", v.summary) + section("Goals", v.goals) + section("GM notes", v.gmNotes));
    for (const auto &v : c.deities) add(v.metadata.id, "deity", v.name,
        section("Domains", v.domains) + section("Tenets", v.tenets) + section("GM notes", v.gmNotes));
    for (const auto &v : c.nations) add(v.metadata.id, "nation", v.name,
        section("Description", v.description) + section("Culture", v.culture) + section("Relationships", v.relationships));
    return result;
}
inline bool sameDocuments(const std::vector<Document> &a, const std::vector<Document> &b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].id != b[i].id || a[i].type != b[i].type || a[i].title != b[i].title ||
            a[i].relativePath != b[i].relativePath || a[i].markdown != b[i].markdown) return false;
    return true;
}
}
