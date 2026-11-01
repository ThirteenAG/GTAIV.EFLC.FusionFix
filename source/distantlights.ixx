module;

#include <common.hxx>
#include <fstream>
#include "distantlights-render.hxx"

export module distantlights;

import common;
import comvars;
import settings;

namespace NewDistantLights
{
    using SpriteFlush = void(__cdecl*)(Vector*, uint32_t*, int*, Vector*, Vector*);
    static void(__cdecl* originalDay)(float);
    static void(__cdecl* originalNightSprites)(float);
    static void(__cdecl* originalNightModels)();
    static SpriteFlush flushSprites;
    static void(__cdecl* beginSprites)(int, int);
    static void(__cdecl* spriteVertex)(float, float, float, float, float, float, uint32_t, float, float);
    static void(__stdcall* endSprites)();
    static int(__thiscall* sphereVisible)(rage::grcViewport*, float, float, float, float, float*);
    static uint32_t* milliseconds;
    static const uint8_t* aircraftSizeBranch;
    static int32_t *coastalCount, *interiorCount;
    static bool ready{}, loadAttempted{}, consolePass{};
    static float nightFade{};
    static Dataset xbox;
    static std::vector<Road> roads;

    static bool LoadConsoleData()
    {
        if (loadAttempted) return !roads.empty();
        loadAttempted = true;
        try
        {
            // Same overload-root API used by FF's IMG/RPF loaders. Read a new
            // loose filename directly; never redirect the native PC DAT name.
            std::array<wchar_t, 32768> root{};
            std::filesystem::path folder;
            if (UAL::GetOverloadPathW && UAL::GetOverloadPathW(root.data(), root.size()) && root[0])
                folder = root.data();
            else
                folder = GetExeModulePath() / L"update";
            if (folder.is_relative()) folder = GetExeModulePath() / folder;
            std::ifstream file(folder / L"pc/data/paths/distantlights_360.dat", std::ios::binary | std::ios::ate);
            if (file)
            {
                const auto size = file.tellg();
                if (size >= 776 && size <= 86000)
                {
                    std::vector<uint8_t> bytes(static_cast<size_t>(size));
                    file.seekg(0);
                    if (file.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size())) && xbox.Decode(bytes))
                    {
                        std::vector<Road> decodedRoads(xbox.strings.size());
                        for (size_t i = 0; i < decodedRoads.size(); ++i)
                        {
                            const auto& line = xbox.strings[i];
                            decodedRoads[i].Assign(std::span(xbox.points.data()+line.firstPoint, line.pointCount));
                        }
                        roads = std::move(decodedRoads);
                        OutputDebugStringA("FusionFix: loaded separate Xbox distant-light data.\n");
                        return true;
                    }
                }
            }
        }
        catch (const std::exception&) {}
        OutputDebugStringA("FusionFix: distantlights_360.dat missing or invalid; Distant Lights retains native PC rendering. Restart after replacing the file.\n");
        return false;
    }

    static Mode GetMode()
    {
        if (ready && FusionFixSettings.Get("PREF_NEWDISTANTLIGHTS") == int(Mode::Console)
            && NativeAircraftScale(*_dwCurrentEpisode == 2, std::span<const uint8_t, 2>(aircraftSizeBranch, 2)) > 0.0f
            && LoadConsoleData())
            return Mode::Console;
        return Mode::Default;
    }

    static void RenderSprites(Vector* positions, uint32_t* colors, int* count, Vector* right, Vector* up)
    {
        auto device = rage::grcDevice::ms_pD3DDevice ? *rage::grcDevice::ms_pD3DDevice : nullptr;
        FlushCoronaBatch(consolePass, device, positions, colors, count, right, up,
            flushSprites, beginSprites, spriteVertex, endSprites);
    }

    static void __cdecl FinalSpriteFlush(Vector* positions, uint32_t* colors, int* count, Vector* right, Vector* up)
    {
        if (!consolePass || nightFade <= 0.0f || !rage::pCurrentViewport || !*rage::pCurrentViewport)
        {
            RenderSprites(positions, colors, count, right, up);
            return;
        }
        auto viewport = rage::GetCurrentViewport();
        const Vector camera{ viewport->mCameraMatrix[3][0], viewport->mCameraMatrix[3][1], viewport->mCameraMatrix[3][2] };
        const float inland = std::clamp((camera.z-75.0f)/40.0f, 0.0f, 1.0f);
        const uint32_t time = *milliseconds;
        const bool tbogt = *_dwCurrentEpisode == 2;
        const float nativeScale = NativeAircraftScale(tbogt, std::span<const uint8_t, 2>(aircraftSizeBranch, 2));
        SubmitConsoleCoronas(tbogt, nativeScale, positions, colors, count, right, up, [&](auto append)
        {
            auto emit = [&](Vector light, float intensity, uint32_t rgb)
            {
                const float distance = (light-camera).Length();
                // Retail Xbox culls blocks and stored path spheres, then applies
                // only this near-distance test to individual sprites. GPU clipping
                // handles the submitted quads; do not add per-light view/far tests.
                if (distance <= 150.0f) return;
                // Use the game's shared generator once per emitted sprite/pass;
                // retain Xbox's range mapping and staged integer colour rounding.
                append(light, XboxSpriteColor(rgb, intensity, distance, XboxBrightness(uint32_t(game_rand()))));
            };
            for (size_t block = 0; block < 64; ++block)
            {
                // Native 8x8 block coordinates and conservative visibility sphere.
                const float x = (float(block/8)+0.5f)*750.0f-3000.0f;
                const float y = (float(block%8)+0.5f)*750.0f-3000.0f;
                if (!sphereVisible(viewport, x, y, 20.0f, 525.0f, nullptr)) continue;
                for (size_t interior = 0; interior < 2; ++interior)
                {
                    if (interior && camera.z <= 75.0f) continue;
                    const uint32_t begin = xbox.firstString[block] + (interior ? xbox.coastalCount[block] : 0);
                    const uint32_t size = interior ? xbox.interiorCount[block] : xbox.coastalCount[block];
                    for (uint32_t i = begin; i < begin+size; ++i)
                    {
                        const auto& line = xbox.strings[i];
                        const auto& road = roads[i];
                        if (road.Length() <= 0.0f || !sphereVisible(viewport, float(line.x), float(line.y),
                            float(line.z), float(line.radius), nullptr)) continue;
                        const float traffic = nightFade*(interior ? inland : 1.0f);
                        ForEachVehicle(road, time, [&](Vector light, bool red, uint32_t)
                        {
                            emit(light, traffic, red ? 0xFF0000u : 0xFFFFFFu);
                        });
                        if (nightFade < StreetlightThreshold(xbox.points[line.firstPoint])) continue;
                        for (float s = 0; s < road.Length(); s += 30.0f)
                        {
                            auto light = road.At(s).position;
                            light.z += 12.0f;
                            // Streetlights switch as whole strings; nightFade is
                            // a threshold here, not a second continuous dimmer.
                            emit(light, 1.0f, 0xFFDC80u);
                        }
                    }
                }
            }
        }, RenderSprites);
    }

    static void __cdecl Day(float value)
    {
        if (GetMode() != Mode::Console) originalDay(value);
    }

    // Only the synchronous native road-sprite traversal sees empty PC block
    // counts. The PC points/strings are never replaced; restore counts before
    // returning, including on C++ unwinding. Aerial lights and render setup stay
    // native. The separate lodlights module is not involved in submission.
    struct EmptyPcBlocks
    {
        std::array<int32_t, 64> coastal, interior;
        EmptyPcBlocks()
        {
            std::copy_n(coastalCount, 64, coastal.begin());
            std::copy_n(interiorCount, 64, interior.begin());
            std::fill_n(coastalCount, 64, 0);
            std::fill_n(interiorCount, 64, 0);
            consolePass = true;
        }
        ~EmptyPcBlocks()
        {
            consolePass = false;
            std::copy(coastal.begin(), coastal.end(), coastalCount);
            std::copy(interior.begin(), interior.end(), interiorCount);
        }
    };

    static void __cdecl NightSprites(float fade)
    {
        nightFade = std::clamp(fade, 0.0f, 1.0f);
        if (GetMode() == Mode::Console)
        {
            EmptyPcBlocks scoped;
            originalNightSprites(fade);
        }
        else originalNightSprites(fade);
    }

    static void __cdecl NightModels()
    {
        if (GetMode() != Mode::Console) originalNightModels();
    }

    static uintptr_t Unique(std::string_view signature, uintptr_t begin = 0, uintptr_t end = 0)
    {
        auto pattern = begin ? hook::range_pattern(begin, end, signature) : hook::pattern(signature);
        return pattern.size() == 1 ? reinterpret_cast<uintptr_t>(pattern.get_first()) : 0;
    }

    static uintptr_t Target(uintptr_t instruction)
    {
        return instruction+5+*reinterpret_cast<int32_t*>(instruction+1);
    }

    static void Init()
    {
        // Resolve the complete CE instruction set before changing any call. No
        // absolute executable addresses or partially installed rendering hooks.
        const auto night = Unique("F3 0F 11 14 24 E8 ? ? ? ? 83 C4 04 E9");
        const auto day = Unique("F3 0F 11 04 24 E8 ? ? ? ? 83 C4 04 C6 05 ? ? ? ? 01 C3");
        const auto data = Unique("6A 04 68 ? ? ? ? 56 E8 ? ? ? ? 6A 04 68 ? ? ? ? 56 E8 ? ? ? ? 68 00 01 00 00");
        if (!night || !day || !data || !game_rand)
        {
            OutputDebugStringA("FusionFix: Distant Lights bindings unavailable; retaining PC rendering.\n");
            return;
        }
        const auto spritePass = Target(night+5);
        const auto modelPass = Target(night+13);
        const auto epilogue = "E8 ? ? ? ? 83 C4 10 C7 04 24 00 00 80 3F E8 ? ? ? ? 6A 00 6A 02";
        const auto spriteFinal = Unique(epilogue, spritePass, spritePass+0x1000);
        const auto timer = Unique("A1 ? ? ? ? 33 D2 F7 F1 F3 0F 10 15", modelPass, modelPass+0x700);
        const auto visibility = Unique("F3 0F 11 04 24 E8 ? ? ? ? 85 C0 0F 84 ? ? ? ? 33 FF", spritePass, spritePass+0x500);
        const auto streetlightIndex = Unique("25 0F 00 00 80 79 05 48 83 C8 F0 40 F3 0F 10 45 08 0F 2F 04 85 ? ? ? ? 0F 82",
            spritePass, spritePass+0x1000);
        // Match both the original conditional and FusionFix's NOP/JMP, whose
        // six-byte operand layout is identical. The body remains unmodified.
        const auto aircraftSize = Unique("83 3D ? ? ? ? 02 ? ? ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 10 8C 24",
            spritePass, spritePass+0x1000);
        if (!spriteFinal || !timer || !visibility || !streetlightIndex || !aircraftSize)
        {
            OutputDebugStringA("FusionFix: Distant Lights pass bindings unavailable; retaining PC rendering.\n");
            return;
        }
        const auto nativeFlush = Target(spriteFinal);
        const auto begin = Unique("8D 04 40 03 C0 50 6A 03 E8 ? ? ? ? 33 C9", nativeFlush, nativeFlush+0x400);
        const auto vertex = Unique("F3 0F 11 14 24 E8 ? ? ? ? F3 0F 10 44 24 54", nativeFlush, nativeFlush+0x400);
        const auto end = Unique("E8 ? ? ? ? C7 07 00 00 00 00 5F 5E 8B E5 5D C3", nativeFlush, nativeFlush+0x400);
        if (!begin || !vertex || !end)
        {
            OutputDebugStringA("FusionFix: Distant Lights sprite bindings unavailable; retaining PC rendering.\n");
            return;
        }
        aircraftSizeBranch = reinterpret_cast<const uint8_t*>(aircraftSize+7);
        beginSprites = reinterpret_cast<decltype(beginSprites)>(Target(begin+8));
        spriteVertex = reinterpret_cast<decltype(spriteVertex)>(Target(vertex+5));
        endSprites = reinterpret_cast<decltype(endSprites)>(Target(end));
        coastalCount = *reinterpret_cast<int32_t**>(data+48);
        interiorCount = *reinterpret_cast<int32_t**>(data+64);
        milliseconds = *reinterpret_cast<uint32_t**>(timer+1);
        originalDay = reinterpret_cast<decltype(originalDay)>(Target(day+5));
        originalNightSprites = reinterpret_cast<decltype(originalNightSprites)>(spritePass);
        originalNightModels = reinterpret_cast<decltype(originalNightModels)>(modelPass);
        flushSprites = reinterpret_cast<SpriteFlush>(nativeFlush);
        sphereVisible = reinterpret_cast<decltype(sphereVisible)>(Target(visibility+5));

        // CE native streetlight hash: AND EAX,8000000Fh implements signed
        // remainder before the table read. Drop the sign bit so the result is
        // always 0..15; JNS then skips the existing negative-remainder repair.
        // Only this immediate byte changes. The native table, coordinate hash,
        // DAT geometry, spacing, traffic models and subsequent COMISS stay intact.
        // Console uses its own corrected lookup; this also fixes PC fallback.
        injector::WriteMemory<uint8_t>(streetlightIndex+4, 0x00, true);

        injector::MakeCALL(spriteFinal, FinalSpriteFlush, true);
        injector::MakeCALL(day+5, Day, true);
        injector::MakeCALL(night+5, NightSprites, true);
        injector::MakeJMP(night+13, NightModels, true);
        ready = true;
    }

    static struct Registration
    {
        Registration() { FusionFix::onInitEvent() += Init; }
    } registration;
}
