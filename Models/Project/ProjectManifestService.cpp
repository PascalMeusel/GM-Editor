#include "ProjectManifestService.hpp"

#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/AtomicProjectFile.hpp"

#include <fstream>
#include <regex>
#include <sstream>

namespace
{
std::string escapeJson(const std::string &value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value)
    {
        switch (character)
        {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(character);
            break;
        }
    }
    return escaped;
}

std::string unescapeJson(const std::string &value)
{
    std::string unescaped;
    unescaped.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        if (value[index] != '\\' || index + 1 >= value.size())
        {
            unescaped.push_back(value[index]);
            continue;
        }

        const char escaped = value[++index];
        switch (escaped)
        {
        case '"':
        case '\\':
        case '/':
            unescaped.push_back(escaped);
            break;
        case 'n':
            unescaped.push_back('\n');
            break;
        case 'r':
            unescaped.push_back('\r');
            break;
        case 't':
            unescaped.push_back('\t');
            break;
        default:
            unescaped.push_back(escaped);
            break;
        }
    }
    return unescaped;
}

std::string stringField(const std::string &json, const std::string &field)
{
    const std::regex pattern("\\\"" + field + "\\\"\\s*:\\s*\\\"((?:\\\\.|[^\\\"\\\\])*)\\\"");
    std::smatch match;
    return std::regex_search(json, match, pattern) ? unescapeJson(match[1].str()) : "";
}

int intField(const std::string &json, const std::string &field, int fallback)
{
    const std::regex pattern("\\\"" + field + "\\\"\\s*:\\s*(-?\\d+)");
    std::smatch match;
    return std::regex_search(json, match, pattern) ? std::stoi(match[1].str()) : fallback;
}

bool boolField(const std::string &json, const std::string &field, bool fallback)
{
    const std::regex pattern("\\\"" + field + "\\\"\\s*:\\s*(true|false)");
    std::smatch match;
    return std::regex_search(json, match, pattern) ? match[1].str() == "true" : fallback;
}

std::vector<std::string> stringArrayField(const std::string &json, const std::string &field)
{
    std::vector<std::string> values;
    const std::regex arrayPattern("\\\"" + field + "\\\"\\s*:\\s*\\[([^\\]]*)\\]");
    std::smatch arrayMatch;
    if (!std::regex_search(json, arrayMatch, arrayPattern))
        return values;

    const std::string arrayBody = arrayMatch[1].str();
    const std::regex valuePattern("\\\"((?:\\\\.|[^\\\"\\\\])*)\\\"");
    for (std::sregex_iterator iterator(arrayBody.begin(), arrayBody.end(), valuePattern), end; iterator != end; ++iterator)
        values.push_back(unescapeJson((*iterator)[1].str()));
    return values;
}

std::string objectField(const std::string &json, const std::string &field)
{
    const std::string key = "\"" + field + "\"";
    const std::size_t keyPosition = json.find(key);
    if (keyPosition == std::string::npos)
        return "";

    const std::size_t objectStart = json.find('{', keyPosition + key.size());
    if (objectStart == std::string::npos)
        return "";

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (std::size_t index = objectStart; index < json.size(); ++index)
    {
        const char character = json[index];
        if (inString)
        {
            escaped = !escaped && character == '\\';
            if (!escaped && character == '"')
                inString = false;
            else if (character != '\\')
                escaped = false;
            continue;
        }

        if (character == '"')
            inString = true;
        else if (character == '{')
            ++depth;
        else if (character == '}' && --depth == 0)
            return json.substr(objectStart, index - objectStart + 1);
    }

    return "";
}

std::vector<std::string> objectArrayEntries(const std::string &json, const std::string &field)
{
    std::vector<std::string> entries;
    const std::string key = "\"" + field + "\"";
    const std::size_t keyPosition = json.find(key);
    if (keyPosition == std::string::npos)
        return entries;

    const std::size_t arrayStart = json.find('[', keyPosition + key.size());
    if (arrayStart == std::string::npos)
        return entries;

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    std::size_t objectStart = std::string::npos;
    for (std::size_t index = arrayStart + 1; index < json.size(); ++index)
    {
        const char character = json[index];
        if (inString)
        {
            escaped = !escaped && character == '\\';
            if (!escaped && character == '"')
                inString = false;
            else if (character != '\\')
                escaped = false;
            continue;
        }

        if (character == '"')
            inString = true;
        else if (character == '{')
        {
            if (depth == 0)
                objectStart = index;
            ++depth;
        }
        else if (character == '}' && depth > 0)
        {
            --depth;
            if (depth == 0 && objectStart != std::string::npos)
                entries.push_back(json.substr(objectStart, index - objectStart + 1));
        }
        else if (character == ']' && depth == 0)
            break;
    }
    return entries;
}

void appendStringField(std::ostringstream &stream, const std::string &field, const std::string &value, const std::string &indent, bool trailingComma = true)
{
    stream << indent << "\"" << field << "\": \"" << escapeJson(value) << "\"";
    if (trailingComma)
        stream << ",";
    stream << "\n";
}
}

