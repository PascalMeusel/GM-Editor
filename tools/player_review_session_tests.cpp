#include "Models/Session/PlayerReviewSession.hpp"
#include "Models/Session/PlayerDisplayReviewService.hpp"
#include "Models/Session/PlayerDisplayReviewConfirmation.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Map/MapRenderCompileMemo.hpp"
#include "map_render_plan_snapshot.hpp"
#include "player_review_async_test_support.hpp"
#include <cstdlib>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace fs=std::filesystem;
namespace
{
using namespace PlayerDisplayReview;
void require(bool b,const char *message){if(!b)throw std::runtime_error(message);}
void write(const fs::path &p,const std::string &bytes)
{fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f<<bytes;require(bool(f),"fixture write failed");}
std::string read(const fs::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
std::uint64_t count(const Measurements &m,Stage s)
{std::uint64_t n=0;for(const auto &row:m.values)n+=row[std::size_t(s)].count;return n;}
Campaign fixture(const fs::path &root,bool map=true)
{
    Campaign c;c.name="Public review fixture";
    auto &d=c.localPlayerDisplay;d.mapOnly=false;d.showMap=map;d.showSharedArt=false;
    d.showInitiative=d.showVisibleNpcs=d.showParty=false;d.showPublicHandouts=true;
    HandoutModel h;h.title="PUBLIC_SNAPSHOT_TITLE";h.body="PUBLIC_SNAPSHOT_BODY";
    h.filePath="Resources/Campaigns/"+c.metadata.id+"/Assets/Handouts/approved.png";
    write(root/fs::u8path(h.filePath),"APPROVED_IMAGE_BYTES");d.publicHandoutIds={h.metadata.id};c.handouts.push_back(h);
    HandoutModel hidden;hidden.title="UNSELECTED_SECRET";hidden.body="PRIVATE_CANARY";c.handouts.push_back(hidden);
    if(map){MapModel m;m.name="Reviewed map";m.width=8;m.height=8;m.cells.resize(64);
        m.visibility=MapVisibility::PlayersVisible;m.atlasLevel=MapLevel::Tactical;
        for(auto &cell:m.cells)cell.fogged=false;
        m.generation.recipe.styleVersion=1;m.generation.recipe.styleSnapshotId.clear();
        MapGeneratedFeature f;f.id="private-feature";f.gmOnly=true;f.name="PRIVATE_FEATURE";
        f.kind=MapGeneratedFeatureKind::Road;f.points={{1,1},{6,6}};m.generation.graph.features.push_back(f);
        d.publicMapId=m.metadata.id;c.maps.push_back(m);}
    return c;
}
void parityAndIdle(const fs::path &root)
{
    for(const auto level:{MapLevel::World,MapLevel::Tactical})
    {
        auto c=fixture(root);c.maps[0].atlasLevel=level;Measurements m;
        auto full=build(c,"en",BuildPurpose::Comparison,&m,Origin::Open);
        auto player=build(c,"en",BuildPurpose::PlayerOnly,&m,Origin::PreSave);
        require(full.eligible&&player.eligible&&full.contentFingerprint==player.contentFingerprint&&
            full.playerMapPlan.contentHash==player.playerMapPlan.contentHash&&player.gmMapPlan.mapId.empty(),"Player-only builder changed canonical projection");
        require(count(m,Stage::GmCompile)==1&&count(m,Stage::PlayerCompile)==2,"Builder purpose did not omit exactly unused GM compilation");
        PlayerReviewSession owner;owner.setMeasurements(&m);require(owner.prepare(c,"en"),"Cannot prepare comparison owner");
        const auto memo=owner.compileMemo();require(memo&&!memo->empty(),"Comparison did not seed a map memo");
        const auto memoHits=memo->stats().hits;
        const auto cached=build(c,"en",BuildPurpose::PlayerOnly,nullptr,Origin::PreSave,memo.get());
        require(memo->stats().hits==memoHits+1&&cached.contentFingerprint==player.contentFingerprint&&
            render_plan_test::snapshot(cached.playerMapPlan)==render_plan_test::snapshot(player.playerMapPlan),
            "Review seed did not hit with exact canonical player output");
        const auto *gm=owner.gmMap(),*safe=owner.playerMap();const auto generation=owner.generation();
        const auto builds=count(m,Stage::Build),filters=count(m,Stage::PreviewFilter);
        require(gm&&safe&&owner.review().playerHandouts.size()==1&&owner.review().playerHandouts[0].title=="PUBLIC_SNAPSHOT_TITLE", "Comparison projection incomplete");
        require(std::none_of(safe->generation.graph.features.begin(),safe->generation.graph.features.end(),[](const auto &f){return f.id=="private-feature";}),"Private feature entered player snapshot");
        for(int frame=0;frame<300;++frame)require(owner.knownCurrent(c,"en"),"Unchanged review became stale");
        require(owner.generation()==generation&&owner.gmMap()==gm&&owner.playerMap()==safe&&count(m,Stage::Build)==builds&&
            count(m,Stage::PreviewFilter)==filters,"Idle frames compiled or replaced frozen preview");
        c.handouts[0].title="CHANGED_LIVE_TITLE";
        require(!owner.knownCurrent(c,"en")&&owner.review().playerHandouts[0].title=="PUBLIC_SNAPSHOT_TITLE", "Stale preview reread live text");
        c.handouts[0].title="PUBLIC_SNAPSHOT_TITLE";require(!owner.knownCurrent(c,"en"),"Staleness was silently reauthorized");
        require(owner.prepare(c,"en")&&owner.generation()==generation+1,"Explicit refresh did not replace owner once");
        owner.invalidate();require(owner.prepared()&&owner.stale()&&!owner.knownCurrent(c,"en")&&
            owner.review().playerHandouts[0].title=="PUBLIC_SNAPSHOT_TITLE","Authoritative invalidation lost frozen preview or reauthorized it");
        require(owner.prepare(c,"en")&&!owner.stale(),"Explicit refresh did not clear authoritative rejection");
        require(!owner.knownCurrent(c,"de"),"Locale switch retained approval");
        owner.clear();require(!owner.prepared()&&!owner.gmMap()&&!owner.knownCurrent(c,"en"),"Reset retained private owner");
    }
}
void bytesAndTransactions(const fs::path &root)
{
    auto c=fixture(root);PlayerReviewSession owner;Measurements m;owner.setMeasurements(&m);
    require(owner.prepare(c,"en"),"Cannot prepare asset owner");
    const auto memo=owner.compileMemo();require(memo&&!memo->empty(),"Asset safety fixture did not seed map memo");
    const auto startingHits=memo->stats().hits;
    const auto review=owner.review();const auto h=review.playerHandouts[0];
    auto image=owner.playerImage(h.projectRelativePath,h.contentHash);const auto source=root/fs::u8path(h.projectRelativePath);
    require(image.valid()&&image.path()!=source&&read(image.path())=="APPROVED_IMAGE_BYTES","Review did not freeze image bytes");
    const auto modified=fs::last_write_time(source);
    const auto replace=[&]{const auto sibling=source.parent_path()/"replacement.png";write(sibling,"REPLACED_IMAGE_DATA!");
        fs::last_write_time(sibling,modified);fs::remove(source);fs::rename(sibling,source);};
    replace();require(fs::file_size(source)==fs::file_size(image.path())&&fs::last_write_time(source)==modified,
        "Replacement fixture did not preserve exact size/mtime");
    require(owner.knownCurrent(c,"en")&&read(image.path())=="APPROVED_IMAGE_BYTES", "Cheap key consumed bytes or source replacement changed frozen image");
    int saves=0;
    require(armAndSave(c,review,[&](Campaign &){++saves;return true;},&m,memo.get()).status==TransactionStatus::ReviewChanged&&saves==0&&memo->stats().hits==startingHits+1,
        "Identical size/mtime source replacement bypassed canonical confirm");
    write(source,"APPROVED_IMAGE_BYTES");fs::last_write_time(source,modified);
    require(armAndSave(c,review,[&](Campaign &){++saves;return false;},&m,memo.get()).status==TransactionStatus::PersistenceFailed&&
        !c.localPlayerDisplay.enabled&&image.valid(),"Failed save mutated display or destroyed retryable preview");
    require(armAndSave(c,review,[&](Campaign &staged){++saves;staged.metadata.touch();return true;},&m,memo.get()).applied()&&saves==2,
        "Unchanged retry or metadata-only save was rejected");
    require(publishAndSave(c,review,[&](Campaign &){++saves;replace();return true;},&m,memo.get()).status==TransactionStatus::ReviewChanged&&
        !localPlayerDisplayIsPublished(c.localPlayerDisplay)&&read(image.path())=="APPROVED_IMAGE_BYTES",
        "Post-save changed file was published or replaced frozen preview");
    fs::remove(source);require(!owner.knownCurrent(c,"en")&&image.valid(),"Missing source retired stale comparison bytes");
    require(!owner.prepare(c,"en")&&image.valid()&&owner.review().contentFingerprint==review.contentFingerprint,
        "Failed refresh destroyed the previous reviewed copy");
    owner.clear();require(!image.valid()&&!fs::exists(image.path()),"Closed comparison retained generation/files");
    require(count(m,Stage::PreValidation)==4&&count(m,Stage::PostValidation)==2&&count(m,Stage::GmCompile)==2&&memo->stats().hits==startingHits+6,
        "Measured transaction boundaries/compilation counts differ");
    c=fixture(root);require(owner.prepare(c,"en"),"Cannot prepare callback lifetime fixture");
    const auto held=owner.compileMemo();const auto heldHits=held->stats().hits;
    require(armAndSave(c,owner.review(),[&](Campaign &){owner.clear();return true;},nullptr,held.get()).applied()&&!owner.prepared()&&
        held->stats().hits==heldHits+2,
        "Model transaction accessed the destroyed popup snapshot during post-save validation");
}
void geometryInvalidation(const fs::path &root)
{
    const auto original=fixture(root);
    for(int change=0;change<5;++change)
    {
        auto c=original;PlayerReviewSession owner;require(owner.prepare(c,"en"),"Cannot prepare mutation fixture");
        if(change==0)c.maps[0].cells[0].fogged=true;
        if(change==1)c.maps[0].generation.graph.features[0].points[0].x+=1;
        if(change==2)c.maps[0].playerRelease.labels=!c.maps[0].playerRelease.labels;
        if(change==3)c.maps[0].visibility=MapVisibility::GameMasterOnly;
        if(change==4)c.localPlayerDisplay.publicHandoutIds.clear();
        require(!owner.knownCurrent(c,"en")&&owner.playerMap()->cells[0].fogged==false,"No-revision mutation escaped conservative invalidation");
    }
    auto c=original;PlayerReviewSession owner;require(owner.prepare(c,"en"),"Cannot prepare canonical-only fixture");
    ++c.maps[0].generation.recipe.generatorVersion;
    require(!isCurrent(c,owner.review()),"Cheap-key omission bypassed authoritative map identity");
    c=original;c.maps[0].backgroundImagePath="Resources/unhashed-private.png";c.maps[0].cells[0].fogged=true;
    write(root/"Resources/unhashed-private.png","PRIVATE_ART");
    require(owner.prepare(c,"en")&&!owner.gmBackground().valid(),"Hashless private artwork used mutable fallback");
}
void memoCanonicalChanges(const fs::path &root)
{
    using render_plan_test::snapshot;
    auto original=fixture(root);auto &map=original.maps[0];map.atlasLevel=MapLevel::World;
    map.playerRelease.placesAndMarkers=map.playerRelease.labels=true;
    MapGeneratedFeature feature;feature.id="public-world-place";feature.kind=MapGeneratedFeatureKind::Settlement;
    feature.name="PUBLIC_WORLD_PLACE";feature.discovered=true;feature.points={{2,2}};map.generation.graph.features.push_back(feature);
    WorldPlaceModel place;place.name=feature.name;place.visibility=WorldObjectVisibility::Public;
    place.linkedMapId=map.metadata.id;place.linkedFeatureId=feature.id;original.worldPlaces.push_back(place);
    for(int change=0;change<3;++change)
    {
        auto c=original;PlayerReviewSession owner;require(owner.prepare(c,"en"),"Cannot prepare canonical mutation seed");
        auto memo=owner.compileMemo();require(memo&&!memo->empty(),"Canonical mutation memo seed absent");
        const auto review=owner.review();const auto prior=snapshot(review.playerMapPlan);
        require(prior.find("PUBLIC_WORLD_PLACE")!=std::string::npos,"World policy positive control absent");
        const auto mutate=[&](Campaign &target){
            if(change==0)target.maps[0].generation.graph.features.back().points[0].x=3;
            else if(change==1)target.worldPlaces[0].visibility=WorldObjectVisibility::GmOnly;
            else target.handouts[0].body="CHANGED_PUBLIC_PAYLOAD";
        };
        auto changed=c;mutate(changed);int saves=0;const auto hits=memo->stats().hits;
        require(armAndSave(changed,review,[&](Campaign &){++saves;return true;},nullptr,memo.get()).status==TransactionStatus::ReviewChanged&&saves==0,
            "No-revision map/campaign/payload change passed canonical pre-save memo check");
        if(change==2)require(memo->stats().hits==hits+1,"Non-map canonical rejection did not exercise a real map hit");
        // Re-seed exact reviewed map, then mutate only the staged save result.
        require(owner.prepare(c,"en"),"Cannot re-seed post-save mutation fixture");memo=owner.compileMemo();const auto seededHits=memo->stats().hits;
        require(armAndSave(c,owner.review(),[&](Campaign &staged){++saves;mutate(staged);return true;},nullptr,memo.get()).status==TransactionStatus::ReviewChanged&&
            saves==1&&!c.localPlayerDisplay.enabled&&!c.localPlayerDisplay.published&&
            memo->stats().hits>=seededHits+1&&snapshot(owner.review().playerMapPlan)==prior,
            "Post-save memo reused changed geometry/policy/payload or altered frozen review");
        require(c.maps[0].generation.graph.features.back().points[0].x==2&&c.worldPlaces[0].visibility==WorldObjectVisibility::Public&&
            c.handouts[0].body=="PUBLIC_SNAPSHOT_BODY","Rejected staged mutation replaced live source");
    }
}
#include "player_review_confirmation_cases.hpp"
#include "player_review_async_cases.hpp"
}
int main()
{
    const auto root=fs::temp_directory_path()/("gm-review-session-test-"+AtomicProjectFile::uniqueSuffix());
    try
    {
        fs::create_directories(root);
#ifdef _WIN32
        require(_putenv_s("TTRPG_DATA_ROOT",root.u8string().c_str())==0,"Cannot isolate data");
#else
        require(setenv("TTRPG_DATA_ROOT",root.u8string().c_str(),1)==0,"Cannot isolate data");
#endif
        parityAndIdle(root);bytesAndTransactions(root);geometryInvalidation(root);memoCanonicalChanges(root);
        compactConfirmationIdentity(root);compactConfirmationCallbackLifetime(root);asyncReviewCases(root);
        fs::remove_all(root);std::cout<<"Player review owner: purpose parity, 300-frame immutable reuse, stale/reset/assets, failure/retry and canonical pre/post-save checks passed.\n";return 0;
    }
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';std::error_code ec;fs::remove_all(root,ec);return 1;}
}
