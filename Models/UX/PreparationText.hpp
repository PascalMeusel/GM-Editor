#pragma once
#include <string>
#include <algorithm>
#include <cctype>

namespace PreparationText
{
// The full passage remains the scene body. Titles use its first sentence and
// stop at a word boundary; UTF-8 codepoints are never split by the length limit.
inline std::string sceneTitle(const std::string &passage, std::size_t limit = 60)
{
    const auto begin = passage.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    std::size_t end = passage.find_first_of("\r\n.!?", begin);
    if (end == std::string::npos) end = passage.size();
    std::size_t cut = begin, count = 0;
    while (cut < end && count < limit)
    {
        ++cut;
        while (cut < end && (static_cast<unsigned char>(passage[cut]) & 0xc0) == 0x80) ++cut;
        ++count;
    }
    if (cut < end)
    {
        const auto space = passage.find_last_of(" \t", cut);
        if (space != std::string::npos && space > begin) cut = space;
    }
    while (cut > begin && std::isspace(static_cast<unsigned char>(passage[cut - 1]))) --cut;
    return passage.substr(begin, cut - begin);
}
}
