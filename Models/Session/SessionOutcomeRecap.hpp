#pragma once
// Included only by SessionOutcomeService.cpp.
namespace SessionOutcomes
{
PlayerRecapReview reviewRecap(const Campaign &c, EntityId sessionId)
{
    PlayerRecapReview review; review.campaignId = c.metadata.id; review.sessionId = sessionId;
    try
    {
        const auto l = detail::ledger(detail::session(c, sessionId));
        review.document.title = l.publicTitle; review.document.intro = l.publicIntro;
        for (const auto &v : l.outcomes)
            if (v.shareWithPlayers && v.state != State::Rejected && !detail::blank(v.publicText))
                review.document.items.push_back({v.id, v.publicText});
        detail::check(!detail::blank(review.document.title) || !detail::blank(review.document.intro) ||
            !review.document.items.empty(), Status::Invalid, "Choose explicit player-facing recap text before exporting.");
        review.contentToken = detail::projectionToken(review.document); review.success = true;
    }
    catch (const std::exception &e) { review.error = e.what(); }
    return review;
}
bool isRecapCurrent(const Campaign &c, const PlayerRecapReview &review)
{
    if (!review.success || review.campaignId != c.metadata.id || review.contentToken.empty()) return false;
    try
    {
        const auto current = reviewRecap(c, review.sessionId);
        return current.success && current.contentToken == review.contentToken &&
            detail::projectionToken(review.document) == review.contentToken;
    }
    catch (...) { return false; }
}
namespace detail
{
inline std::string escapeHtml(const std::string &text)
{
    std::string output;
    for (const char c : text)
        switch (c)
        {
        case '&': output += "&amp;"; break;
        case '<': output += "&lt;"; break;
        case '>': output += "&gt;"; break;
        case '"': output += "&quot;"; break;
        case '\'': output += "&#39;"; break;
        default: output += c;
        }
    return output;
}
inline std::string escapeMarkdown(const std::string &text)
{
    std::string result;
    for (const char c : text)
    {
        // Keep user prose literal in Markdown, including raw HTML and links.
        if (c == '&') result += "&amp;";
        else if (c == '<') result += "&lt;";
        else if (c == '>') result += "&gt;";
        else { if (std::string("\\`*_{}[]()#+-.!|").find(c) != std::string::npos) result += '\\'; result += c; }
    }
    return result;
}
}
std::string markdown(const PlayerRecapDocument &document)
{
    std::string result;
    if (!detail::blank(document.title)) result += "# " + detail::escapeMarkdown(document.title) + "\n\n";
    if (!detail::blank(document.intro)) result += detail::escapeMarkdown(document.intro) + "\n\n";
    for (const auto &item : document.items) result += detail::escapeMarkdown(item.text) + "\n\n";
    return result;
}
std::string html(const PlayerRecapDocument &document)
{
    std::string result = "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width\"><title>";
    result += detail::escapeHtml(document.title) + "</title><style>body{max-width:48rem;margin:2rem auto;padding:0 1rem;font:18px/1.6 serif;color:#17251f}h1,p{white-space:pre-wrap;overflow-wrap:anywhere}@media print{body{margin:0;max-width:none}}</style></head><body>";
    if (!detail::blank(document.title)) result += "<h1>" + detail::escapeHtml(document.title) + "</h1>";
    if (!detail::blank(document.intro)) result += "<p>" + detail::escapeHtml(document.intro) + "</p>";
    for (const auto &item : document.items) result += "<p>" + detail::escapeHtml(item.text) + "</p>";
    return result + "</body></html>\n";
}
}
