#include "../source/distantlights-render.hxx"
#include <cassert>
#include <iostream>

using namespace DistantLights;

static void CheckEpisodeSubmission()
{
    const std::array<uint8_t, 2> original{0x0F, 0x85}, patched{0x90, 0xE9}, unknown{0x90, 0x90};
    assert(NativeAircraftScale(true, original) == 0.5f);
    assert(NativeAircraftScale(true, patched) == 1.0f);
    assert(NativeAircraftScale(true, unknown) == 0.0f);
    assert(NativeAircraftScale(false, original) == 1.0f);
    assert(NativeAircraftScale(false, patched) == 1.0f);
    // Exercise both native branch states, all batch boundaries and no aircraft.
    for (bool tbogt : {false, true})
    for (auto branch : {original, patched})
    for (int roadCount : {0, 1, 63, 64, 65, 127, 128, 129, 150, 257})
    for (int aerialCount : {0, 10, 11})
    {
        std::array<Vector, 11> aircraft;
        std::array<uint32_t, 11> colors;
        for (int i = 0; i < aerialCount; ++i)
        {
            aircraft[i] = {float(10000+i), float(i), -float(i)};
            colors[i] = uint32_t(10000+i);
        }
        const float nativeScale = NativeAircraftScale(tbogt, branch);
        Vector right = Vector{2, 4, 6} * nativeScale;
        Vector up = Vector{-4, 6, 2} * nativeScale;
        int count = aerialCount, drawn = 0;
        auto flush = [&](Vector* p, uint32_t* c, int* n, Vector* r, Vector* u)
        {
            assert(*n >= 0 && *n <= 64);
            const bool aircraftBatch = p == aircraft.data();
            if (*n) assert(aircraftBatch == (drawn >= roadCount));
            for (int i = 0; i < *n; ++i, ++drawn)
            {
                const bool aerial = drawn >= roadCount;
                assert(aerial == aircraftBatch); // No batch may mix road and aircraft sprites.
                const int index = aerial ? drawn-roadCount : drawn;
                const uint32_t id = uint32_t(index + (aerial ? 10000 : 0));
                assert(c[i] == id && p[i].x == float(id));
                assert(p[i].y == float(index) && p[i].z == -float(index));
                const float scale = tbogt && aerial ? 1.0f : 2.0f;
                assert(r->x == scale && r->y == 2*scale && r->z == 3*scale);
                assert(u->x == -2*scale && u->y == 3*scale && u->z == scale);
            }
            *n = 0;
        };
        SubmitConsoleCoronas(tbogt, nativeScale, aircraft.data(), colors.data(), &count, &right, &up,
            [&](auto append)
            {
                for (int i = 0; i < roadCount; ++i)
                    append(Vector{float(i), float(i), -float(i)}, uint32_t(i));
                assert(drawn == (roadCount/64)*64);
            }, flush);
        assert(count == 0 && drawn == roadCount+aerialCount);
        assert(right.x == 2*nativeScale && up.x == -4*nativeScale);
    }
}

int main()
{
    CheckEpisodeSubmission();
    // Oblique, non-unit basis checks orientation in world space, not just UV
    // numbers; the Xbox four-corner mapping is independent of PC triangulation.
    std::array<Vector, 64> positions;
    std::array<uint32_t, 64> colors;
    for (int i = 0; i < 64; ++i)
    {
        positions[i] = {float(i*10), float(i-100), float(i+300)};
        colors[i] = 0xFFDC8000u + i;
    }
    Vector right{2, 3, 1}, up{-1, 4, 2};
    int count = 64, emitted = 0, nativeCalls = 0, begins = 0, ends = 0;
    auto native = [&](Vector* p, uint32_t* c, int* n, Vector* r, Vector* u)
    {
        assert(p == positions.data() && c == colors.data() && n == &count && r == &right && u == &up);
        ++nativeCalls;
        *n = 0;
    };
    auto begin = [&](int mode, int vertices)
    {
        assert(mode == 3 && vertices == 6*count);
        ++begins;
    };
    auto vertex = [&](float x, float y, float z, float nx, float ny, float nz, uint32_t color, float u, float v)
    {
        const int sprite = emitted/6, corner = emitted%6;
        constexpr std::array<float, 6> expectedU{0, 1, 0, 1, 1, 0};
        constexpr std::array<float, 6> expectedV{0, 1, 1, 1, 0, 0};
        assert(u == expectedU[corner] && v == expectedV[corner]);
        const auto expected = positions[sprite] + right*(2*u-1) + up*(1-2*v);
        assert(x == expected.x && y == expected.y && z == expected.z);
        assert(nx == 0 && ny == 0 && nz == -1 && color == colors[sprite]);
        ++emitted;
    };
    auto end = [&] { ++ends; };
    auto flush = [&](bool console)
    {
        FlushCoronaBatch(console, positions.data(), colors.data(), &count, &right, &up, native, begin, vertex, end);
    };
    // Default delegates without generating vertices.
    flush(false);
    assert(nativeCalls == 1 && begins == 0 && ends == 0 && count == 0);
    for (int batch : {64, 1})
    {
        emitted = 0;
        count = batch;
        flush(true);
        assert(emitted == batch*6 && count == 0);
    }
    flush(true);
    assert(begins == 2 && ends == 2 && nativeCalls == 1);
    std::cout << "Independent road/aircraft sizing across native and FusionFix branch states, IV/TLAD submission, Console geometry/UVs and Default bypass passed.\n";
}
