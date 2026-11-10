#pragma once
#include <cstring>
#include <istream>
#include <limits>
#include <memory>
#include <streambuf>
#include <vector>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <objidl.h>
#include <combaseapi.h>
#elif defined(TTRPG_HAS_LIBPNG)
#include <png.h>
#endif

namespace gm::image_decode
{
class MemoryBuffer final : public std::streambuf
{
public:
    explicit MemoryBuffer(const std::vector<unsigned char> &bytes)
    {
        // The stream is input-only; streambuf's interface predates const data.
        char *begin=bytes.empty()?&empty_:reinterpret_cast<char *>(const_cast<unsigned char *>(bytes.data()));
        setg(begin,begin,begin+bytes.size());
    }
protected:
    pos_type seekoff(off_type offset,std::ios_base::seekdir direction,std::ios_base::openmode mode) override
    {
        if (!(mode & std::ios_base::in)) return pos_type(off_type(-1));
        const off_type size=egptr()-eback();
        const off_type base=direction==std::ios_base::beg?0:direction==std::ios_base::cur?gptr()-eback():size;
        if (offset < -base || offset > size-base) return pos_type(off_type(-1));
        setg(eback(),eback()+base+offset,egptr());
        return pos_type(base+offset);
    }
    pos_type seekpos(pos_type position,std::ios_base::openmode mode) override
    { return seekoff(off_type(position),std::ios_base::beg,mode); }
private:
    char empty_=0;
};
class MemoryInput final : public std::istream
{
public:
    explicit MemoryInput(const std::vector<unsigned char> &bytes):std::istream(nullptr),buffer_(bytes)
    { rdbuf(&buffer_); }
private:
    MemoryBuffer buffer_;
};

#if defined(_WIN32)
struct ReleaseStream { void operator()(IStream *stream) const { if(stream)stream->Release(); } };
using OwnedStream=std::unique_ptr<IStream,ReleaseStream>;
inline OwnedStream encodedStream(const std::vector<unsigned char> &bytes)
{
    // OLE owns and releases the HGLOBAL; keep the stream alive past Bitmap.
    // https://learn.microsoft.com/windows/win32/api/combaseapi/nf-combaseapi-createstreamonhglobal
    if (bytes.empty() || bytes.size()>std::numeric_limits<ULONG>::max()) return {};
    IStream *raw=nullptr;
    if (FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&raw))) return {};
    OwnedStream stream(raw);
    ULONG written=0;
    if (FAILED(stream->Write(bytes.data(),static_cast<ULONG>(bytes.size()),&written)) || written!=bytes.size()) return {};
    LARGE_INTEGER start{};
    if (FAILED(stream->Seek(start,STREAM_SEEK_SET,nullptr))) return {};
    return stream;
}
#elif defined(TTRPG_HAS_LIBPNG)
struct PngInput { const std::vector<unsigned char> &bytes; std::size_t offset=0; };
inline void readPng(png_structp png,png_bytep output,png_size_t count)
{
    auto &input=*static_cast<PngInput *>(png_get_io_ptr(png));
    if (count>input.bytes.size()-input.offset) png_error(png,"Truncated image input");
    std::memcpy(output,input.bytes.data()+input.offset,count);
    input.offset+=count;
}
#endif
}
