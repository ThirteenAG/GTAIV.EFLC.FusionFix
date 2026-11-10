module;

#include <common.hxx>
#include <fstream>
#include <span>

export module distantlights;

import common;
import comvars;
import settings;

// Distant lights: the streetlight sprites along the roads of distantlights.dat, the traffic on them and the aircraft.
// PC draws streetlights every 60 units and imposter car models as traffic; the Xbox 360 has its own road data
// (shipped as distantlights_360.dat), streetlights every 30 units and moving white/red sprites as traffic. The Console
// setting draws the Xbox ones. In both, inland lights and cars fade in with the camera's height above them instead of
// the game's fixed 75 m camera height.
namespace DistantLights
{
    // The layout of the game's sprite buffer
    struct alignas(16) Vector
    {
        float x{}, y{}, z{}, w{};
        Vector operator+(const Vector& b) const { return { x + b.x, y + b.y, z + b.z }; }
        Vector operator-(const Vector& b) const { return { x - b.x, y - b.y, z - b.z }; }
        Vector operator*(float s) const { return { x * s, y * s, z * s }; }
        float Length() const { return std::sqrt(x * x + y * y + z * z); }
    };

    // distantlights.dat: point and string counts, three tables of the 8x8 map blocks (first string, coastal strings,
    // inland strings), then the points and the strings. The Xbox file is big-endian.
    struct PathPoint
    {
        int16_t x, y, z;
    };

    struct PathString
    {
        int16_t x, y, z, radius;
        uint16_t firstPoint, pointCount;
    };

    constexpr size_t BlockCount = 64;

    // A road as straight pieces, each with its distance from the start; repeated points are skipped
    class Road
    {
        struct Piece
        {
            Vector start, direction;
            float length, distance;
        };
        std::vector<Piece> pieces;
        float length = 0.0f;

    public:
        Road(std::span<const PathPoint> points)
        {
            for (size_t i = 1; i < points.size(); ++i)
            {
                Vector a{ float(points[i - 1].x), float(points[i - 1].y), float(points[i - 1].z) };
                Vector b{ float(points[i].x), float(points[i].y), float(points[i].z) };
                auto delta = b - a;
                float pieceLength = delta.Length();
                if (pieceLength <= 0.0001f)
                    continue;
                pieces.push_back({ a, delta * (1.0f / pieceLength), pieceLength, length });
                length += pieceLength;
            }
        }

        float Length() const { return length; }

        Vector At(float distance) const
        {
            auto it = std::lower_bound(pieces.begin(), pieces.end(), distance, [](const Piece& p, float d) { return p.distance + p.length < d; });
            if (it == pieces.end())
                --it;
            return it->start + it->direction * (distance - it->distance);
        }
    };

    std::array<uint32_t, BlockCount> firstString, coastalStrings, inlandStrings;
    std::vector<PathPoint> points;
    std::vector<PathString> strings;
    std::vector<Road> roads;

