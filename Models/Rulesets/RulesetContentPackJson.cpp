#include "RulesetContentPackJson.hpp"

#include <regex>
#include <sstream>

namespace
{
std::string escapeJson(const std::string &value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (char character : value)
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

bool boolField(const std::string &json, const std::string &field, bool fallback)
{
    const std::regex pattern("\\\"" + field + "\\\"\\s*:\\s*(true|false)");
    std::smatch match;
    return std::regex_search(json, match, pattern) ? match[1].str() == "true" : fallback;
}

void appendStringField(std::ostringstream &stream, const std::string &field, const std::string &value, bool trailingComma = true)
{
    stream << "  \"" << field << "\": \"" << escapeJson(value) << "\"";
    if (trailingComma)
        stream << ",";
    stream << "\n";
}
}

std::string exportRulesetContentPackToJson(const RulesetContentPackModel &pack)
{
    std::ostringstream stream;
    stream << "{\n";
    appendStringField(stream, "format", "ttrpg-editor.homebrew-pack.v1");
    appendStringField(stream, "id", pack.metadata.id);
    appendStringField(stream, "rulesetId", pack.rulesetId);
    appendStringField(stream, "name", pack.name);
    appendStringField(stream, "version", pack.version);
    appendStringField(stream, "author", pack.author);
    appendStringField(stream, "license", pack.license);
    appendStringField(stream, "importedAtUtc", pack.importedAtUtc);
    appendStringField(stream, "source", pack.source);
    stream << "  \"enabled\": " << (pack.enabled ? "true" : "false") << ",\n";
    stream << "  \"tags\": [";
    for (std::size_t index = 0; index < pack.metadata.tags.size(); ++index)
    {
        if (index > 0)
            stream << ", ";
        stream << "\"" << escapeJson(pack.metadata.tags[index]) << "\"";
    }
    stream << "],\n";
    appendStringField(stream, "definitionJson", pack.definitionJson, false);
    stream << "}\n";
    return stream.str();
}

RulesetContentPackImportResult importRulesetContentPackFromJson(const std::string &json)
{
    RulesetContentPackImportResult result;
    const std::string format = stringField(json, "format");
    if (format != "ttrpg-editor.homebrew-pack.v1")
    {
        result.error = "Unsupported content pack format.";
        return result;
    }

    result.pack.metadata.id = stringField(json, "id");
    result.pack.rulesetId = stringField(json, "rulesetId");
    result.pack.name = stringField(json, "name");
    result.pack.version = stringField(json, "version");
    result.pack.author = stringField(json, "author");
    result.pack.license = stringField(json, "license");
    result.pack.importedAtUtc = stringField(json, "importedAtUtc");
    result.pack.source = stringField(json, "source");
    result.pack.enabled = boolField(json, "enabled", true);
    result.pack.metadata.tags = stringArrayField(json, "tags");
    result.pack.definitionJson = stringField(json, "definitionJson");

    if (result.pack.name.empty())
    {
        result.error = "Content pack name is required.";
        return result;
    }

    result.success = true;
    return result;
}
