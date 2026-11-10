#pragma once
#include "Models/Session/PlayerReviewSession.hpp"
#include <functional>
struct CachedImageTexture;
namespace gm::player_review_ui
{
using HandoutTextureLookup=std::function<const CachedImageTexture &(const FrozenImageAsset &)>;
// Reserves the same layout before loading, after loading, and when unavailable.
// The caller supplies only a frozen-asset lookup; no mutable path fallback.
void drawHandoutImage(const FrozenImageAsset &asset,const HandoutTextureLookup &lookup);
void drawMaps(const PlayerReviewSession &session);
void drawContent(const PlayerReviewSession &session);
}
