#include "map_render_plan_workload.hpp"
#include "Models/Map/MapRenderPlanMeasurements.hpp"
#include "Models/Map/MapRenderCompileMemo.hpp"
#include "Models/Map/MapPackageService.hpp"
#include "Models/Map/MapFileIoPath.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// No application/controller, ImGui, GLFW, audio, database or parallel workers.
// --prepare is the only generation path. Benchmark runs read those exact map
// packages with remapping disabled; they never regenerate their inputs.
namespace {
using namespace render_plan_test;
namespace fs=std::filesystem;
using Files=std::map<std::string,std::string>;
void ordinary(const fs::path &path) {
    for(auto p=fs::absolute(path);!p.empty();) {
        require(fs::exists(mapFileIoPath(p))&&!fs::is_symlink(mapFileIoPath(p)),"Missing/linked path: "+p.u8string());
#ifdef _WIN32
        const auto attributes=GetFileAttributesW(mapFileIoPath(p).c_str());
        require(attributes!=INVALID_FILE_ATTRIBUTES&&!(attributes&FILE_ATTRIBUTE_REPARSE_POINT),"Reparse path: "+p.u8string());
#endif
        const auto parent=p.parent_path();if(parent==p)break;p=parent;
    }
}
void freshDirectory(const fs::path &path) {
    require(path.is_absolute()&&!fs::exists(mapFileIoPath(path)),"Output must be a new absolute directory: "+path.u8string());
    ordinary(path.parent_path());require(fs::create_directory(mapFileIoPath(path)),"Cannot reserve output directory");
}
std::string read(const fs::path &path) {
    ordinary(path);require(fs::is_regular_file(mapFileIoPath(path)),"Not a regular file: "+path.u8string());
    const auto size=fs::file_size(mapFileIoPath(path));require(size<=512ull*1024*1024,"Artifact exceeds bounded read");
    std::ifstream in(mapFileIoPath(path),std::ios::binary);require(bool(in),"Cannot open artifact");
    std::string result{std::istreambuf_iterator<char>(in),{}};
    require(!in.bad()&&result.size()==size,"Artifact changed/truncated while read");return result;
}
void writeNew(const fs::path &path,const std::string &bytes) {
    ordinary(path.parent_path());require(!fs::exists(mapFileIoPath(path)),"Refusing artifact overwrite: "+path.u8string());
    std::ofstream out(mapFileIoPath(path),std::ios::binary);out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));out.close();
    require(bool(out),"Artifact write failed: "+path.u8string());
}
std::string fingerprint(const std::string &bytes) {
    std::uint64_t value=14695981039346656037ull;for(unsigned char c:bytes){value^=c;value*=1099511628211ull;}
    std::ostringstream out;out<<std::hex<<std::setfill('0')<<std::setw(16)<<value;return out.str();
}
Files inventory(const fs::path &root) {
    ordinary(root);require(fs::is_directory(mapFileIoPath(root)),"Not an artifact directory");Files files;
    for(const auto &entry:fs::directory_iterator(mapFileIoPath(root))) {
        require(entry.is_regular_file(),"Unexpected artifact child directory/type");
        const auto name=entry.path().filename().u8string();files.emplace(name,read(root/fs::u8path(name)));
    }return files;
}
std::set<std::string> fixtureNames() { return {"cave","classic","control","dungeon","region","settlement","tactical","travel","world","world80"}; }
void prepare(const fs::path &root) {
    freshDirectory(root);const auto fixtures=makeFixtures(true);std::ostringstream manifest;manifest<<"GM_RENDER_FIXTURES_V1\n";
    MapPackageExportOptions options;options.includeBackgroundAsset=options.includeImportedAsset=options.includeWorkingProxyAsset=false;
    for(const auto &[name,map]:fixtures) {
        const auto path=root/(name+".mapfixture");const auto result=MapPackageService::exportPackage(map,path,options);
        require(result.success,"Cannot persist fixture "+name+": "+result.error);
        const auto bytes=read(path);manifest<<name<<'\t'<<bytes.size()<<'\t'<<fingerprint(bytes)<<'\n';
    }
    writeNew(root/"fixtures.tsv",manifest.str());
    std::cout<<"Prepared "<<fixtures.size()<<" exact map packages; world80 seed612345, 80x80. No benchmark run.\n";
}
Fixtures loadFixtures(const fs::path &root,const Files &files) {
    const auto found=files.find("fixtures.tsv");require(found!=files.end(),"Missing fixtures.tsv");
    std::istringstream manifest(found->second);std::string version;std::getline(manifest,version);
    require(version=="GM_RENDER_FIXTURES_V1","Unsupported fixture format");Fixtures maps;std::set<std::string> names;
    std::string line;while(std::getline(manifest,line)) {
        std::istringstream row(line);std::string name,size,digest,extra;
        require(bool(std::getline(row,name,'\t'))&&bool(std::getline(row,size,'\t'))&&bool(std::getline(row,digest,'\t'))&&
            !std::getline(row,extra,'\t'),"Malformed fixture manifest row");
        require(fixtureNames().count(name)&&names.insert(name).second,"Unknown/duplicate fixture name");
        const auto file=files.find(name+".mapfixture");require(file!=files.end(),"Fixture file absent");
        require(std::to_string(file->second.size())==size&&fingerprint(file->second)==digest,"Fixture input fingerprint differs: "+name);
        MapPackageImportOptions options;options.standaloneCopy=false;options.remapEntityIds=false;
        auto loaded=MapPackageService::importPackage(root/(name+".mapfixture"),options);
        require(loaded.success&&loaded.assets.empty()&&loaded.idRemapping.empty(),"Exact fixture decode failed: "+name+": "+loaded.error);
        maps.emplace(name,std::move(loaded.map));
    }
    require(names==fixtureNames()&&files.size()==names.size()+1,"Fixture directory membership mismatch");
    const auto &large=maps.at("world80");require(large.width==80&&large.height==80&&large.generation.recipe.seed==612345&&
        large.generation.recipe.generatorVersion==11,"80x80 baseline recipe identity changed");return maps;
}
std::string caseDescription(const Case &test,const MapRenderPlanRequest &r) {
    std::ostringstream out;out<<test.name<<'\t'<<test.fixture<<'\t'<<static_cast<int>(r.audience)<<'\t'<<r.locale<<'\t'<<r.layers<<'\t'
        <<r.applyFog<<'\t'<<r.showPoliticalOverlay<<'\t'<<r.showGrid<<'\t'<<static_cast<int>(test.variation)<<'\n';return out.str();
}
std::string callsBytes(const std::vector<std::string> &calls) { SnapshotWriter out;out.strings(calls);return out.take(); }
void compare(const fs::path &baseline,const fs::path &output) {
    const auto left=inventory(baseline),right=inventory(output);
    const auto canonical=[](const Files &all){Files selected;for(const auto &[name,bytes]:all)
        if(fs::path(name).extension()==".plan"||fs::path(name).extension()==".calls"||name=="cases.tsv"||name=="input-fixtures.tsv")selected.emplace(name,bytes);
        return selected;};
    const auto a=canonical(left),b=canonical(right);require(!a.empty()&&a.size()==b.size(),"Baseline artifact membership differs");
    for(const auto &[name,bytes]:a) {
        const auto found=b.find(name);require(found!=b.end(),"Missing optimized artifact: "+name);
        if(found->second!=bytes) {
            const auto mismatch=std::mismatch(bytes.begin(),bytes.end(),found->second.begin(),found->second.end());
            throw std::runtime_error("Byte parity failed: "+name+" offset="+std::to_string(std::distance(bytes.begin(),mismatch.first))+
                " baseline_bytes="+std::to_string(bytes.size())+" current_bytes="+std::to_string(found->second.size()));
        }
    }
    writeNew(output/"comparison.txt","PASS exact artifact membership and complete bytes; "+std::to_string(a.size())+
        " artifacts. Timing files excluded. Baseline: "+baseline.u8string()+"\n");
}
void run(const fs::path &fixturesRoot,const fs::path &output,int samples,const fs::path &baseline,bool useMemo) {
    ordinary(fixturesRoot);const auto before=inventory(fixturesRoot);const auto fixtures=loadFixtures(fixturesRoot,before);
    freshDirectory(output);writeNew(output/"input-fixtures.tsv",before.at("fixtures.tsv"));
    std::ostringstream descriptors,metrics,artifacts;descriptors<<"name\tfixture\taudience\tlocale\tlayers\tapplyFog\tpolitical\tgrid\tvariation\n";
    metrics<<"case,sample,stage,nanoseconds,calls\n";artifacts<<"file\tbytes\tfnv1a64_diagnostic\n";
    std::size_t count=0;std::ostringstream memoStats;memoStats<<"case,sample,hits,misses,bypasses\n";
    for(const auto &test:cases(fixtures)) {
        const auto map=source(fixtures,test);std::vector<std::string> callbacks;
        auto r=request(test);descriptors<<caseDescription(test,r);
        // Compile and record the ordinary production path outside timing. This
        // is also the single warmup and exact-output oracle for measured calls.
        const auto ordinaryPlan=compileMapRenderPlan(map,request(test,&callbacks));const auto expected=snapshot(ordinaryPlan),expectedCalls=callsBytes(callbacks);
        gm::map_render::MapRenderCompileMemo memo;
        if(useMemo)require(snapshot(compileMapRenderPlanWithMemo(map,r,memo))==expected&&!memo.empty()&&memo.stats().misses==1,
            "Cannot seed exact bounded memo outside timing: "+test.name);
        for(const auto &artifact:std::vector<std::pair<std::string,std::string>>{{test.name+".plan",expected},{test.name+".calls",expectedCalls}}) {
            writeNew(output/artifact.first,artifact.second);artifacts<<artifact.first<<'\t'<<artifact.second.size()<<'\t'<<fingerprint(artifact.second)<<'\n';
        }
        for(int sample=0;sample<samples;++sample) {
            MapRenderPlanMeasurements measured;
            const auto hits=memo.stats().hits;
            const auto plan=useMemo?compileMapRenderPlanWithMemo(map,r,memo,&measured):compileMapRenderPlanMeasured(map,r,measured);
            require(snapshot(plan)==expected,"Repeated measured result changed: "+test.name);
            if(useMemo) {
                require(memo.stats().hits==hits+1&&memo.stats().misses==1&&memo.stats().bypasses==0,
                    "Timed memo sample was not an admitted HIT: "+test.name);
                memoStats<<test.name<<','<<sample<<','<<memo.stats().hits<<','<<memo.stats().misses<<','<<memo.stats().bypasses<<'\n';
            }
            for(std::size_t stage=0;stage<measured.values.size();++stage)
                metrics<<test.name<<','<<sample<<','<<mapRenderPlanCompileStageName(static_cast<MapRenderPlanCompileStage>(stage))<<','
                    <<measured.values[stage].nanoseconds<<','<<measured.values[stage].calls<<'\n';
        }
        ++count;std::cout<<"Completed "<<test.name<<" bytes="<<expected.size()<<'\n';
    }
    require(inventory(fixturesRoot)==before,"Persisted fixture bytes/membership changed during run");
    writeNew(output/"cases.tsv",descriptors.str());writeNew(output/"samples.csv",metrics.str());writeNew(output/"artifacts.tsv",artifacts.str());
    if(useMemo)writeNew(output/"memo-stats.csv",memoStats.str());
    if(!baseline.empty())compare(baseline,output);
    writeNew(output/"summary.txt","PASS CPU compilation parity and immutable input check\nmode="+std::string(useMemo?"hot-exact-input-memo":"ordinary-compilation")+
        "\n"+(useMemo?"Memo seeded outside timing; every timed call asserted an actual hit. This measures hot reuse, not cold compilation.\n":"")+
        "cases="+std::to_string(count)+"\nsamples_per_case="+std::to_string(samples)+
        "\nfixture_directory="+fixturesRoot.u8string()+"\nTotal is inclusive; other scopes are exclusive.\nOne ordinary warmup with callback trace per case. Timed calls have no diagnostic callback; only policy-deny retains its pure policy predicate.\nFixture generation, package decode, per-case input copies, plan serialization and artifact IO are outside timing. Compiler-internal model copies remain timed.\n"
        "Player cases compile raw MapModel through built-in render-plan safety; they do not measure builder playerSafeMapSource or campaign-linked World visibility policy.\n"
        "FNV is diagnostic only; root must independently SHA256-bind binaries, source and fixture/output inventories.\nNo publication/save, image decode/upload, GL/frame, audio, physical input or cold-start acceptance is claimed.\n");
    std::cout<<"PASS "<<count<<" cases x "<<samples<<" measured samples; CPU compilation only\n";
}
}
int main(int argc,char **argv) {
    try {
        fs::path prepareRoot,fixtures,output,baseline;int samples=5;bool useMemo=false;
        for(int i=1;i<argc;++i) {
            const std::string flag=argv[i];if(flag=="--memo"){require(!useMemo,"Duplicate --memo");useMemo=true;continue;}
            require(i+1<argc,"Missing argument value: "+flag);const std::string value=argv[++i];
            if(flag=="--prepare")prepareRoot=fs::u8path(value);else if(flag=="--fixtures")fixtures=fs::u8path(value);
            else if(flag=="--output")output=fs::u8path(value);else if(flag=="--compare")baseline=fs::u8path(value);
            else if(flag=="--samples"){require(value.size()<=2&&value.find_first_not_of("0123456789")==std::string::npos,"Invalid sample count");samples=std::stoi(value);}
            else throw std::runtime_error("Unknown option: "+flag);
        }
        require(samples>=1&&samples<=20,"Samples must be 1..20");
        if(!prepareRoot.empty()){require(fixtures.empty()&&output.empty()&&baseline.empty()&&!useMemo,"Prepare cannot be mixed with run arguments");prepare(prepareRoot);}
        else {require(fixtures.is_absolute()&&output.is_absolute()&&(baseline.empty()||baseline.is_absolute()),
            "Usage: --prepare <new absolute dir> OR --fixtures <existing absolute dir> --output <new absolute dir> [--compare <baseline dir>] [--samples 1..20] [--memo]");run(fixtures,output,samples,baseline,useMemo);}
        return 0;
    }catch(const std::exception &error){std::cerr<<"FAIL CPU render benchmark: "<<error.what()<<'\n';return 1;}
}
