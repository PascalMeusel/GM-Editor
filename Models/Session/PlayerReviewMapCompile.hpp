#pragma once
#include "Models/Session/PlayerDisplayReviewFingerprint.hpp"
#include "Models/Map/MapRenderCompileMemo.hpp"
#include <utility>

namespace PlayerDisplayReview
{
inline MapRenderPlanRequest gmCompileRequest(const MapModel &map,const std::string &locale)
{
    MapRenderPlanRequest request;
    request.audience=MapRenderAudience::GameMaster;request.locale=locale;
    request.layers=MapRenderPlanAll;request.applyFog=false;request.showPoliticalOverlay=true;
    request.showGrid=map.atlasLevel==MapLevel::Tactical&&map.gridType==GridType::Square;
    return request;
}
inline MapRenderPlanRequest playerCompileRequest(const MapModel &map,const std::string &locale)
{
    MapRenderPlanRequest request;
    request.audience=MapRenderAudience::Player;request.locale=locale;
    request.layers=playerMapLayers(map);request.applyFog=true;
    request.showPoliticalOverlay=map.playerRelease.politicalBorders;
    request.showGrid=map.atlasLevel==MapLevel::Tactical&&map.gridType==GridType::Square;
    return request;
}
inline bool compileReviewMap(const MapModel &map,const MapRenderPlanRequest &request,
    gm::map_render::MapRenderCompileMemo *memo,bool cacheOnly,MapRenderPlan &plan,
    MapRenderCompileStatus *status)
{
    if(cacheOnly)
    {
        if(!memo){if(status)*status=MapRenderCompileStatus::Unavailable;return false;}
        auto attempt=tryCompileMapRenderPlanWithMemo(map,request,*memo);
        if(status)*status=attempt.status;
        if(!attempt.plan)return false;
        plan=std::move(*attempt.plan);return true;
    }
    plan=memo?compileMapRenderPlanWithMemo(map,request,*memo):compileMapRenderPlan(map,request);
    return true;
}
}
