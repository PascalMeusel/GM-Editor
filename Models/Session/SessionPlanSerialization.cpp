#include "SessionPlanService.hpp"
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace SessionPlan
{
namespace
{
// A deliberately small JSON vocabulary (strings and arrays) keeps the storage
// version explicit and supports arbitrary UTF-8 prose without delimiter loss.
struct Json
{
    bool array = false;
    std::string text;
    std::vector<Json> values;
    Json() = default;
    explicit Json(std::string value) : text(std::move(value)) {}
    explicit Json(std::vector<Json> value) : array(true), values(std::move(value)) {}
};
void require(bool condition) { if (!condition) throw std::runtime_error("Invalid saved session plan data."); }
std::string quote(const std::string &text)
{
    static constexpr char hex[] = "0123456789abcdef";
    std::string out = "\"";
    for (unsigned char c : text)
    {
        if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
        else if (c < 32) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
        else out += static_cast<char>(c);
    }
    return out + '"';
}
std::string write(const Json &value)
{
    if (!value.array) return quote(value.text);
    std::string out = "[";
    for (std::size_t i = 0; i < value.values.size(); ++i) { if (i) out += ','; out += write(value.values[i]); }
    return out + ']';
}
class Parser
{
    const std::string &text; std::size_t offset = 0;
    void whitespace() { while (offset < text.size() && std::isspace(static_cast<unsigned char>(text[offset]))) ++offset; }
    unsigned hex4()
    {
        unsigned value = 0;
        for (int i = 0; i < 4; ++i)
        {
            require(offset < text.size()); const char c = text[offset++];
            const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            require(digit >= 0); value = value * 16 + digit;
        }
        return value;
    }
    void utf8(std::string &out, unsigned value)
    {
        if (value < 0x80) out += static_cast<char>(value);
        else if (value < 0x800) { out += static_cast<char>(0xc0 | value >> 6); out += static_cast<char>(0x80 | (value & 63)); }
        else if (value < 0x10000) { out += static_cast<char>(0xe0 | value >> 12); out += static_cast<char>(0x80 | ((value >> 6) & 63)); out += static_cast<char>(0x80 | (value & 63)); }
        else { out += static_cast<char>(0xf0 | value >> 18); out += static_cast<char>(0x80 | ((value >> 12) & 63)); out += static_cast<char>(0x80 | ((value >> 6) & 63)); out += static_cast<char>(0x80 | (value & 63)); }
    }
    Json node(int depth)
    {
        require(depth < 8); whitespace(); require(offset < text.size());
        if (text[offset] == '[')
        {
            ++offset; Json result(std::vector<Json>{}); whitespace();
            if (offset < text.size() && text[offset] == ']') { ++offset; return result; }
            while (true)
            {
                result.values.push_back(node(depth + 1)); whitespace(); require(offset < text.size());
                const char delimiter = text[offset++]; if (delimiter == ']') return result;
                require(delimiter == ',');
            }
        }
        require(text[offset++] == '"'); std::string out;
        while (offset < text.size())
        {
            const unsigned char c = text[offset++]; if (c == '"') return Json(std::move(out));
            require(c >= 32);
            if (c != '\\') { out += static_cast<char>(c); continue; }
            require(offset < text.size()); const char escape = text[offset++];
            switch (escape)
            {
            case '"': case '\\': case '/': out += escape; break;
            case 'n': out += '\n'; break; case 'r': out += '\r'; break; case 't': out += '\t'; break;
            case 'b': out += '\b'; break; case 'f': out += '\f'; break;
            case 'u': {
                unsigned code = hex4();
                if (code >= 0xd800 && code <= 0xdbff)
                {
                    require(offset + 2 <= text.size() && text[offset] == '\\' && text[offset + 1] == 'u'); offset += 2;
                    const unsigned low = hex4(); require(low >= 0xdc00 && low <= 0xdfff);
                    code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
                }
                else require(code < 0xdc00 || code > 0xdfff);
                utf8(out, code); break;
            }
            default: require(false);
            }
        }
        require(false); return {};
    }
public:
    explicit Parser(const std::string &value) : text(value) {}
    Json parse() { auto result = node(0); whitespace(); require(offset == text.size()); return result; }
};
const std::string &field(const Json &value, std::size_t index)
{ require(value.array && index < value.values.size() && !value.values[index].array); return value.values[index].text; }
int enumValue(const std::string &value, int maximum)
{ require(value.size() == 1 && value[0] >= '0' && value[0] <= '0' + maximum); return value[0] - '0'; }
}
std::string serializeEntries(const std::vector<SessionPlanEntry> &entries)
{
    Json root(std::vector<Json>{Json("session-plan-v1")});
    for (const auto &entry : entries)
    {
        Json tags(std::vector<Json>{}), properties(std::vector<Json>{});
        for (const auto &tag : entry.metadata.tags) tags.values.emplace_back(tag);
        for (const auto &[key, value] : entry.metadata.customProperties) { properties.values.emplace_back(key); properties.values.emplace_back(value); }
        root.values.emplace_back(std::vector<Json>{Json(entry.metadata.id), Json(entry.storyBeatId), Json(entry.prepNote),
            Json(std::to_string(static_cast<int>(entry.state))), Json(entry.metadata.createdAtUtc), Json(entry.metadata.updatedAtUtc), tags, properties});
    }
    return write(root);
}
std::vector<SessionPlanEntry> deserializeEntries(const std::string &text)
{
    if (text.empty()) return {};
    const Json root = Parser(text).parse(); require(field(root, 0) == "session-plan-v1");
    std::vector<SessionPlanEntry> entries; std::unordered_set<EntityId> ids;
    for (std::size_t i = 1; i < root.values.size(); ++i)
    {
        const auto &row = root.values[i]; require(row.array && row.values.size() == 8);
        SessionPlanEntry entry; entry.metadata.id = field(row, 0); entry.storyBeatId = field(row, 1); entry.prepNote = field(row, 2);
        require(!entry.metadata.id.empty() && !entry.storyBeatId.empty() && ids.insert(entry.metadata.id).second);
        entry.state = static_cast<SessionEntryState>(enumValue(field(row, 3), 2));
        entry.metadata.createdAtUtc = field(row, 4); entry.metadata.updatedAtUtc = field(row, 5);
        require(row.values[6].array && row.values[7].array && row.values[7].values.size() % 2 == 0);
        for (std::size_t t = 0; t < row.values[6].values.size(); ++t) entry.metadata.tags.push_back(field(row.values[6], t));
        for (std::size_t p = 0; p < row.values[7].values.size(); p += 2) entry.metadata.customProperties[field(row.values[7], p)] = field(row.values[7], p + 1);
        entries.push_back(std::move(entry));
    }
    return entries;
}
std::string serializeLiveState(const SessionLiveState &state)
{
    return write(Json(std::vector<Json>{Json("session-live-v1"), Json(state.initialized ? "1" : "0"),
        Json(std::to_string(static_cast<int>(state.phase))), Json(std::to_string(static_cast<int>(state.phaseBeforeBreak))),
        Json(state.activePlanEntryId), Json(state.activeEncounterId)}));
}
SessionLiveState deserializeLiveState(const std::string &text)
{
    if (text.empty()) return {};
    const auto row = Parser(text).parse(); require(row.array && row.values.size() == 6 && field(row, 0) == "session-live-v1");
    SessionLiveState state; state.initialized = enumValue(field(row, 1), 1) != 0;
    state.phase = static_cast<SessionPhase>(enumValue(field(row, 2), 5)); state.phaseBeforeBreak = static_cast<SessionPhase>(enumValue(field(row, 3), 5));
    state.activePlanEntryId = field(row, 4); state.activeEncounterId = field(row, 5); return state;
}
}