    void LoadConsolePaths()
    {
        std::wstring root(MAX_PATH, L'\0');
        std::filesystem::path folder;
        if (UAL::GetOverloadPathW && UAL::GetOverloadPathW(root.data(), root.size()))
            folder = root.c_str();
        else
            folder = GetExeModulePath() / L"update";

        std::ifstream file(folder / L"pc/data/paths/distantlights_360.dat", std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        constexpr size_t header = 8 + 3 * BlockCount * 4;
        auto u16 = [&](size_t at) { return uint16_t((bytes[at] << 8) | bytes[at + 1]); };
        auto u32 = [&](size_t at) { return (uint32_t(u16(at)) << 16) | u16(at + 2); };
        if (bytes.size() < header)
            return;
        uint32_t pointCount = u32(0);
        uint32_t stringCount = u32(4);
        if (bytes.size() != header + uint64_t(pointCount) * 6 + uint64_t(stringCount) * 12)
            return;

        for (size_t b = 0; b < BlockCount; ++b)
        {
            firstString[b] = u32(8 + b * 4);
            coastalStrings[b] = u32(8 + BlockCount * 4 + b * 4);
            inlandStrings[b] = u32(8 + BlockCount * 8 + b * 4);
            if (uint64_t(firstString[b]) + coastalStrings[b] + inlandStrings[b] > stringCount)
                return;
        }

        for (size_t i = 0, at = header; i < pointCount; ++i, at += 6)
            points.push_back({ int16_t(u16(at)), int16_t(u16(at + 2)), int16_t(u16(at + 4)) });

        for (size_t i = 0, at = header + pointCount * 6; i < stringCount; ++i, at += 12)
        {
            PathString s{ int16_t(u16(at)), int16_t(u16(at + 2)), int16_t(u16(at + 4)), int16_t(u16(at + 6)), u16(at + 8), u16(at + 10) };
            if (uint32_t(s.firstPoint) + s.pointCount > pointCount)
            {
                points.clear();
                strings.clear();
                return;
            }
            strings.push_back(s);
        }

        for (auto& s : strings)
            roads.emplace_back(std::span(points.data() + s.firstPoint, s.pointCount));
    }

    float Ramp(float start, float end, float value)
    {
        return std::clamp((value - start) / (end - start), 0.0f, 1.0f);
    }

    // The game fades distant sprites in from 150 m over distanceSpan. Limiting the distance it sees makes the light
    // just as faded as the camera is low above it: invisible at its height, fully faded in 100 m above it.
    float HeightLimitedDistance(float distance, float cameraZ, float lightZ, float distanceSpan)
    {
        return std::min(distance, 150.0f + distanceSpan * Ramp(0.0f, 100.0f, cameraZ - lightZ));
    }

    float HeightLimitedOpacity(float opacity, float cameraZ, float carZ)
    {
        return std::min(opacity, Ramp(0.0f, 100.0f, cameraZ - carZ));
    }

    // Xbox sprite colour: the colour times the intensity, then a random brightness of 170-254 that fades in from
    // 150 to 250 m
    uint32_t XboxSpriteColor(uint32_t rgb, float intensity, float distance)
    {
        uint32_t brightness = 170 + ((uint32_t(game_rand()) * 85) >> 15);
        if (distance < 250.0f)
            brightness = uint32_t(std::max(0.0f, (distance - 150.0f) * 0.01f) * float(brightness));
        auto channel = [&](uint32_t shift)
        {
            auto input = uint32_t(float((rgb >> shift) & 0xFF) * std::clamp(intensity, 0.0f, 1.0f));
            return ((input * brightness) >> 8) << shift;
        };
        return 0xFF000000 | channel(16) | channel(8) | channel(0);
    }

    // Each road switches all its streetlights on at its own point of dusk, picked by where it starts
    float StreetlightThreshold(const PathPoint& first)
    {
        static constexpr std::array<float, 16> thresholds{ 0.1f, 0.7f, 0.5f, 0.96f, 0.34f, 0.75f, 0.2f, 0.4f, 0.65f, 0.05f, 0.76f, 0.83f, 0.59f, 0.34f, 0.66f, 0.43f };
        int index = (first.x / 300 + first.y / 500) % 16;
        return thresholds[index < 0 ? index + 16 : index];
    }

    void(__cdecl* CDistantLights__Render)(float);
    void(__cdecl* RenderDayTrafficModels)(float);
    void(__cdecl* RenderNightTrafficModels)();
    void(__cdecl* CDistantLights__RenderOutSpriteBuffer)(Vector* positions, uint32_t* colors, int* count, Vector* right, Vector* up);
    void(__cdecl* rage__grcBegin)(int primitive, int vertexCount);
    void(__cdecl* rage__grcVertex)(float x, float y, float z, float nx, float ny, float nz, uint32_t color, float u, float v);
    void(__stdcall* rage__grcEnd)();
    int(__thiscall* rage__grcViewport__IsSphereVisible)(rage::grcViewport*, float x, float y, float z, float radius, float* distance);
    int32_t* nCoastalStrings;
    int32_t* nInlandStrings;

    bool consolePass = false;
    float nightFade = 0.0f;

    bool IsConsole()
    {
        static auto distantLights = FusionFixSettings.GetRef("PREF_DISTANTLIGHTS");
        return distantLights->get() == 1 && !roads.empty();
    }

    // The game's sprite batch drawing, with the texture flipped vertically in Console like on the Xbox
    void DrawSprites(Vector* positions, uint32_t* colors, int* count, Vector* right, Vector* up)
    {
        if (!consolePass)
            return CDistantLights__RenderOutSpriteBuffer(positions, colors, count, right, up);

        if (*count > 0)
        {
            auto difference = *right - *up;
            auto sum = *right + *up;
            rage__grcBegin(3, 6 * *count); // triangle list
            for (int i = 0; i < *count; ++i)
            {
                auto vertex = [&](const Vector& p, float u, float v)
                {
                    rage__grcVertex(p.x, p.y, p.z, 0.0f, 0.0f, -1.0f, colors[i], u, v);
                };
                auto a = positions[i] - difference;
                auto b = positions[i] + difference;
                auto c = positions[i] - sum;
                auto d = positions[i] + sum;
                vertex(a, 0.0f, 0.0f);
                vertex(b, 1.0f, 1.0f);
                vertex(c, 0.0f, 1.0f);
                vertex(b, 1.0f, 1.0f);
                vertex(d, 1.0f, 0.0f);
                vertex(a, 0.0f, 0.0f);
            }
            rage__grcEnd();
        }
        *count = 0;
    }

    template<class Append>
    void AddConsoleRoadSprites(rage::grcViewport* viewport, const Vector& camera, Append append)
    {
        auto add = [&](const Vector& light, float intensity, uint32_t rgb, bool inland)
        {
            if (inland && camera.z <= light.z)
                return;
            float distance = (light - camera).Length();
            if (distance <= 150.0f)
                return;
            append(light, XboxSpriteColor(rgb, intensity, inland ? HeightLimitedDistance(distance, camera.z, light.z, 100.0f) : distance));
        };

        // Traffic moves along each road both ways: white lights every 33 units, red every 45.5, timed by the clock
        uint32_t time = *CTimer::m_snTimeInMilliseconds;
        float whiteStart = float((time / 43) & 0xFF) * (33.0f / 256.0f);
        float redStart = 45.5f - float((time / 57) & 0xFF) * (45.5f / 256.0f);

        for (size_t block = 0; block < BlockCount; ++block)
        {
            // 8x8 blocks of 750 units
            float x = (float(block / 8) + 0.5f) * 750.0f - 3000.0f;
            float y = (float(block % 8) + 0.5f) * 750.0f - 3000.0f;
            if (!rage__grcViewport__IsSphereVisible(viewport, x, y, 20.0f, 525.0f, nullptr))
                continue;

            for (bool inland : { false, true })
            {
                uint32_t begin = firstString[block] + (inland ? coastalStrings[block] : 0);
                uint32_t end = begin + (inland ? inlandStrings[block] : coastalStrings[block]);
                for (uint32_t i = begin; i < end; ++i)
                {
                    auto& s = strings[i];
                    auto& road = roads[i];
                    if (road.Length() <= 0.0f || !rage__grcViewport__IsSphereVisible(viewport, s.x, s.y, s.z, s.radius, nullptr))
                        continue;

                    for (float d = whiteStart; d < road.Length(); d += 33.0f)
                        add(road.At(d) + Vector{ 0.0f, 0.0f, 2.0f }, nightFade, 0xFFFFFF, inland);
                    for (float d = redStart; d < road.Length(); d += 45.5f)
                        add(road.At(d) + Vector{ 0.0f, 0.0f, 2.0f }, nightFade, 0xFF0000, inland);

                    if (nightFade < StreetlightThreshold(points[s.firstPoint]))
                        continue;
                    for (float d = 0.0f; d < road.Length(); d += 30.0f)
                        add(road.At(d) + Vector{ 0.0f, 0.0f, 12.0f }, 1.0f, 0xFFDC80, inland);
                }
            }
        }
    }

    // The last sprite batch of CDistantLights::Render, the aircraft. In Console the roads are drawn first, then the
    // aircraft, at half size in TBoGT like on the Xbox (Fusion Fix keeps them full size on PC).
    void __cdecl RenderOutLastSprites(Vector* positions, uint32_t* colors, int* count, Vector* right, Vector* up)
    {
        if (!consolePass || nightFade <= 0.0f || !*rage::pCurrentViewport)
            return DrawSprites(positions, colors, count, right, up);

        auto viewport = rage::GetCurrentViewport();
        Vector camera{ viewport->mCameraMatrix[3][0], viewport->mCameraMatrix[3][1], viewport->mCameraMatrix[3][2] };

        std::array<Vector, 64> roadPositions;
        std::array<uint32_t, 64> roadColors;
        int roadCount = 0;
        AddConsoleRoadSprites(viewport, camera, [&](const Vector& light, uint32_t color)
        {
            roadPositions[roadCount] = light;
            roadColors[roadCount++] = color;
            if (roadCount == int(roadPositions.size()))
                DrawSprites(roadPositions.data(), roadColors.data(), &roadCount, right, up);
        });
        DrawSprites(roadPositions.data(), roadColors.data(), &roadCount, right, up);

        float aircraftSize = *_dwCurrentEpisode == 2 ? 0.5f : 1.0f;
        auto aircraftRight = *right * aircraftSize;
        auto aircraftUp = *up * aircraftSize;
        DrawSprites(positions, colors, count, &aircraftRight, &aircraftUp);
    }

    // Console: no PC streetlights (their road counts are zero while the aircraft are drawn) and no PC traffic models
    void __cdecl Render(float fade)
    {
        nightFade = std::clamp(fade, 0.0f, 1.0f);
        if (!IsConsole())
            return CDistantLights__Render(fade);

        std::array<int32_t, BlockCount> coastal, inland;
        std::copy_n(nCoastalStrings, BlockCount, coastal.begin());
        std::copy_n(nInlandStrings, BlockCount, inland.begin());
        std::fill_n(nCoastalStrings, BlockCount, 0);
        std::fill_n(nInlandStrings, BlockCount, 0);
        consolePass = true;
        CDistantLights__Render(fade);
        consolePass = false;
        std::copy_n(coastal.begin(), BlockCount, nCoastalStrings);
        std::copy_n(inland.begin(), BlockCount, nInlandStrings);
    }

    void __cdecl RenderDayTraffic(float fade)
    {
        if (!IsConsole())
            RenderDayTrafficModels(fade);
    }

    void __cdecl RenderNightTraffic()
    {
        if (!IsConsole())
            RenderNightTrafficModels();
    }
}

class DistantLightsModule
{
public:
    DistantLightsModule()
    {
        FusionFix::onInitEvent() += []()
        {
            using namespace DistantLights;

            auto pattern = hook::pattern("F3 0F 11 14 24 E8 ? ? ? ? 83 C4 04 E9");
            if (pattern.empty()) // only 1.2.0.32, 1.2.0.43, 1.2.0.59
                return;

            LoadConsolePaths();

            // Vehicle imposter callback, night: CDistantLights::Render (streetlights, aircraft), then the traffic models
            CDistantLights__Render = injector::GetBranchDestination(pattern.get_first(5)).get();
            RenderNightTrafficModels = injector::GetBranchDestination(pattern.get_first(13)).get();
            injector::MakeCALL(pattern.get_first(5), Render, true);
            injector::MakeJMP(pattern.get_first(13), RenderNightTraffic, true);

            // Day: the traffic models
            pattern = hook::pattern("F3 0F 11 04 24 E8 ? ? ? ? 83 C4 04 C6 05 ? ? ? ? 01 C3");
            RenderDayTrafficModels = injector::GetBranchDestination(pattern.get_first(5)).get();
            injector::MakeCALL(pattern.get_first(5), RenderDayTraffic, true);

            // CDistantLights::Load: the coastal and inland string counts of each block
            pattern = hook::pattern("6A 04 68 ? ? ? ? 56 E8 ? ? ? ? 6A 04 68 ? ? ? ? 56 E8 ? ? ? ? 68 00 01 00 00");
            nCoastalStrings = *pattern.get_first<int32_t*>(48);
            nInlandStrings = *pattern.get_first<int32_t*>(64);

            pattern = hook::pattern("F3 0F 11 04 24 E8 ? ? ? ? 85 C0 0F 84 ? ? ? ? 33 FF");
            rage__grcViewport__IsSphereVisible = injector::GetBranchDestination(pattern.get_first(5)).get();

            pattern = hook::pattern("50 68 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 83 C4 10 C7 04 24 00 00 80 3F");
            CDistantLights__RenderOutSpriteBuffer = injector::GetBranchDestination(pattern.get_first(11)).get();
            injector::MakeCALL(pattern.get_first(11), RenderOutLastSprites, true);

            // In CDistantLights::RenderOutSpriteBuffer
            pattern = hook::pattern("8D 04 40 03 C0 50 6A 03 E8 ? ? ? ? 33 C9");
            rage__grcBegin = injector::GetBranchDestination(pattern.get_first(8)).get();
            pattern = hook::pattern("F3 0F 11 14 24 E8 ? ? ? ? F3 0F 10 44 24 54");
            rage__grcVertex = injector::GetBranchDestination(pattern.get_first(5)).get();
            pattern = hook::pattern("E8 ? ? ? ? C7 07 00 00 00 00 5F 5E 8B E5 5D C3");
            rage__grcEnd = injector::GetBranchDestination(pattern.get_first(0)).get();

            // The streetlight's dusk threshold index is a signed remainder, negative in parts of Alderney, which reads
            // before the table: and eax, 8000000Fh -> and eax, 0Fh
            pattern = hook::pattern("25 0F 00 00 80 79 05 48 83 C8 F0 40 F3 0F 10 45 08 0F 2F 04 85");
            injector::WriteMemory<uint32_t>(pattern.get_first(1), 0x0F, true);

            // Inland streetlights and cars are drawn only once the camera is above 75 m; the height fades below
            // replace that.
            pattern = hook::pattern("83 FF 01 0F 85 ? ? ? ? F3 0F 10 44 24 58 0F 2F 05 ? ? ? ? 0F 86");
            injector::MakeNOP(pattern.get_first(22), 6, true);
            pattern = hook::pattern("83 F9 01 0F 85 ? ? ? ? F3 0F 10 84 24 DC 00 00 00 0F 2F 05 ? ? ? ? 0F 86");
            injector::MakeNOP(pattern.get_first(25), 6, true);
            pattern = hook::pattern("83 F9 01 0F 85 ? ? ? ? F3 0F 10 84 24 08 01 00 00 0F 2F 05 ? ? ? ? 0F 86");
            injector::MakeNOP(pattern.get_first(25), 6, true);

            // PC streetlights: the distance passed to CDistantLights::ModifyColourForDist. [esp+4C] is 1 for inland
            // roads, [esp+C8] the camera height, [esp+168] the light's height.
            pattern = hook::pattern("51 8D 84 24 80 00 00 00 F3 0F 11 04 24 50 E8");
            static auto StreetlightFadeHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            {
                if (*(int32_t*)(regs.esp + 0x4C) == 1)
                    regs.xmm0.f32[0] = HeightLimitedDistance(regs.xmm0.f32[0], *(float*)(regs.esp + 0xC8), *(float*)(regs.esp + 0x168), 150.0f);
            });

            // PC traffic models: their distance opacity, in xmm1 by day and xmm0 at night. [esp+E4] / [esp+C8] is 1 for
            // inland roads, [esp+138] the camera height, [esp+108] / [esp+118] the car's height.
            pattern = hook::pattern("8B 0C CD ? ? ? ? 51 8D 84 24 94 01 00 00 50 8D 84 24 88 01 00 00");
            static auto DayTrafficFadeHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            {
                if (*(int32_t*)(regs.esp + 0xE4) == 1)
                    regs.xmm1.f32[0] = HeightLimitedOpacity(regs.xmm1.f32[0], *(float*)(regs.esp + 0x138), *(float*)(regs.esp + 0x108));
            });

            pattern = hook::pattern("6A 00 8D 84 24 A4 01 00 00 50 8D 84 24 98 01 00 00 50 8D 84 24 E8 00 00 00 50 68 ? ? ? ? 8D 84 24 94 01 00 00");
            static auto NightTrafficFadeHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            {
                if (*(int32_t*)(regs.esp + 0xC8) == 1)
                    regs.xmm0.f32[0] = HeightLimitedOpacity(regs.xmm0.f32[0], *(float*)(regs.esp + 0x138), *(float*)(regs.esp + 0x118));
            });
        };
    }
} DistantLightsModule;
