#pragma once

#include "Models/Map/MapStyleResourcePaths.hpp"

inline std::string mapStyleJsonUnescape(const std::string &value)
{
    std::string result;
    result.reserve(value.size());
    for (size_t index = 0; index < value.size(); ++index)
    {
        if (value[index] != '\\' || index + 1 >= value.size())
        {
            result.push_back(value[index]);
            continue;
        }

        const char escaped = value[++index];
        switch (escaped)
        {
        case '"':
        case '\\':
        case '/':
            result.push_back(escaped);
            break;
        case 'b':
            result.push_back('\b');
            break;
        case 'f':
            result.push_back('\f');
            break;
        case 'n':
            result.push_back('\n');
            break;
        case 'r':
            result.push_back('\r');
            break;
        case 't':
            result.push_back('\t');
            break;
        default:
            result.push_back(escaped);
            break;
        }
    }
    return result;
}

inline std::string mapStyleJsonStringField(const std::string &json, const std::string &field)
{
    const std::string key = "\"" + field + "\"";
    const std::size_t keyPosition = json.find(key);
    if (keyPosition == std::string::npos)
        return {};

    const std::size_t colon = json.find(':', keyPosition + key.size());
    if (colon == std::string::npos)
        return {};

    std::size_t valueStart = colon + 1;
    while (valueStart < json.size() && std::isspace(static_cast<unsigned char>(json[valueStart])))
        ++valueStart;
    if (valueStart >= json.size() || json[valueStart] != '"')
        return {};

    ++valueStart;
    bool escaped = false;
    for (std::size_t index = valueStart; index < json.size(); ++index)
    {
        const char character = json[index];
        if (escaped)
        {
            escaped = false;
            continue;
        }
        if (character == '\\')
        {
            escaped = true;
            continue;
        }
        if (character == '"')
            return mapStyleJsonUnescape(json.substr(valueStart, index - valueStart));
    }
    return {};
}

inline int mapStyleJsonIntField(const std::string &json, const std::string &field, int fallback = 0)
{
    const std::string key = "\"" + field + "\"";
    const std::size_t keyPosition = json.find(key);
    if (keyPosition == std::string::npos)
        return fallback;

    const std::size_t colon = json.find(':', keyPosition + key.size());
    if (colon == std::string::npos)
        return fallback;

    std::size_t valueStart = colon + 1;
    while (valueStart < json.size() && std::isspace(static_cast<unsigned char>(json[valueStart])))
        ++valueStart;
    if (valueStart >= json.size())
        return fallback;

    std::size_t valueEnd = valueStart;
    if (json[valueEnd] == '-')
        ++valueEnd;
    while (valueEnd < json.size() && std::isdigit(static_cast<unsigned char>(json[valueEnd])))
        ++valueEnd;
    if (valueEnd == valueStart || (valueEnd == valueStart + 1 && json[valueStart] == '-'))
        return fallback;

    try
    {
        return std::stoi(json.substr(valueStart, valueEnd - valueStart));
    }
    catch (...)
    {
        return fallback;
    }
}

inline float mapStyleJsonFloatField(const std::string &json, const std::string &field, float fallback = 0.0f)
{
    const std::string key = "\"" + field + "\"";
    const std::size_t keyPosition = json.find(key);
    if (keyPosition == std::string::npos)
        return fallback;
    const std::size_t colon = json.find(':', keyPosition + key.size());
    if (colon == std::string::npos)
        return fallback;
    std::size_t valueStart = colon + 1;
    while (valueStart < json.size() && std::isspace(static_cast<unsigned char>(json[valueStart])))
        ++valueStart;
    std::size_t valueEnd = valueStart;
    while (valueEnd < json.size() && (std::isdigit(static_cast<unsigned char>(json[valueEnd])) ||
                                      json[valueEnd] == '-' || json[valueEnd] == '+' || json[valueEnd] == '.' ||
                                      json[valueEnd] == 'e' || json[valueEnd] == 'E'))
        ++valueEnd;
    try
    {
        return valueEnd > valueStart ? std::stof(json.substr(valueStart, valueEnd - valueStart)) : fallback;
    }
    catch (...)
    {
        return fallback;
    }
}

