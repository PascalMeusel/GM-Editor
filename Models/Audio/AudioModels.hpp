#pragma once

#include "Models/Core/EntityMetadata.hpp"
#include <string>
#include <vector>

enum class AudioAssetType
{
    Music,
    Ambience,
    Effect,
    Voice,
    Other
};

struct AudioAssetModel
{
    EntityMetadata metadata{"audio"};
    std::string name;
    std::string originalFileName;
    std::string originalSourcePath;
    std::string filePath;
    AudioAssetType type = AudioAssetType::Other;
    float defaultVolume = 1.0f;
    bool loop = false;
    int durationSeconds = 0;
    bool missingFile = false;
    std::string lastPlayedAtUtc;
};

struct AudioPlaylistModel
{
    EntityMetadata metadata{"playlist"};
    std::string name;
    std::vector<EntityId> audioAssetIds;
    bool shuffle = false;
    bool repeat = false;
    bool oneShot = false;
    float crossfadeSeconds = 0.0f;
};

struct AudioAmbienceLayerModel
{
    EntityMetadata metadata{"ambience_layer"};
    std::string name;
    EntityId audioAssetId;
    float volume = 0.35f;
    bool loop = true;
    bool active = false;
    int sortOrder = 0;
};

struct AudioTriggerModel
{
    EntityMetadata metadata{"audio_trigger"};
    std::string name;
    std::string sourceType = "StoryBeat";
    EntityId sourceEntityId;
    std::string eventName = "Scene Started";
    EntityId audioAssetId;
    float volume = 1.0f;
    bool loop = false;
    bool enabled = true;
};

struct AudioVolumeGroupsModel
{
    float master = 1.0f;
    float music = 1.0f;
    float ambience = 1.0f;
    float effects = 1.0f;
    float voice = 1.0f;
};

inline const char *audioAssetTypeLabel(AudioAssetType type)
{
    switch (type)
    {
    case AudioAssetType::Music:
        return "Music";
    case AudioAssetType::Ambience:
        return "Ambience";
    case AudioAssetType::Effect:
        return "Sound Effect";
    case AudioAssetType::Voice:
        return "Voice";
    case AudioAssetType::Other:
        return "Other";
    }
    return "Other";
}
