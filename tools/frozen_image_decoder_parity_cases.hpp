#pragma once
// Synthetic JPEG encoding follows the existing import-service test fixture.
// Static linkage keeps this test encoder independent of production symbols.
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "extern/glfw/deps/stb_image_write.h"
#undef STB_IMAGE_WRITE_IMPLEMENTATION
#undef STB_IMAGE_WRITE_STATIC

namespace frozen_decode_test
{
inline void sameImage(const LoadedRgbaImage &a,const LoadedRgbaImage &b)
{check(a.width==b.width&&a.height==b.height&&a.sourceWidth==b.sourceWidth&&a.sourceHeight==b.sourceHeight&&
    a.exifOrientation==b.exifOrientation&&a.pixels==b.pixels,"File/memory/worker image metadata or RGBA differs");}
inline void jpegAndAllocationParity()
{
    Fixture f;f.path=f.storage->directory()/"orientation.jpg";
    const unsigned char rgba[]={255,0,0,255,0,255,0,255,0,0,255,255,90,60,30,255,10,70,120,255,230,170,40,255};
    std::vector<unsigned char> jpeg;
    const auto append=[](void *context,void *data,int size){auto &v=*static_cast<std::vector<unsigned char> *>(context);
        const auto *p=static_cast<unsigned char *>(data);v.insert(v.end(),p,p+size);};
    check(stbi_write_jpg_to_func(append,&jpeg,3,2,4,rgba,95)!=0&&jpeg.size()>2,"Synthetic JPEG encoder failed");
    FrozenImageDecodeWorker worker;RgbaImageLoadOptions limits;limits.maximumSourceEdge=16;limits.maximumSourcePixels=64;
    for(int orientation=1;orientation<=8;++orientation){
        const std::vector<unsigned char> exif{0xff,0xe1,0x00,0x22,'E','x','i','f',0,0,'I','I',0x2a,0,8,0,0,0,
            1,0,0x12,1,3,0,1,0,0,0,static_cast<unsigned char>(orientation),0,0,0,0,0,0,0};
        std::vector<unsigned char> encoded(jpeg.begin(),jpeg.begin()+2);encoded.insert(encoded.end(),exif.begin(),exif.end());
        encoded.insert(encoded.end(),jpeg.begin()+2,jpeg.end());
        {std::ofstream out(f.path,std::ios::binary);out.write(reinterpret_cast<const char *>(encoded.data()),encoded.size());check(bool(out),"EXIF fixture write failed");}
        LoadedRgbaImage file,memory;std::string error;
        check(loadRgbaImage(f.path,file,limits,&error)&&loadRgbaImage(encoded,".jpg",memory,limits,&error),"Real JPEG file/memory decode failed");
        sameImage(file,memory);check(file.exifOrientation==orientation&&file.width==(orientation>=5?2:3)&&file.height==(orientation>=5?3:2),
            "JPEG parity fixture did not exercise its independent orientation/dimension expectation");
        check(worker.request(f.asset())!=0,"JPEG worker request refused");auto result=take(worker);
        check(result->success,"JPEG worker decode failed");sameImage(file,result->image);acknowledge(worker,result);
        for(bool edge:{false,true}){auto bound=limits;if(edge)bound.maximumSourceEdge=2;else bound.maximumSourcePixels=5;
            LoadedRgbaImage rejected;check(!loadRgbaImage(encoded,".jpg",rejected,bound,&error)&&error=="image.too_large"&&rejected.pixels.empty(),
                "Memory JPEG ignored pre-allocation dimension/pixel bound");
            check(!loadRgbaImage(f.path,rejected,bound,&error)&&error=="image.too_large"&&rejected.pixels.empty(),"File JPEG bounds differ from memory input");}
    }
}
inline void exactBytesAndEncodedBound()
{
    Fixture f("ORIGINAL_BYTES_123");const auto original=f.asset();const auto oldTime=std::filesystem::last_write_time(f.path);
    const auto originalSize=std::filesystem::file_size(f.path);
    auto gate=std::make_shared<Gate>();auto decoder=std::make_shared<Decoder>();
    FrozenImageDecodeWorker worker(options(decoder,gate));Release release{gate};
    check(worker.request(original)!=0,"Exact-byte request refused");entered(gate);
    {std::ofstream out(f.path,std::ios::binary);out<<"REPLACED_BYTES_456";check(bool(out),"Exact-byte mutation failed");}
    std::filesystem::last_write_time(f.path,oldTime);check(std::filesystem::file_size(f.path)==originalSize,"Exact-byte fixture changed encoded size");gate->open();
    auto result=take(worker);check(result->success&&std::string(decoder->encodedSeen.begin(),decoder->encodedSeen.end())=="ORIGINAL_BYTES_123",
        "Worker decoded changed path bytes under the old reviewed hash");acknowledge(worker,result);
    check(worker.request(original)!=0,"Old-hash retry refused before readback test");result=take(worker);
    check(!result->success&&decoder->calls==1,"Later old-hash request accepted replaced bytes");acknowledge(worker,result);
    Fixture large;const auto largeAsset=large.asset();std::filesystem::resize_file(large.path,256ull*1024*1024+1);
    if(worker.request(largeAsset)){result=take(worker);check(!result->success&&result->image.pixels.empty(),"Encoded allocation bound accepted oversized file");acknowledge(worker,result);}
    check(decoder->calls==1,"Oversized encoded file reached decoder");
}
}
