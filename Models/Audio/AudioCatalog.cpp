#include "AudioCatalog.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <sstream>

namespace
{
std::string trim(std::string value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }).base();

    if (first >= last)
        return {};
    return std::string(first, last);
}

std::vector<std::string> splitPipeLine(const std::string &line)
{
    std::vector<std::string> values;
    std::stringstream stream(line);
    std::string value;
    while (std::getline(stream, value, '|'))
        values.push_back(trim(value));
    return values;
}

AudioAssetType parseAssetType(const std::string &value)
{
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (normalized == "music")
        return AudioAssetType::Music;
    if (normalized == "ambience")
        return AudioAssetType::Ambience;
    if (normalized == "effect")
        return AudioAssetType::Effect;
    if (normalized == "voice")
        return AudioAssetType::Voice;
    return AudioAssetType::Other;
}
}

std::vector<AudioCatalogEntry> loadBundledAudioCatalog(const std::filesystem::path &projectRoot, std::string &errorMessage)
{
    std::vector<AudioCatalogEntry> entries;
    errorMessage.clear();

    const std::filesystem::path manifestPath = projectRoot / "Resources" / "Audio" / "catalog.psv";
    std::ifstream manifest(manifestPath);
    if (!manifest)
    {
        errorMessage = "Bundled audio catalog manifest is missing: " + manifestPath.string();
        return entries;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(manifest, line))
    {
        ++lineNumber;
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#')
            continue;

        const std::vector<std::string> values = splitPipeLine(trimmed);
        if (values.size() != 11)
        {
            errorMessage = "Invalid bundled audio catalog line " + std::to_string(lineNumber) + ".";
            entries.clear();
            return entries;
        }

        AudioCatalogEntry entry;
        entry.id = values[0];
        entry.name = values[1];
        entry.type = parseAssetType(values[2]);
        entry.theme = values[3];
        entry.relativePath = std::filesystem::path(values[4]);
        try
        {
            entry.defaultVolume = std::stof(values[5]);
        }
        catch (const std::exception &)
        {
            entry.defaultVolume = 1.0f;
        }
        entry.loop = values[6] == "1" || values[6] == "true" || values[6] == "True";
        entry.sourceTitle = values[7];
        entry.author = values[8];
        entry.license = values[9];
        entry.sourceUrl = values[10];
        entries.push_back(entry);
    }

    return entries;
}
