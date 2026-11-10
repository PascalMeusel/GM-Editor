#include "ProjectAssetCachePolicy.hpp"

namespace ProjectAssetCachePolicy
{
AssetCacheRule textureCacheRule()
{
    AssetCacheRule rule;
    rule.owner = "Views/Editor/Widgets/ImageTextureCache";
    rule.lazyLoad = true;
    rule.invalidateOnFileChange = true;
    rule.releaseOnOwnerShutdown = true;
    rule.retainOnlyWhileActive = false;
    return rule;
}

AssetCacheRule audioCacheRule()
{
    AssetCacheRule rule;
    rule.owner = "Models/Audio/AudioPlaybackService";
    rule.lazyLoad = true;
    rule.invalidateOnFileChange = false;
    rule.releaseOnOwnerShutdown = true;
    rule.retainOnlyWhileActive = true;
    return rule;
}
}
