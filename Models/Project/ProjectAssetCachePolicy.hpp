#pragma once

#include <string>

namespace ProjectAssetCachePolicy
{
struct AssetCacheRule
{
    std::string owner;
    bool lazyLoad = true;
    bool invalidateOnFileChange = false;
    bool releaseOnOwnerShutdown = true;
    bool retainOnlyWhileActive = false;
};

AssetCacheRule textureCacheRule();
AssetCacheRule audioCacheRule();
}
