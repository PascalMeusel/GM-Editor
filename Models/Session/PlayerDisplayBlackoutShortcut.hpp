#pragma once

#include <cstdint>

// Blackout is a safety action, not an ordinary editing shortcut. It therefore
// wins even while a text widget owns keyboard input. In that one frame only
// the B/b text event caused by the shortcut is consumed; unrelated queued
// characters remain available to the active text field.
inline bool globalPlayerDisplayBlackoutRequested(
    bool bKeyPressed,
    bool playerOutputMayBeVisible)
{
    return bKeyPressed && playerOutputMayBeVisible;
}

inline bool isPlayerDisplayBlackoutTextCharacter(std::uint32_t codepoint)
{
    return codepoint == static_cast<std::uint32_t>('b') ||
           codepoint == static_cast<std::uint32_t>('B');
}

template <typename CharacterBuffer>
void consumeGlobalPlayerDisplayBlackoutText(
    CharacterBuffer &characters,
    bool bKeyPressed,
    bool textInputActive)
{
    if (!bKeyPressed || !textInputActive || characters.empty())
        return;

    auto write = characters.begin();
    for (auto read = characters.begin(); read != characters.end(); ++read)
    {
        if (!isPlayerDisplayBlackoutTextCharacter(
                static_cast<std::uint32_t>(*read)))
            *write++ = *read;
    }
    if (write != characters.end())
        characters.erase(write, characters.end());
}

template <typename CharacterBuffer, typename BlackoutAction>
bool handleGlobalPlayerDisplayBlackoutShortcut(
    CharacterBuffer &characters,
    bool bKeyPressed,
    bool playerOutputMayBeVisible,
    bool textInputActive,
    BlackoutAction blackoutAction)
{
    if (!globalPlayerDisplayBlackoutRequested(
            bKeyPressed, playerOutputMayBeVisible))
        return false;
    consumeGlobalPlayerDisplayBlackoutText(
        characters, bKeyPressed, textInputActive);
    blackoutAction();
    return true;
}
