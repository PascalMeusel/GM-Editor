#pragma once
#include "Models/Media/FrozenImageDecodeWorker.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace gm::frozen_decode
{
inline constexpr std::uintmax_t maximumEncodedBytes=256ull*1024*1024;
inline bool cancelled(const FrozenImageAsset &asset,const std::atomic<bool> &cancel)
{ return cancel.load() || !asset.valid(); }
inline void ordinary(const std::filesystem::path &path,bool directory)
{
    const auto status=std::filesystem::symlink_status(mapFileIoPath(path));
    if (std::filesystem::is_symlink(status) ||
        (directory?!std::filesystem::is_directory(status):!std::filesystem::is_regular_file(status)))
        throw std::runtime_error("Frozen image storage is unavailable.");
#if defined(_WIN32)
    const auto attributes=GetFileAttributesW(mapFileIoPath(path).c_str());
    if (attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::runtime_error("Frozen image storage is redirected.");
#endif
}
inline bool read(const FrozenImageAsset &asset,const FrozenImageStorage &storage,
                 const std::atomic<bool> &cancel,std::vector<unsigned char> &bytes)
{
    if (cancelled(asset,cancel)) return false;
    if (!storage.contains(asset.path())) throw std::runtime_error("Frozen image has no owned storage.");
    ordinary(storage.directory(),true);
    ordinary(asset.path(),false);
    const auto size=std::filesystem::file_size(mapFileIoPath(asset.path()));
    if (!size || size>maximumEncodedBytes) throw std::runtime_error("Frozen image exceeds the encoded byte limit.");
    bytes.resize(static_cast<std::size_t>(size));
    std::ifstream input(mapFileIoPath(asset.path()),std::ios::binary);
    if (!input) throw std::runtime_error("Frozen image could not be read.");
    std::uint64_t hash=14695981039346656037ull;
    for (std::size_t offset=0;offset<bytes.size();)
    {
        if (cancelled(asset,cancel)) return false;
        const auto count=std::min<std::size_t>(65536,bytes.size()-offset);
        if (!input.read(reinterpret_cast<char *>(bytes.data()+offset),static_cast<std::streamsize>(count)))
            throw std::runtime_error("Frozen image changed during read.");
        for (std::size_t i=0;i<count;++i) {hash^=bytes[offset+i];hash*=1099511628211ull;}
        offset+=count;
    }
    if (input.peek()!=std::char_traits<char>::eof() || input.bad())
        throw std::runtime_error("Frozen image changed during read.");
    std::ostringstream expected;
    expected<<"fnv1a64:"<<std::hex<<std::setw(16)<<std::setfill('0')<<hash;
    if (expected.str()!=asset.contentHash()) throw std::runtime_error("Frozen image no longer matches reviewed bytes.");
    return !cancelled(asset,cancel);
}
class Measure final
{
public:
    Measure(bool enabled,std::uint64_t &calls,double &milliseconds):enabled_(enabled),elapsed_(milliseconds)
    { if(enabled_){++calls;start_=Clock::now();} }
    ~Measure() {if(enabled_)elapsed_+=std::chrono::duration<double,std::milli>(Clock::now()-start_).count();}
private:
    using Clock=std::chrono::steady_clock;
    bool enabled_;double &elapsed_;Clock::time_point start_{};
};
inline void decode(const FrozenImageAsset &asset,const FrozenImageStorage &storage,
                   const std::atomic<bool> &cancel,const FrozenImageDecodeOptions &options,
                   FrozenImageDecodeResult &result)
{
    if (options.phaseHook) options.phaseHook(result.ticket,FrozenImageDecodePhase::BeforeRead);
    std::vector<unsigned char> encoded;
    {
        Measure timing(options.measure,result.measurements.hashReads,result.measurements.hashMilliseconds);
        if (!read(asset,storage,cancel,encoded)) return;
    }
    if (options.phaseHook) options.phaseHook(result.ticket,FrozenImageDecodePhase::BeforeDecode);
    if (cancelled(asset,cancel)) return;
    RgbaImageLoadOptions limits;
    limits.maximumSourceEdge=16384;limits.maximumSourcePixels=100000000;
    {
        Measure timing(options.measure,result.measurements.decodes,result.measurements.decodeMilliseconds);
        result.success=options.decoder?options.decoder(encoded,asset.path().extension().u8string(),result.image,limits,&result.error)
            :loadRgbaImage(encoded,asset.path().extension().u8string(),result.image,limits,&result.error);
    }
    const auto &image=result.image;
    if (result.success && (image.width<=0 || image.height<=0 || image.width>16384 || image.height>16384 ||
        static_cast<std::uint64_t>(image.width)*image.height>100000000 ||
        image.pixels.size()!=static_cast<std::uint64_t>(image.width)*image.height*4))
    { result.success=false;result.error="Decoded image has invalid dimensions or pixels."; }
    if (!result.success || cancelled(asset,cancel)) {result.success=false;result.image={};}
}
}
