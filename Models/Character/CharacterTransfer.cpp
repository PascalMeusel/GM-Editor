#include "CharacterTransfer.hpp"
#include "Models/Persistence/PersistenceSerialization.hpp"
#include <cstring>
#include <sstream>
#include <stdexcept>
namespace CharacterTransfer
{
constexpr char kExportFieldSeparator = '\x1f';
std::string serializeCharacterForExport(const CharacterModel &character)
{
    using namespace PersistenceSerialization;
    const std::vector<std::string> fields = {character.nameBuffer,
                                             std::to_string(character.level),
                                             character.selectedClass,
                                             character.selectedRace,
                                             character.background,
                                             character.backstory,
                                             character.playerNotes,
                                             character.gmNotes,
                                             serializeAttributes(character.attributes),
                                             serializeStringList(character.feats),
                                             serializeStringList(character.traitsAndFeatures),
                                             serializeStringList(character.languages),
                                             serializeStringList(character.senses),
                                             serializeStringList(character.savingThrowProficiencies),
                                             serializeStringList(character.skillProficiencies),
                                             serializeStringList(character.toolProficiencies),
                                             std::to_string(character.speedFeet),
                                             std::to_string(character.currentHitPoints),
                                             std::to_string(character.maxHitPoints),
                                             character.portraitAssetId,
                                             serializeInventoryEntries(character.inventory.entries),
                                             std::to_string(character.inventory.copper),
                                             std::to_string(character.inventory.silver),
                                             std::to_string(character.inventory.gold),
                                             std::to_string(character.inventory.platinum),
                                             serializeStringList(character.conditions),
                                             character.spellcasting.enabled ? "1" : "0",
                                             character.spellcasting.ability,
                                             std::to_string(character.spellcasting.spellAttackBonus),
                                             std::to_string(character.spellcasting.spellSaveDc),
                                             serializeSpellSlots(character.spellcasting.slotMaximums),
                                             serializeSpellSlots(character.spellcasting.slotUsed),
                                             serializeSpellbookEntries(character.spellcasting.spells),
                                             serializeCharacterRelationships(character.relationships),
                                             serializeAttributes(character.classLevels),
                                             serializeMonsterActions(character.featureActions),
                                             serializeResourceTrackers(character.resourceTrackers),
                                             serializeAttributes(character.ruleValues),
                                             std::to_string(std::max(0, character.temporaryHitPoints)),
                                             std::to_string(static_cast<int>(character.spellcasting.spellAttackMode)),
                                             std::to_string(static_cast<int>(character.spellcasting.spellSaveDcMode)),
                                             "END_V4"};
    std::ostringstream stream;
    stream << "TTRPG_CHARACTER_V4\n";
    for (const auto &field : fields)
        stream << field.size() << '\n' << field;
    return stream.str();
}

bool deserializeCharacterForImport(const std::string &content, CharacterModel &character, std::string &error)
{
    using namespace PersistenceSerialization;
    std::stringstream stream(content);
    std::vector<std::string> fields;
    if (content.rfind("TTRPG_CHARACTER_V2\n", 0) == 0 ||
        content.rfind("TTRPG_CHARACTER_V3\n", 0) == 0 ||
        content.rfind("TTRPG_CHARACTER_V4\n", 0) == 0)
    {
        std::string header;
        std::getline(stream, header);
        fields.push_back(header);
        const int fieldCount = header == "TTRPG_CHARACTER_V4" ? 42 : (header == "TTRPG_CHARACTER_V3" ? 40 : 39);
        for (int i = 0; i < fieldCount; ++i)
        {
            std::string length;
            if (!std::getline(stream, length) || length.empty() ||
                length.find_first_not_of("0123456789") != std::string::npos)
            {
                error = "Unsupported character export file.";
                return false;
            }
            std::size_t size = 0;
            try
            {
                size = std::stoull(length);
            }
            catch (...)
            {
                error = "Unsupported character export file.";
                return false;
            }
            if (size > content.size())
            {
                error = "Unsupported character export file.";
                return false;
            }
            std::string field(size, '\0');
            if (size && !stream.read(field.data(), static_cast<std::streamsize>(size)))
            {
                error = "Unsupported character export file.";
                return false;
            }
            fields.push_back(std::move(field));
        }
        if (stream.peek() != std::char_traits<char>::eof())
        {
            error = "Unsupported character export file.";
            return false;
        }
    }
    else
    {
        std::string field;
        while (std::getline(stream, field, kExportFieldSeparator))
            fields.push_back(field);
    }
    if (fields.size() < 34 ||
        (fields[0] != "TTRPG_CHARACTER_V1" && fields[0] != "TTRPG_CHARACTER_V2" && fields[0] != "TTRPG_CHARACTER_V3" && fields[0] != "TTRPG_CHARACTER_V4") ||
        (fields[0] == "TTRPG_CHARACTER_V2" && (fields.size() != 40 || fields[39] != "END_V2")) ||
        (fields[0] == "TTRPG_CHARACTER_V3" && (fields.size() != 41 || fields[40] != "END_V3")) ||
        (fields[0] == "TTRPG_CHARACTER_V4" && (fields.size() != 43 || fields[42] != "END_V4")))
    {
        error = "Unsupported character export file.";
        return false;
    }

    character = CharacterModel{};
#if defined(_MSC_VER)
    strncpy_s(character.nameBuffer, sizeof(character.nameBuffer), fields[1].c_str(), _TRUNCATE);
#else
    std::strncpy(character.nameBuffer, fields[1].c_str(), sizeof(character.nameBuffer) - 1);
    character.nameBuffer[sizeof(character.nameBuffer) - 1] = '\0';
#endif
    try
    {
        character.level = std::max(1, std::stoi(fields[2]));
        character.speedFeet = std::max(0, std::stoi(fields[17]));
        character.currentHitPoints = std::max(0, std::stoi(fields[18]));
        character.maxHitPoints = std::max(0, std::stoi(fields[19]));
        if (fields[0] == "TTRPG_CHARACTER_V3" || fields[0] == "TTRPG_CHARACTER_V4")
            character.temporaryHitPoints = std::max(0, std::stoi(fields[39]));
        if (fields[0] == "TTRPG_CHARACTER_V4")
        {
            const auto mode=[](const std::string &value) {
                if(value!="0"&&value!="1"&&value!="2")throw std::invalid_argument("Invalid spell value mode.");
                return static_cast<SpellValueMode>(value[0]-'0');
            };
            character.spellcasting.spellAttackMode=mode(fields[40]);
            character.spellcasting.spellSaveDcMode=mode(fields[41]);
        }
        character.inventory.copper = std::max(0, std::stoi(fields[22]));
        character.inventory.silver = std::max(0, std::stoi(fields[23]));
        character.inventory.gold = std::max(0, std::stoi(fields[24]));
        character.inventory.platinum = std::max(0, std::stoi(fields[25]));
        character.spellcasting.enabled = fields[27] == "1";
        character.spellcasting.spellAttackBonus = std::stoi(fields[29]);
        character.spellcasting.spellSaveDc = std::stoi(fields[30]);
    }
    catch (const std::exception &)
    {
        error = "Character export contains invalid numeric data.";
        return false;
    }
    character.selectedClass = fields[3];
    character.selectedRace = fields[4];
    character.background = fields[5];
    character.backstory = fields[6];
    character.playerNotes = fields[7];
    character.gmNotes = fields[8];
    character.attributes = deserializeAttributes(fields[9]);
    character.feats = deserializeStringList(fields[10]);
    character.traitsAndFeatures = deserializeStringList(fields[11]);
    character.languages = deserializeStringList(fields[12]);
    character.senses = deserializeStringList(fields[13]);
    character.savingThrowProficiencies = deserializeStringList(fields[14]);
    character.skillProficiencies = deserializeStringList(fields[15]);
    character.toolProficiencies = deserializeStringList(fields[16]);
    character.portraitAssetId = fields[20];
    character.inventory.entries = deserializeInventoryEntries(fields[21]);
    character.conditions = deserializeStringList(fields[26]);
    character.spellcasting.ability = fields[28];
    character.spellcasting.slotMaximums = deserializeSpellSlots(fields[31]);
    character.spellcasting.slotUsed = deserializeSpellSlots(fields[32]);
    character.spellcasting.spells = deserializeSpellbookEntries(fields[33]);
    if (fields.size() > 34)
        character.relationships = deserializeCharacterRelationships(fields[34]);
    if (fields[0] == "TTRPG_CHARACTER_V2" || fields[0] == "TTRPG_CHARACTER_V3" || fields[0] == "TTRPG_CHARACTER_V4")
    {
        character.classLevels = deserializeAttributes(fields[35]);
        character.featureActions = deserializeMonsterActions(fields[36]);
        character.resourceTrackers = deserializeResourceTrackers(fields[37]);
        character.ruleValues = deserializeAttributes(fields[38]);
        long long total = 0;
        for (const auto &[name, level] : character.classLevels)
        {
            if (name.empty() || level < 1)
            {
                error = "Invalid class history.";
                return false;
            }
            total += level;
        }
        if (!character.classLevels.empty() && total != character.level)
        {
            error = "Invalid class history.";
            return false;
        }
    }
    normalizeCharacterHitPoints(character);
    normalizeSpellcasting(character.spellcasting);
    return true;
}
} // namespace CharacterTransfer
