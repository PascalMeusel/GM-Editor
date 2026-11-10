#pragma once
#include "CharacterModel.hpp"
namespace CharacterTransfer
{
std::string serializeCharacterForExport(const CharacterModel &character);
bool deserializeCharacterForImport(const std::string &content, CharacterModel &character, std::string &error);
} // namespace CharacterTransfer
