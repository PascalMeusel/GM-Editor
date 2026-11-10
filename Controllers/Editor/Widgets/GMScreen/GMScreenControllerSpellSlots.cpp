#include "Controllers/Editor/Widgets/GMScreen/GMScreenControllerSupport.hpp"

void GMScreenController::setPlayerSpellSlotMaximum(int characterIndex, int spellLevel, int value)
{
    if (characterIndex < 0 || characterIndex >= static_cast<int>(_currentCampaign.characters.size()) || spellLevel < 1 || spellLevel > 9)
        return;

    CharacterModel &character = _currentCampaign.characters[static_cast<std::size_t>(characterIndex)];
    normalizeSpellcasting(character.spellcasting);
    character.spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)] = std::max(0, value);
    character.spellcasting.slotUsed[static_cast<std::size_t>(spellLevel)] = std::clamp(character.spellcasting.slotUsed[static_cast<std::size_t>(spellLevel)], 0, character.spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)]);
    character.metadata.touch();
    _currentCampaign.metadata.touch();
    ProjectSaveService::getInstance().saveCampaign(_currentCampaign);
}

void GMScreenController::setPlayerSpellSlotUsed(int characterIndex, int spellLevel, int value)
{
    if (characterIndex < 0 || characterIndex >= static_cast<int>(_currentCampaign.characters.size()) || spellLevel < 1 || spellLevel > 9)
        return;

    CharacterModel &character = _currentCampaign.characters[static_cast<std::size_t>(characterIndex)];
    normalizeSpellcasting(character.spellcasting);
    character.spellcasting.slotUsed[static_cast<std::size_t>(spellLevel)] = std::clamp(value, 0, character.spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)]);
    character.metadata.touch();
    _currentCampaign.metadata.touch();
    ProjectSaveService::getInstance().saveCampaign(_currentCampaign);
}

void GMScreenController::resetPlayerSpellSlots(int characterIndex)
{
    if (characterIndex < 0 || characterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;

    CharacterModel &character = _currentCampaign.characters[static_cast<std::size_t>(characterIndex)];
    normalizeSpellcasting(character.spellcasting);
    std::fill(character.spellcasting.slotUsed.begin(), character.spellcasting.slotUsed.end(), 0);
    character.metadata.touch();
    _currentCampaign.metadata.touch();
    ProjectSaveService::getInstance().saveCampaign(_currentCampaign);
}

void GMScreenController::setNpcSpellSlotMaximum(int npcIndex, int spellLevel, int value)
{
    if (npcIndex < 0 || npcIndex >= static_cast<int>(_currentCampaign.npcs.size()) || spellLevel < 1 || spellLevel > 9)
        return;

    NPCModel &npc = _currentCampaign.npcs[static_cast<std::size_t>(npcIndex)];
    normalizeSpellcasting(npc.spellcasting);
    npc.spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)] = std::max(0, value);
    npc.spellcasting.slotUsed[static_cast<std::size_t>(spellLevel)] = std::clamp(npc.spellcasting.slotUsed[static_cast<std::size_t>(spellLevel)], 0, npc.spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)]);
    npc.metadata.touch();
    _currentCampaign.metadata.touch();
    ProjectSaveService::getInstance().saveCampaign(_currentCampaign);
}

void GMScreenController::setNpcSpellSlotUsed(int npcIndex, int spellLevel, int value)
{
    if (npcIndex < 0 || npcIndex >= static_cast<int>(_currentCampaign.npcs.size()) || spellLevel < 1 || spellLevel > 9)
        return;

    NPCModel &npc = _currentCampaign.npcs[static_cast<std::size_t>(npcIndex)];
    normalizeSpellcasting(npc.spellcasting);
    npc.spellcasting.slotUsed[static_cast<std::size_t>(spellLevel)] = std::clamp(value, 0, npc.spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)]);
    npc.metadata.touch();
    _currentCampaign.metadata.touch();
    ProjectSaveService::getInstance().saveCampaign(_currentCampaign);
}

void GMScreenController::resetNpcSpellSlots(int npcIndex)
{
    if (npcIndex < 0 || npcIndex >= static_cast<int>(_currentCampaign.npcs.size()))
        return;

    NPCModel &npc = _currentCampaign.npcs[static_cast<std::size_t>(npcIndex)];
    normalizeSpellcasting(npc.spellcasting);
    std::fill(npc.spellcasting.slotUsed.begin(), npc.spellcasting.slotUsed.end(), 0);
    npc.metadata.touch();
    _currentCampaign.metadata.touch();
    ProjectSaveService::getInstance().saveCampaign(_currentCampaign);
}

void GMScreenController::onShow()
{
    _view->show();
    _view->requestDockOnNextUpdate();
}
