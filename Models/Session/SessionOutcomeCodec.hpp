#pragma once
#include "Models/Session/SessionOutcomeModels.hpp"
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace SessionOutcomes::detail
{
inline constexpr std::size_t MaxPayload = 8 * 1024 * 1024;
inline constexpr std::size_t MaxEntries = 10000;
inline void require(bool value, const char *message = "Invalid saved outcome data.")
{ if (!value) throw std::runtime_error(message); }
struct Writer
{
    std::string value;
    void text(const std::string &text)
    {
        require(text.size() <= MaxPayload, "Outcome text is too large.");
        value += std::to_string(text.size()) + ':' + text;
        require(value.size() <= MaxPayload, "Outcome ledger is too large.");
    }
    void number(std::size_t number) { text(std::to_string(number)); }
};
struct Reader
{
    const std::string &value;
    std::size_t cursor = 0;
    std::string text()
    {
        require(value.size() <= MaxPayload, "Outcome ledger is too large.");
        std::size_t length = 0, digits = 0;
        while (cursor < value.size() && value[cursor] != ':')
        {
            const char c = value[cursor++];
            require(c >= '0' && c <= '9' && ++digits <= 9);
            length = length * 10 + static_cast<unsigned>(c - '0');
            require(length <= MaxPayload);
        }
        require(digits && cursor < value.size());
        ++cursor;
        require(length <= value.size() - cursor);
        auto result = value.substr(cursor, length); cursor += length; return result;
    }
    std::size_t number(std::size_t maximum)
    {
        const auto encoded = text(); require(!encoded.empty() && encoded.size() <= 9);
        std::size_t result = 0;
        for (const char c : encoded)
        {
            require(c >= '0' && c <= '9');
            result = result * 10 + static_cast<unsigned>(c - '0');
            require(result <= maximum);
        }
        return result;
    }
};
inline void target(Writer &w, const Target &v)
{ w.number(static_cast<std::size_t>(v.type)); w.text(v.id); }
inline Target target(Reader &r)
{
    Target result{static_cast<TargetType>(r.number(4)), r.text()};
    require((result.type == TargetType::None) == result.id.empty());
    return result;
}
inline void outcome(Writer &w, const Outcome &v)
{
    w.text(v.id); w.text(v.createdAtUtc); w.number(static_cast<std::size_t>(v.kind));
    w.number(static_cast<std::size_t>(v.state)); w.text(v.privateText); w.text(v.publicText);
    w.number(v.shareWithPlayers); w.text(v.sourceNoteId); w.text(v.sourceSceneId); target(w, v.target);
    w.number(static_cast<std::size_t>(v.action)); w.text(v.changeText);
    require(v.desiredStatus >= 0 && v.desiredStatus <= 3); w.number(v.desiredStatus);
}
inline Outcome outcome(Reader &r)
{
    Outcome v; v.id = r.text(); v.createdAtUtc = r.text();
    v.kind = static_cast<Kind>(r.number(2)); v.state = static_cast<State>(r.number(2));
    v.privateText = r.text(); v.publicText = r.text(); v.shareWithPlayers = r.number(1) != 0;
    v.sourceNoteId = r.text(); v.sourceSceneId = r.text(); v.target = target(r);
    v.action = static_cast<Action>(r.number(static_cast<std::size_t>(Action::NpcNote))); v.changeText = r.text();
    v.desiredStatus = static_cast<int>(r.number(3)); require(!v.id.empty()); return v;
}
inline void change(Writer &w, const Change &v)
{
    w.text(v.outcomeId); target(w, v.target); w.number(static_cast<std::size_t>(v.action));
    w.text(v.before); w.text(v.after); w.text(v.outcomeSignature);
}
inline Change change(Reader &r)
{
    Change v; v.outcomeId = r.text(); v.target = target(r); v.action = static_cast<Action>(r.number(static_cast<std::size_t>(Action::NpcNote)));
    v.before = r.text(); v.after = r.text(); v.outcomeSignature = r.text();
    require(!v.outcomeId.empty()); return v;
}
inline std::string encode(const Ledger &v)
{
    require(v.outcomes.size() <= MaxEntries && v.receipts.size() <= MaxEntries && v.followUps.size() <= MaxEntries);
    Writer w; w.text("gm-session-outcomes-1"); w.text(v.publicTitle); w.text(v.publicIntro);
    w.text(v.carryRequestId); w.text(v.carryRequestToken);
    w.number(v.outcomes.size()); for (const auto &row : v.outcomes) outcome(w, row);
    w.number(v.receipts.size());
    for (const auto &row : v.receipts)
    {
        w.text(row.id); w.text(row.appliedAtUtc); w.number(row.undone); w.number(row.changes.size());
        require(row.changes.size() <= MaxEntries);
        for (const auto &item : row.changes) change(w, item);
    }
    w.number(v.followUps.size());
    for (const auto &row : v.followUps)
    {
        w.text(row.id); w.text(row.sourceSessionId); w.text(row.sourceOutcomeId); w.text(row.sourceSceneId);
        w.text(row.privateText); target(w, row.target); w.number(row.completed);
    }
    return w.value;
}
inline Ledger decode(const std::string &text)
{
    if (text.empty()) return {};
    Reader r{text}; require(r.text() == "gm-session-outcomes-1", "Unsupported outcome ledger version.");
    Ledger v; v.publicTitle = r.text(); v.publicIntro = r.text();
    v.carryRequestId = r.text(); v.carryRequestToken = r.text();
    std::unordered_set<EntityId> ids, outcomeIds;
    const auto identity = [&](const EntityId &id) { require(!id.empty() && ids.insert(id).second, "Duplicate or empty outcome ledger identity."); };
    auto count = r.number(MaxEntries);
    for (std::size_t i = 0; i < count; ++i)
    {
        auto row = outcome(r); identity(row.id); outcomeIds.insert(row.id); v.outcomes.push_back(std::move(row));
    }
    count = r.number(MaxEntries);
    for (std::size_t i = 0; i < count; ++i)
    {
        Receipt row; row.id = r.text(); identity(row.id); row.appliedAtUtc = r.text(); row.undone = r.number(1) != 0;
        const auto changes = r.number(MaxEntries); require(changes > 0);
        std::unordered_set<EntityId> changed;
        for (std::size_t c = 0; c < changes; ++c)
        {
            auto item = change(r); require(outcomeIds.count(item.outcomeId) && changed.insert(item.outcomeId).second);
            row.changes.push_back(std::move(item));
        }
        v.receipts.push_back(std::move(row));
    }
    count = r.number(MaxEntries);
    for (std::size_t i = 0; i < count; ++i)
    {
        FollowUp row; row.id = r.text(); identity(row.id); row.sourceSessionId = r.text();
        row.sourceOutcomeId = r.text(); row.sourceSceneId = r.text(); row.privateText = r.text();
        row.target = target(r); row.completed = r.number(1) != 0;
        require(!row.sourceSessionId.empty() && !row.sourceOutcomeId.empty()); v.followUps.push_back(std::move(row));
    }
    require(r.cursor == text.size(), "Trailing outcome ledger data.");
    return v;
}
inline std::string projectionToken(const PlayerRecapDocument &document)
{
    Writer w; w.text(document.title); w.text(document.intro); w.number(document.items.size());
    for (const auto &item : document.items) { w.text(item.outcomeId); w.text(item.text); }
    return w.value;
}
}
