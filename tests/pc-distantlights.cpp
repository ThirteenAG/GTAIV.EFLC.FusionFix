#include "../source/distantlights.hxx"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>

using namespace DistantLights;

// Execute the exact verified CE instruction window with the patched AND
// immediate. In a Win32 test process, DEC/INC retain their original encodings.
__declspec(naked) int CorrectedNativeIndex(int)
{
    __asm {
        mov eax, [esp+4]
        _emit 0x25
        _emit 0x0F
        _emit 0x00
        _emit 0x00
        _emit 0x00
        _emit 0x79
        _emit 0x05
        _emit 0x48
        _emit 0x83
        _emit 0xC8
        _emit 0xF0
        _emit 0x40
        ret
    }
}

int main(int argc, char** argv)
{
    assert(argc==2);
    for(int hash=-10000;hash<=10000;++hash)
    {
        assert(CorrectedNativeIndex(hash)==((hash%16)+16)%16);
        if(hash>=0) assert(CorrectedNativeIndex(hash)==hash%16);
    }
    assert(CorrectedNativeIndex(INT32_MIN)==0);
    assert(CorrectedNativeIndex(INT32_MAX)==15);
    std::ifstream file(argv[1],std::ios::binary);
    assert(file);
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};
    auto u16=[&](size_t p)->uint16_t {return uint16_t(bytes[p])|(uint16_t(bytes[p+1])<<8);};
    auto s16=[&](size_t p){return std::bit_cast<int16_t>(u16(p));};
    auto u32=[&](size_t p)->uint32_t {return uint32_t(u16(p))|(uint32_t(u16(p+2))<<16);};
    assert(bytes.size()==82904 && u32(0)==11680 && u32(4)==1004);
    std::vector<Point> points;
    for(size_t p=776;p<776+11680*6;p+=6) points.push_back({s16(p),s16(p+2),s16(p+4)});
    // Recorded native PC adjacent values, indexed -15..15. These are evidence
    // for the regression test, never values installed into game memory.
    const float original[]{200,15,std::bit_cast<float>(35u),std::bit_cast<float>(35u),
        std::bit_cast<float>(90u),1024,768,std::bit_cast<float>(1u),1,250,325,
        std::bit_cast<float>(3u),std::bit_cast<float>(1u),1,0,
        .1f,.7f,.5f,.96f,.34f,.75f,.2f,.4f,.65f,.05f,.76f,.83f,.59f,.34f,.66f,.43f};
    size_t nativeLights=0,correctedLights=0,blockedPaths=0;
    Road road;
    for(size_t p=776+11680*6;p<bytes.size();p+=12)
    {
        const auto first=u16(p+8), count=u16(p+10);
        assert(count>=2 && size_t(first)+count<=points.size());
        const auto& point=points[first];
        const int hash=int(point.x)/300+int(point.y)/500;
        const int index=CorrectedNativeIndex(hash);
        const float threshold=original[15+index];
        assert(threshold==StreetlightThreshold(point));
        const bool nativeOn=original[15+hash%16]<=1;
        if(!nativeOn) ++blockedPaths;
        road.Assign(std::span(points.data()+first,count));
        for(float s=0;s<road.Length();s+=60)
        {
            if(nativeOn) ++nativeLights;
            if(threshold<=1) ++correctedLights;
        }
    }
    assert(nativeLights==4057 && correctedLights==4381 && blockedPaths==71);
    std::cout << "Native x86 lookup and original PC DAT passed: 324 candidates restored on 71 paths\n";
}