inline std::string mapStyleJsonObjectField(const std::string &json, const std::string &field)
{
    const std::string key = "\"" + field + "\"";
    const std::size_t keyPosition = json.find(key);
    if (keyPosition == std::string::npos)
        return {};

    const std::size_t colon = json.find(':', keyPosition + key.size());
    if (colon == std::string::npos)
        return {};

    std::size_t objectStart = colon + 1;
    while (objectStart < json.size() && std::isspace(static_cast<unsigned char>(json[objectStart])))
        ++objectStart;
    if (objectStart >= json.size() || json[objectStart] != '{')
        return {};

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (std::size_t index = objectStart; index < json.size(); ++index)
    {
        const char character = json[index];
        if (escaped)
        {
            escaped = false;
            continue;
        }
        if (character == '\\' && inString)
        {
            escaped = true;
            continue;
        }
        if (character == '"')
        {
            inString = !inString;
            continue;
        }
        if (inString)
            continue;
        if (character == '{')
            ++depth;
        else if (character == '}')
        {
            --depth;
            if (depth == 0)
                return json.substr(objectStart, index - objectStart + 1);
        }
    }
    return {};
}

inline std::string mapStyleJsonArrayField(const std::string &json, const std::string &field)
{
    const std::string key = "\"" + field + "\"";
    const std::size_t keyPosition = json.find(key);
    if (keyPosition == std::string::npos)
        return {};

    const std::size_t colon = json.find(':', keyPosition + key.size());
    if (colon == std::string::npos)
        return {};

    std::size_t arrayStart = colon + 1;
    while (arrayStart < json.size() && std::isspace(static_cast<unsigned char>(json[arrayStart])))
        ++arrayStart;
    if (arrayStart >= json.size() || json[arrayStart] != '[')
        return {};

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (std::size_t index = arrayStart; index < json.size(); ++index)
    {
        const char character = json[index];
        if (escaped)
        {
            escaped = false;
            continue;
        }
        if (character == '\\' && inString)
        {
            escaped = true;
            continue;
        }
        if (character == '"')
        {
            inString = !inString;
            continue;
        }
        if (inString)
            continue;
        if (character == '[')
            ++depth;
        else if (character == ']')
        {
            --depth;
            if (depth == 0)
                return json.substr(arrayStart, index - arrayStart + 1);
        }
    }
    return {};
}

inline std::vector<std::string> mapStyleJsonStringArrayField(const std::string &json, const std::string &field)
{
    const std::string arrayJson = mapStyleJsonArrayField(json, field);
    if (arrayJson.empty())
        return {};

    std::vector<std::string> values;
    for (std::size_t index = 0; index < arrayJson.size(); ++index)
    {
        if (arrayJson[index] != '"')
            continue;

        const std::size_t valueStart = index + 1;
        bool escaped = false;
        for (++index; index < arrayJson.size(); ++index)
        {
            const char character = arrayJson[index];
            if (escaped)
            {
                escaped = false;
                continue;
            }
            if (character == '\\')
            {
                escaped = true;
                continue;
            }
            if (character == '"')
            {
                values.push_back(mapStyleJsonUnescape(arrayJson.substr(valueStart, index - valueStart)));
                break;
            }
        }
    }
    return values;
}

inline std::vector<std::string> mapStyleJsonStringOrArrayField(const std::string &json, const std::string &field)
{
    std::vector<std::string> values = mapStyleJsonStringArrayField(json, field);
    if (!values.empty())
        return values;
    const std::string scalar = mapStyleJsonStringField(json, field);
    if (!scalar.empty())
        values.push_back(scalar);
    return values;
}

inline std::vector<std::string> mapStyleJsonObjectArrayEntries(const std::string &json, const std::string &field)
{
    const std::string arrayJson = mapStyleJsonArrayField(json, field);
    std::vector<std::string> entries;
    if (arrayJson.empty())
        return entries;

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    std::size_t objectStart = std::string::npos;
    for (std::size_t index = 0; index < arrayJson.size(); ++index)
    {
        const char character = arrayJson[index];
        if (escaped)
        {
            escaped = false;
            continue;
        }
        if (character == '\\' && inString)
        {
            escaped = true;
            continue;
        }
        if (character == '"')
        {
            inString = !inString;
            continue;
        }
        if (inString)
            continue;

        if (character == '{')
        {
            if (depth == 0)
                objectStart = index;
            ++depth;
        }
        else if (character == '}')
        {
            --depth;
            if (depth == 0 && objectStart != std::string::npos)
            {
                entries.push_back(arrayJson.substr(objectStart, index - objectStart + 1));
                objectStart = std::string::npos;
            }
        }
    }
    return entries;
}
