#include "../source/distantlights.hxx"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>

using namespace NewDistantLights;
static void Near(float a, float b) { assert(std::abs(a-b) < .0002f); }

int main(int argc, char** argv)
{
    assert(argc == 2);
    std::ifstream file(argv[1], std::ios::binary);
    assert(file);
    const std::vector<uint8_t> original{std::istreambuf_iterator<char>(file), {}};
    Dataset data;
    assert(data.Decode(original));
    assert(data.points.size() == 2542 && data.strings.size() == 309);
    const auto firstRoadPoint = data.points[data.strings.front().firstPoint];
    assert(firstRoadPoint.x == -1659 && firstRoadPoint.y == -305);
    const auto baseline = data.points.front();
    auto reject = [&](std::vector<uint8_t> bytes) {
        assert(!data.Decode(bytes));
        assert(data.points.size() == 2542 && data.strings.size() == 309);
        assert(data.points.front().x == baseline.x); // Failed decode is atomic.
    };
    for (size_t n : {size_t(0),size_t(7),size_t(775),original.size()-1})
        reject({original.begin(), original.begin()+n});
    auto bad = original; bad.push_back(0); reject(bad);
    bad = original; std::reverse(bad.begin(),bad.begin()+4); reject(bad); // PC endian
    bad = original; bad[0] = 0xFF; reject(bad); // Allocation bound
    bad = original; bad[264] = 0xFF; reject(bad); // Block range
    bad = original; bad[8+11*4+3] = 1; reject(bad); // Gap/overlap in partition
    const size_t strings = 776+2542*6;
    bad = original; bad[strings+8] = 0xFF; reject(bad); // Point range
    bad = original; bad[strings+6] = 0xFF; reject(bad); // Negative radius
    bad = original; bad[strings+10] = bad[strings+11] = 0; reject(bad);

    // Every possible native rand() result retains Xbox's exact range mapping.
    std::array<int,85> histogram{};
    for (uint32_t sample=0; sample<32768; ++sample)
    {
        const auto brightness=XboxBrightness(sample);
        assert(brightness==170u+uint32_t(float(sample)*(1.0f/32768.0f)*85.0f));
        assert(brightness>=170 && brightness<255);
        ++histogram[brightness-170];
    }
    for (int count:histogram) assert(count==385 || count==386);
    assert(XboxSpriteColor(0xFFDC80,1,150,170)==0xFF000000);
    assert(XboxSpriteColor(0xFFDC80,1,151,170)==0xFF000000);
    assert(XboxSpriteColor(0xFFDC80,1,200,170)==0xFF54492A);
    assert(XboxSpriteColor(0xFFDC80,1,250,254)==0xFFFDDA7F);
    assert(XboxSpriteColor(0xFFFFFF,.5f,250,170)==0xFF545454);
    assert(XboxSpriteColor(0xFF0000,.5f,250,170)==0xFF540000);
    assert(XboxSpriteColor(0xFFFFFF,0,250,254)==0xFF000000);
    assert(XboxSpriteColor(0xFFDC80,1,300,254)==XboxSpriteColor(0xFFDC80,1,250,254));

    Near(FirstDistance(42,false), 0);
    Near(FirstDistance(43,false), 33.0f/256);
    Near(FirstDistance(56,true), 45.5f);
    Near(FirstDistance(57,true), 45.5f-45.5f/256);
    Near(FirstDistance(256*43,false), 0);
    Near(FirstDistance(256*57,true), 45.5f);
    assert(std::isfinite(FirstDistance(UINT32_MAX,false)));
    assert(std::isfinite(FirstDistance(UINT32_MAX,true)));
    // All tick transitions, including wrap: opposing Xbox centerline streams.
    for (uint32_t q = 0; q < 256; ++q)
    {
        Near(std::fmod(FirstDistance((q+1)*43,false)-FirstDistance(q*43,false)+33.0f,33.0f),33.0f/256);
        Near(std::fmod(FirstDistance(q*57,true)-FirstDistance((q+1)*57,true)+45.5f,45.5f),45.5f/256);
    }
    Road road;
    const Point bend[]{ {0,0,0},{0,0,0},{30,0,0},{30,40,0} };
    road.Assign(bend);
    Near(road.Length(),70);
    Near(road.At(35).position.y,5);
    int whites=0, reds=0;
    ForEachVehicle(road,0,[&](Vector p,bool red,uint32_t) {
        Near(p.z,2);
        if(red) { ++reds; Near(p.x,30); Near(p.y,15.5f); }
        else ++whites;
    });
    assert(whites==3 && reds==1);
    const Point repeated[]{ {0,0,0},{0,0,0} };
    road.Assign(repeated);
    ForEachVehicle(road,0,[&](Vector,bool,uint32_t) { assert(false); });

    // Corrected lookup restores all candidates at full night, including Alderney.
    size_t candidates=0, visible=0, blocked=0;
    for(const auto& line:data.strings)
    {
        road.Assign(std::span(data.points.data()+line.firstPoint,line.pointCount));
        const bool on=StreetlightThreshold(data.points[line.firstPoint])<=1;
        if(!on) ++blocked;
        for(float s=0;s<road.Length();s+=30) { ++candidates; if(on) ++visible; }
    }
    assert(candidates==3404 && visible==3404 && blocked==0);
    Near(StreetlightThreshold(firstRoadPoint),.83f);
    Near(StreetlightThreshold(Point{-299,-499,0}),.1f); // Truncation, not floor.
    Near(StreetlightThreshold(Point{-300,0,0}),.43f);
    Near(StreetlightThreshold(Point{4500,0,0}),.43f);
    // Negative-coordinate cohorts use the same 16 original dusk/dawn values.
    for (int i=1;i<16;++i)
    {
        const float threshold=StreetlightThreshold(Point{int16_t(-300*i),0,0});
        Near(threshold,StreetlightThreshold(Point{int16_t(300*(16-i)),0,0}));
        assert(threshold>0 && threshold<1);
    }
    std::cout << "Xbox decode, malformed input, atomic failure, traffic timing, Xbox twinkle and streetlight counts passed\n";
}
