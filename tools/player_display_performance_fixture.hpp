#pragma once
#include "player_display_performance_metrics.hpp"
#include "Models/Media/RgbaImageLoader.hpp"
#include "Models/Map/CurrentMapArtRecipe.hpp"
#include "Models/Map/MapGeneration.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectAssetImportService.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "Models/Session/PlayerRecapPackage.hpp"
#include <array>
#include <cstdlib>
#include <optional>
#include <iterator>
#include <string_view>

namespace player_perf
{
using Color = std::array<unsigned char, 4>;
inline constexpr std::array<Color,4> colors{{{{255,0,0,255}},{{0,255,0,255}},{{0,0,255,255}},{{255,255,0,255}}}};
struct Environment
{
    std::map<std::string,std::optional<std::string>> prior;
    void set(const std::string &key, const std::string &value)
    {
        const char *old = std::getenv(key.c_str()); prior[key] = old ? std::optional<std::string>(old) : std::nullopt;
#ifdef _WIN32
        require(_putenv_s(key.c_str(), value.c_str()) == 0, "Cannot isolate " + key);
#else
        require(setenv(key.c_str(), value.c_str(), 1) == 0, "Cannot isolate " + key);
#endif
    }
    ~Environment()
    {
        for (const auto &[key,value] : prior)
        {
#ifdef _WIN32
            _putenv_s(key.c_str(), value ? value->c_str() : "");
#else
            if (value) setenv(key.c_str(), value->c_str(), 1); else unsetenv(key.c_str());
#endif
        }
    }
};
struct Fixture
{
    Campaign campaign;
    std::filesystem::path source, replacement, atomicReplacement, sentinel;
    EntityId handoutId, sessionId;
    std::filesystem::file_time_type originalTime;
    std::uintmax_t encodedBytes = 0;
    std::string originalHash, replacementHash;
};
inline LoadedRgbaImage image(int side, int permutation)
{
    LoadedRgbaImage result; result.width = result.height = side;
    result.pixels.resize(static_cast<std::size_t>(side) * side * 4);
    for (int y=0; y<side; ++y) for (int x=0; x<side; ++x)
    {
        const auto &color = colors[((y >= side/2 ? 2 : 0) + (x >= side/2 ? 1 : 0) + permutation) % 4];
        std::copy(color.begin(),color.end(),result.pixels.begin() + (static_cast<std::size_t>(y)*side+x)*4);
    }
    return result;
}
inline void validateImage(const std::filesystem::path &path, const LoadedRgbaImage &expected)
{
    LoadedRgbaImage decoded; std::string error;
    const bool loaded=loadRgbaImage(path, decoded, RgbaImageLoadOptions{4096,4096ull*4096,true}, &error);
    require(loaded, "Fixture decode: " + error);
    require(decoded.width == expected.width && decoded.height == expected.height && decoded.pixels == expected.pixels,
        "Generated PNG did not decode to exact expected RGBA pixels");
}
inline void writePng(const std::filesystem::path &path,const LoadedRgbaImage &image)
{
    std::string error; const bool saved=saveRgbaImageAsPng(path,image,&error);
    require(saved,"Fixture encode "+path.u8string()+": "+error);
}
inline std::uint32_t pngCrc(std::string_view bytes)
{
    std::uint32_t crc=0xffffffffu;
    for(unsigned char value:bytes)
    {
        crc^=value;
        for(int bit=0;bit<8;++bit) crc=(crc>>1)^((crc&1u) ? 0xedb88320u : 0u);
    }
    return crc^0xffffffffu;
}
inline void appendPngU32(std::string &bytes,std::uint32_t value)
{
    for(int shift=24;shift>=0;shift-=8) bytes.push_back(static_cast<char>((value>>shift)&255));
}
inline std::size_t pngEndOffset(const std::string &bytes)
{
    require(bytes.size()>=20 && bytes.compare(0,8,"\x89PNG\r\n\x1a\n",8)==0,"Fixture PNG signature invalid");
    const auto u32=[&](std::size_t offset) {
        require(offset+4<=bytes.size(),"Fixture PNG integer truncated"); std::uint32_t value=0;
        for(int i=0;i<4;++i) value=(value<<8)|static_cast<unsigned char>(bytes[offset+i]); return value;
    };
    bool imageData=false;
    for(std::size_t offset=8;offset+12<=bytes.size();)
    {
        const auto length=u32(offset);
        require(length<=0x7fffffffu && length<=bytes.size()-offset-12,"Fixture PNG chunk length invalid");
        const auto type=bytes.substr(offset+4,4);
        require(offset!=8 || (type=="IHDR" && length==13),"Fixture PNG header invalid");
        require(pngCrc(std::string_view(bytes).substr(offset+4,length+4))==u32(offset+8+length),"Fixture PNG chunk CRC invalid");
        if(type=="IDAT") imageData=true;
        if(type=="IEND")
        {
            require(imageData && length==0 && offset+12==bytes.size(),"Fixture PNG end/trailing bytes invalid");
            return offset;
        }
        offset+=std::size_t(length)+12;
    }
    throw std::runtime_error("Fixture PNG has no complete IEND");
}
inline void equalizePngLength(const std::filesystem::path &path,std::uintmax_t targetBytes)
{
    std::ifstream input(path,std::ios::binary);
    require(bool(input),"Cannot read PNG for ancillary padding");
    std::string bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    require(!input.bad(),"Cannot finish PNG ancillary padding read"); input.close();
    const auto end=pngEndOffset(bytes);
    require(targetBytes>=bytes.size()+20 && targetBytes<=256ull*1024*1024,"Invalid bounded PNG fixture padding size");
    // Standard tEXt chunk: Latin-1 keyword + NUL + text, before IEND.
    // This preserves image data and is deliberately not trailing garbage.
    std::string data="Comment"; data.push_back('\0');
    data.append(static_cast<std::size_t>(targetBytes-bytes.size()-20),'P');
    std::string chunk; appendPngU32(chunk,static_cast<std::uint32_t>(data.size()));
    const auto payload=std::string("tEXt")+data; chunk+=payload; appendPngU32(chunk,pngCrc(payload));
    bytes.insert(end,chunk);
    require(bytes.size()==targetBytes && pngEndOffset(bytes)+12==bytes.size(),"Padded PNG structural validation failed");
    std::ofstream output(path,std::ios::binary|std::ios::trunc); output.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    output.close(); require(bool(output),"Could not finish padded PNG fixture");
}
inline Fixture makeFixture(const std::filesystem::path &root, Metrics &metrics)
{
    Fixture f; f.campaign.metadata.id = "player-performance"; f.campaign.name = "Player performance fixture";
    f.campaign.description="DURABILITY_INITIAL";
    f.campaign.ruleSet = std::make_shared<DND5e>();
    MapGenerationOptions options; options.width=options.height=80; options.seed=612345; options.qualityAttempts=1;
    options.generatorVersion=11; options.recipeVersion=kCurrentMapIllustrationRecipe; options.styleId="illustrated_atlas";
    options.scale=MapLevel::World; options.scope=MapScope::World; options.creationIntent=MapCreationIntent::WorldOverview;
    options.contentLanguage="en";
    metrics.time("fixture_map_generation",-1,[&] { f.campaign.maps.push_back(generateSemanticMap(options).map); });
    auto &map=f.campaign.maps.front(); map.name="Public 80x80 world"; map.visibility=MapVisibility::PlayersVisible;
    map.playerRelease={true,true,true,true,true}; for(auto &cell:map.cells) cell.fogged=false;
    MapGeneratedFeature privateRoad;privateRoad.id="native-private-review-road";privateRoad.name="GM_ONLY_REVIEW_CANARY";
    privateRoad.kind=MapGeneratedFeatureKind::Road;privateRoad.gmOnly=true;privateRoad.width=2;
    privateRoad.points={{10,10},{30,30}};map.generation.graph.features.push_back(privateRoad);
    require(map.width==80 && map.height==80 && !map.importedReference.enabled && map.backgroundImagePath.empty(), "Unexpected map workload");
    f.source=root/"data/Resources/Campaigns/player-performance/Assets/Handouts/approved.png";
    f.replacement=root/"replacement.png"; f.sentinel=root/"ordinary-sentinel.png";
    std::filesystem::create_directories(f.source.parent_path());
    metrics.time("fixture_png_pair_prepare_validate",-1,[&] {
        const auto original=image(4096,0), changed=image(4096,1);
        writePng(f.source,original); writePng(f.replacement,changed);
        const auto originalBytes=std::filesystem::file_size(f.source), changedBytes=std::filesystem::file_size(f.replacement);
        metrics.facts["original_unpadded_bytes"]=std::to_string(originalBytes);
        metrics.facts["replacement_unpadded_bytes"]=std::to_string(changedBytes);
        f.encodedBytes=std::max(originalBytes,changedBytes)+20;
        equalizePngLength(f.source,f.encodedBytes); equalizePngLength(f.replacement,f.encodedBytes);
        validateImage(f.source,original); validateImage(f.replacement,changed);
        require(original.pixels!=changed.pixels && std::filesystem::file_size(f.source)==f.encodedBytes &&
            std::filesystem::file_size(f.replacement)==f.encodedBytes,"Validated PNG pair does not have equal sizes and changed pixels");
        metrics.facts["replacement_quadrant_permutation"]="1";
        metrics.facts["png_equal_length_method"]="Production PNG encode; CRC-validated standard tEXt ancillary chunk before IEND; exact production RGBA decode after padding";
        std::filesystem::copy_file(f.source,root/"original.png");
        f.originalHash=ProjectAssetImport::computeContentHash(f.source);
        f.replacementHash=ProjectAssetImport::computeContentHash(f.replacement);
        require(!f.originalHash.empty() && f.originalHash!=f.replacementHash,"PNG pair hashes did not differ");
        writePng(f.sentinel,image(8,0));
    });
    f.originalTime=std::filesystem::last_write_time(f.source);
    f.atomicReplacement=f.source.parent_path()/"replacement.prepared.png";
    std::filesystem::copy_file(f.replacement,f.atomicReplacement);
    std::filesystem::last_write_time(f.atomicReplacement,f.originalTime);
    HandoutModel handout; handout.title="Synthetic image";
    handout.filePath=std::filesystem::relative(f.source,root/"data").generic_u8string(); f.handoutId=handout.metadata.id;
    f.campaign.handouts.push_back(handout);
    SessionModel session; session.title="Fixture session"; f.sessionId=session.metadata.id; f.campaign.sessions.push_back(session);
    SessionOutcomes::Outcome outcome; outcome.publicText="Synthetic public outcome."; outcome.privateText="Fixture private note"; outcome.shareWithPlayers=true;
    require(SessionOutcomes::capture(f.campaign,f.sessionId,outcome,[](Campaign &){return true;}).success,"Cannot prepare recap outcome");
    const auto applyReview=SessionOutcomes::reviewApply(f.campaign,f.sessionId,{outcome.id});
    require(SessionOutcomes::apply(f.campaign,applyReview,[](Campaign &){return true;}).success,"Cannot accept recap outcome");
    auto &d=f.campaign.localPlayerDisplay; d.mapOnly=false; d.fullscreenMapOnly=false; d.publicMapId=f.campaign.maps.front().metadata.id;
    d.showMap=d.showPublicHandouts=true; d.showInitiative=d.showVisibleNpcs=d.showParty=d.showPartyInventory=d.showSharedArt=false;
    d.publicHandoutIds={f.handoutId}; d.playerTextScale=1; d.publicCamera.followGmCamera=false;
    metrics.facts["map"]= "80x80 world, seed 612345, generator11, recipe"+std::to_string(options.recipeVersion);
    metrics.facts["png_dimensions"]="4096x4096 RGBA"; metrics.facts["png_encoded_bytes"]=std::to_string(f.encodedBytes);
    metrics.facts["png_original_fnv1a64"]=f.originalHash; metrics.facts["png_replacement_fnv1a64"]=f.replacementHash;
    metrics.facts["png_sha256"]="Recorded by launch-wrapper manifest outside timing";
    return f;
}
inline void replaceSource(Fixture &fixture,const std::filesystem::path &overrideSibling={})
{
    const auto &sibling=overrideSibling.empty()?fixture.atomicReplacement:overrideSibling;
    require(std::filesystem::file_size(sibling)==fixture.encodedBytes &&
        std::filesystem::last_write_time(sibling)==fixture.originalTime,
        "Atomic replacement sibling lost its validated size/mtime");
#ifdef _WIN32
    const auto from=mapFileIoPath(sibling), to=mapFileIoPath(fixture.source);
    if(!MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::system_error(GetLastError(),std::system_category(),"Atomic same-metadata fixture replacement");
#else
    std::filesystem::rename(sibling,fixture.source);
#endif
    require(std::filesystem::file_size(fixture.source)==fixture.encodedBytes &&
        std::filesystem::last_write_time(fixture.source)==fixture.originalTime,"Atomic replacement did not preserve exact size/mtime");
}
}