namespace ProjectManifest
{
ProjectManifestModel buildManifest(const Campaign &campaign, const RecoveryMetadata &recovery)
{
    ProjectManifestModel manifest;
    manifest.campaignId = campaign.metadata.id;
    manifest.campaignName = campaign.name;
    manifest.assetFolders = ProjectPackageLayout::assetFolderNames();
    manifest.recovery = recovery;

    for (const RulesetContentPackModel &pack : campaign.rulesetContentPacks)
    {
        RulesetPackEntry entry;
        entry.id = pack.metadata.id;
        entry.rulesetId = pack.rulesetId;
        entry.name = pack.name;
        entry.version = pack.version;
        entry.enabled = pack.enabled;
        manifest.rulesetPacks.push_back(entry);
    }

    return manifest;
}

std::string serializeManifest(const ProjectManifestModel &manifest)
{
    std::ostringstream stream;
    stream << "{\n";
    appendStringField(stream, "format", manifest.format, "  ");
    appendStringField(stream, "appVersion", manifest.appVersion, "  ");
    stream << "  \"schemaVersion\": " << manifest.schemaVersion << ",\n";
    appendStringField(stream, "campaignId", manifest.campaignId, "  ");
    appendStringField(stream, "campaignName", manifest.campaignName, "  ");
    appendStringField(stream, "databasePath", manifest.databasePath, "  ");

    stream << "  \"assetFolders\": [";
    for (std::size_t index = 0; index < manifest.assetFolders.size(); ++index)
    {
        if (index > 0)
            stream << ", ";
        stream << "\"" << escapeJson(manifest.assetFolders[index]) << "\"";
    }
    stream << "],\n";

    stream << "  \"rulesetPacks\": [\n";
    for (std::size_t index = 0; index < manifest.rulesetPacks.size(); ++index)
    {
        const RulesetPackEntry &entry = manifest.rulesetPacks[index];
        stream << "    {\n";
        appendStringField(stream, "id", entry.id, "      ");
        appendStringField(stream, "rulesetId", entry.rulesetId, "      ");
        appendStringField(stream, "name", entry.name, "      ");
        appendStringField(stream, "version", entry.version, "      ");
        stream << "      \"enabled\": " << (entry.enabled ? "true" : "false") << "\n";
        stream << "    }";
        if (index + 1 < manifest.rulesetPacks.size())
            stream << ",";
        stream << "\n";
    }
    stream << "  ],\n";

    stream << "  \"recovery\": {\n";
    appendStringField(stream, "latestRecoverySnapshotAtUtc", manifest.recovery.latestRecoverySnapshotAtUtc, "    ");
    appendStringField(stream, "latestRecoverySnapshotPath", manifest.recovery.latestRecoverySnapshotPath, "    ");
    appendStringField(stream, "latestBackupAtUtc", manifest.recovery.latestBackupAtUtc, "    ");
    appendStringField(stream, "latestBackupPath", manifest.recovery.latestBackupPath, "    ");
    stream << "    \"maxRecoverySnapshots\": " << manifest.recovery.maxRecoverySnapshots << ",\n";
    stream << "    \"maxBackupSnapshots\": " << manifest.recovery.maxBackupSnapshots << "\n";
    stream << "  }\n";
    stream << "}\n";
    return stream.str();
}

bool writeManifest(const Campaign &campaign, const RecoveryMetadata &recovery, std::string &errorMessage)
{
    try
    {
        ProjectPackageLayout::ensureCampaignPackageLayout(campaign);
        const ProjectManifestModel manifest = buildManifest(campaign, recovery);
        AtomicProjectFile::write(ProjectPackageLayout::layoutForCampaign(campaign).manifestPath, serializeManifest(manifest));
        errorMessage.clear();
        return true;
    }
    catch (const std::exception &e)
    {
        errorMessage = e.what();
        return false;
    }
}

ProjectManifestModel loadManifest(const std::filesystem::path &manifestPath, std::string &errorMessage)
{
    ProjectManifestModel manifest;
    try
    {
        std::ifstream file(manifestPath);
        if (!file)
        {
            errorMessage = "Could not open project manifest.";
            return manifest;
        }

        const std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        manifest.format = stringField(json, "format");
        manifest.appVersion = stringField(json, "appVersion");
        manifest.schemaVersion = intField(json, "schemaVersion", manifest.schemaVersion);
        manifest.campaignId = stringField(json, "campaignId");
        manifest.campaignName = stringField(json, "campaignName");
        manifest.databasePath = stringField(json, "databasePath");
        manifest.assetFolders = stringArrayField(json, "assetFolders");

        for (const std::string &entryJson : objectArrayEntries(json, "rulesetPacks"))
        {
            RulesetPackEntry entry;
            entry.id = stringField(entryJson, "id");
            entry.rulesetId = stringField(entryJson, "rulesetId");
            entry.name = stringField(entryJson, "name");
            entry.version = stringField(entryJson, "version");
            entry.enabled = boolField(entryJson, "enabled", true);
            manifest.rulesetPacks.push_back(entry);
        }

        const std::string recoveryJson = objectField(json, "recovery");
        manifest.recovery.latestRecoverySnapshotAtUtc = stringField(recoveryJson, "latestRecoverySnapshotAtUtc");
        manifest.recovery.latestRecoverySnapshotPath = stringField(recoveryJson, "latestRecoverySnapshotPath");
        manifest.recovery.latestBackupAtUtc = stringField(recoveryJson, "latestBackupAtUtc");
        manifest.recovery.latestBackupPath = stringField(recoveryJson, "latestBackupPath");
        manifest.recovery.maxRecoverySnapshots = intField(recoveryJson, "maxRecoverySnapshots", manifest.recovery.maxRecoverySnapshots);
        manifest.recovery.maxBackupSnapshots = intField(recoveryJson, "maxBackupSnapshots", manifest.recovery.maxBackupSnapshots);

        if (manifest.format != "ttrpg-editor.project.v1")
            errorMessage = "Unsupported project manifest format.";
        else
            errorMessage.clear();
        return manifest;
    }
    catch (const std::exception &e)
    {
        errorMessage = e.what();
        return manifest;
    }
}
}
