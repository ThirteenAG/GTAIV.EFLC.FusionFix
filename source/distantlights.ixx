module;

#include <common.hxx>
#include <fstream>
#include "distantlights-render.hxx"

export module distantlights;

import common;
import comvars;
import settings;

namespace DistantLights
{
    using RenderOutSpriteBufferFn = void(__cdecl*)(Vector*, uint32_t*, int*, Vector*, Vector*);
    // CE splits road models from CDistantLights::Render (streetlights and aircraft).
    static void(__cdecl* originalRenderDayModels)(float);
    static void(__cdecl* originalRender)(float);
    static void(__cdecl* originalRenderNightModels)();
    static RenderOutSpriteBufferFn nativeRenderOutSpriteBuffer;
    static void(__cdecl* nativeGrcBegin)(int, int);
    static void(__cdecl* nativeGrcVertex)(float, float, float, float, float, float, uint32_t, float, float);
    static void(__stdcall* nativeGrcEnd)();
    static int(__thiscall* nativeIsSphereVisible)(rage::grcViewport*, float, float, float, float, float*);
    static uint32_t* milliseconds;
    static const uint8_t* aircraftSizeBranch;
    static int32_t *coastalCount, *interiorCount;
    static bool ready{}, loadAttempted{}, consolePass{};
    static float nightFade{};
    static Dataset xbox;
    static std::vector<Road> roads;
    static SafetyHookMid pcStreetlightFade, pcDayTrafficFade, pcNightTrafficFade;

    static bool LoadConsoleData()
    {
        if (loadAttempted)
            return !roads.empty();
        loadAttempted = true;
        try
        {
            std::array<wchar_t, 32768> root{};
            std::filesystem::path folder;
            if (UAL::GetOverloadPathW && UAL::GetOverloadPathW(root.data(), root.size()) && root[0])
                folder = root.data();
            else
                folder = GetExeModulePath() / L"update";
            if (folder.is_relative())
                folder = GetExeModulePath() / folder;
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
                            decodedRoads[i].Assign(std::span(xbox.points.data() + line.firstPoint, line.pointCount));
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
        if (ready && FusionFixSettings.Get("PREF_DISTANTLIGHTS") == int(Mode::Console)
            && NativeAircraftScale(*_dwCurrentEpisode == 2, std::span<const uint8_t, 2>(aircraftSizeBranch, 2)) > 0.0f
            && LoadConsoleData())
            return Mode::Console;
        return Mode::Default;
    }

    static void RenderOutSpriteBuffer(Vector* positions, uint32_t* colors, int* count, Vector* right, Vector* up)
    {
        FlushCoronaBatch(consolePass, positions, colors, count, right, up,
            nativeRenderOutSpriteBuffer, nativeGrcBegin, nativeGrcVertex, nativeGrcEnd);
    }

    template<class Append>
    static void RenderConsoleRoads(rage::grcViewport* viewport, const Vector& camera, uint32_t time, Append append)
    {
        auto emit = [&](Vector light, float intensity, uint32_t rgb, bool interior)
        {
            if (interior && camera.z <= light.z)
                return;
            const float distance = (light - camera).Length();
            if (distance <= 150.0f)
                return;
            const float fadeDistance = interior ? HeightLimitedDistance(distance, camera.z, light.z, 100.0f) : distance;
            append(light, XboxSpriteColor(rgb, intensity, fadeDistance, XboxBrightness(uint32_t(game_rand()))));
        };
        for (size_t block = 0; block < 64; ++block)
        {
            // Xbox uses an 8x8 grid of 750 m blocks, culled with 525 m spheres.
            const float x = (float(block / 8) + 0.5f) * 750.0f - 3000.0f;
            const float y = (float(block % 8) + 0.5f) * 750.0f - 3000.0f;
            if (!nativeIsSphereVisible(viewport, x, y, 20.0f, 525.0f, nullptr))
                continue;
            for (size_t interior = 0; interior < 2; ++interior)
            {
                const uint32_t begin = xbox.firstString[block] + (interior ? xbox.coastalCount[block] : 0);
                const uint32_t size = interior ? xbox.interiorCount[block] : xbox.coastalCount[block];
                for (uint32_t i = begin; i < begin + size; ++i)
                {
                    const auto& line = xbox.strings[i];
                    const auto& road = roads[i];
                    if (road.Length() <= 0.0f || !nativeIsSphereVisible(viewport, float(line.x), float(line.y),
                        float(line.z), float(line.radius), nullptr))
                        continue;
                    ForEachVehicle(road, time, [&](Vector light, bool red, uint32_t)
                    {
                        emit(light, nightFade, red ? 0xFF0000u : 0xFFFFFFu, interior != 0);
                    });
                    // Streetlights switch whole strings at staggered thresholds.
                    if (nightFade < StreetlightThreshold(xbox.points[line.firstPoint]))
                        continue;
                    for (float s = 0; s < road.Length(); s += 30.0f)
                    {
                        auto light = road.At(s).position;
                        light.z += 12.0f;
                        emit(light, 1.0f, 0xFFDC80u, interior != 0);
                    }
                }
            }
        }
    }

    static void __cdecl RenderOutFinalSpriteBuffer(Vector* positions, uint32_t* colors, int* count, Vector* right, Vector* up)
    {
        if (!consolePass || nightFade <= 0.0f || !rage::pCurrentViewport || !*rage::pCurrentViewport)
        {
            RenderOutSpriteBuffer(positions, colors, count, right, up);
            return;
        }
        auto viewport = rage::GetCurrentViewport();
        const Vector camera{ viewport->mCameraMatrix[3][0], viewport->mCameraMatrix[3][1], viewport->mCameraMatrix[3][2] };
        const uint32_t time = *milliseconds;
        const bool tbogt = *_dwCurrentEpisode == 2;
        const float nativeScale = NativeAircraftScale(tbogt, std::span<const uint8_t, 2>(aircraftSizeBranch, 2));
        SubmitConsoleCoronas(tbogt, nativeScale, positions, colors, count, right, up, [&](auto append)
        {
            RenderConsoleRoads(viewport, camera, time, append);
        }, RenderOutSpriteBuffer);
    }

    static void __cdecl RenderDayModels(float value)
    {
        if (GetMode() != Mode::Console)
            originalRenderDayModels(value);
    }

    // Keep native aircraft and render setup while suppressing PC road sprites.
    struct ScopedPcRoadSuppression
    {
        std::array<int32_t, 64> coastal, interior;
        ScopedPcRoadSuppression()
        {
            std::copy_n(coastalCount, 64, coastal.begin());
            std::copy_n(interiorCount, 64, interior.begin());
            std::fill_n(coastalCount, 64, 0);
            std::fill_n(interiorCount, 64, 0);
            consolePass = true;
        }
        ScopedPcRoadSuppression(const ScopedPcRoadSuppression&) = delete;
        ScopedPcRoadSuppression& operator=(const ScopedPcRoadSuppression&) = delete;
        ~ScopedPcRoadSuppression()
        {
            consolePass = false;
            std::copy(coastal.begin(), coastal.end(), coastalCount);
            std::copy(interior.begin(), interior.end(), interiorCount);
        }
    };

    static void __cdecl Render(float fade)
    {
        nightFade = std::clamp(fade, 0.0f, 1.0f);
        if (GetMode() == Mode::Console)
        {
            ScopedPcRoadSuppression scoped;
            originalRender(fade);
        }
        else
            originalRender(fade);
    }

    static void __cdecl RenderNightModels()
    {
        if (GetMode() != Mode::Console)
            originalRenderNightModels();
    }

    static uintptr_t Unique(std::string_view signature, uintptr_t begin = 0, uintptr_t end = 0)
    {
        auto pattern = begin ? hook::range_pattern(begin, end, signature) : hook::pattern(signature);
        return pattern.size() == 1 ? reinterpret_cast<uintptr_t>(pattern.get_first()) : 0;
    }

    static uintptr_t Target(uintptr_t instruction)
    {
        return instruction + 5 + *reinterpret_cast<int32_t*>(instruction + 1);
    }

    static void Init()
    {
        // Resolve __vehicleImpostersCallback and CDistantLights::Load before patching.
        const auto night = Unique("F3 0F 11 14 24 E8 ? ? ? ? 83 C4 04 E9");
        const auto day = Unique("F3 0F 11 04 24 E8 ? ? ? ? 83 C4 04 C6 05 ? ? ? ? 01 C3");
        const auto data = Unique("6A 04 68 ? ? ? ? 56 E8 ? ? ? ? 6A 04 68 ? ? ? ? 56 E8 ? ? ? ? 68 00 01 00 00");
        if (!night || !day || !data || !game_rand)
        {
            OutputDebugStringA("FusionFix: Distant Lights bindings unavailable; retaining PC rendering.\n");
            return;
        }
        const auto spritePass = Target(night + 5); // CDistantLights::Render
        const auto modelPass = Target(night + 13);
        const auto dayPass = Target(day + 5);
        const auto epilogue = "E8 ? ? ? ? 83 C4 10 C7 04 24 00 00 80 3F E8 ? ? ? ? 6A 00 6A 02";
        const auto spriteFinal = Unique(epilogue, spritePass, spritePass + 0x1000);
        const auto timer = Unique("A1 ? ? ? ? 33 D2 F7 F1 F3 0F 10 15", modelPass, modelPass + 0x700);
        const auto visibility = Unique("F3 0F 11 04 24 E8 ? ? ? ? 85 C0 0F 84 ? ? ? ? 33 FF", spritePass, spritePass + 0x500);
        const auto streetlightIndex = Unique("25 0F 00 00 80 79 05 48 83 C8 F0 40 F3 0F 10 45 08 0F 2F 04 85 ? ? ? ? 0F 82",
            spritePass, spritePass + 0x1000);
        // Original JNE and FusionFix's NOP/JMP both occupy six bytes.
        const auto aircraftSize = Unique("83 3D ? ? ? ? 02 ? ? ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 10 8C 24",
            spritePass, spritePass + 0x1000);
        const auto heightGate = Unique("83 FF 01 0F 85 ? ? ? ? F3 0F 10 44 24 58 0F 2F 05 ? ? ? ? 0F 86 ? ? ? ?",
            spritePass, spritePass + 0x500);
        const auto spriteHeight = Unique("F3 0F 11 84 24 68 01 00 00 F3 0F 5C 84 24 C8 00 00 00 F3 0F 59 E4",
            spritePass, spritePass + 0x1000);
        const auto streetlightFade = Unique("51 8D 84 24 80 00 00 00 F3 0F 11 04 24 50 E8 ? ? ? ?",
            spritePass, spritePass + 0x1000);
        const auto roadGroup = Unique("8B 74 24 1C 8B 7C 24 4C 47 89 7C 24 4C 83 FF 02",
            spritePass, spritePass + 0x1000);
        const auto dayHeightGate = Unique("83 F9 01 0F 85 ? ? ? ? F3 0F 10 84 24 DC 00 00 00 0F 2F 05 ? ? ? ? 0F 86 ? ? ? ?",
            dayPass, dayPass + 0x500);
        const auto nightHeightGate = Unique("83 F9 01 0F 85 ? ? ? ? F3 0F 10 84 24 08 01 00 00 0F 2F 05 ? ? ? ? 0F 86 ? ? ? ?",
            modelPass, modelPass + 0x500);
        const auto dayTrafficFade = Unique("8B 0C CD ? ? ? ? 51 8D 84 24 94 01 00 00 50 8D 84 24 88 01 00 00",
            dayPass, dayPass + 0xCE0);
        const auto nightTrafficFade = Unique("6A 00 8D 84 24 A4 01 00 00 50 8D 84 24 98 01 00 00 50 8D 84 24 E8 00 00 00 50 68 ? ? ? ? 8D 84 24 94 01 00 00",
            modelPass, modelPass + 0xC70);
        // Verify the stack slots used below against their native stores.
        const auto dayTrafficGroup = Unique("89 8C 24 E4 00 00 00 85 C9 75 0E", dayPass, dayPass + 0x500);
        const auto nightTrafficGroup = Unique("89 8C 24 C8 00 00 00 85 C9 75 0E", modelPass, modelPass + 0x500);
        const auto dayTrafficHeight = Unique("F3 0F 11 94 24 08 01 00 00 F3 0F 5C 94 24 38 01 00 00", dayPass, dayPass + 0xCE0);
        const auto nightTrafficHeight = Unique("F3 0F 11 B4 24 18 01 00 00 F3 0F 5C B4 24 38 01 00 00", modelPass, modelPass + 0xC70);
        if (!spriteFinal || !timer || !visibility || !streetlightIndex || !aircraftSize ||
            !heightGate || !spriteHeight || !streetlightFade || !roadGroup ||
            !dayHeightGate || !nightHeightGate || !dayTrafficFade || !nightTrafficFade ||
            !dayTrafficGroup || !nightTrafficGroup || !dayTrafficHeight || !nightTrafficHeight)
        {
            OutputDebugStringA("FusionFix: Distant Lights pass bindings unavailable; retaining PC rendering.\n");
            return;
        }
        const auto nativeFlush = Target(spriteFinal); // CDistantLights::RenderOutSpriteBuffer
        const auto begin = Unique("8D 04 40 03 C0 50 6A 03 E8 ? ? ? ? 33 C9", nativeFlush, nativeFlush + 0x400);
        const auto vertex = Unique("F3 0F 11 14 24 E8 ? ? ? ? F3 0F 10 44 24 54", nativeFlush, nativeFlush + 0x400);
        const auto end = Unique("E8 ? ? ? ? C7 07 00 00 00 00 5F 5E 8B E5 5D C3", nativeFlush, nativeFlush + 0x400);
        if (!begin || !vertex || !end)
        {
            OutputDebugStringA("FusionFix: Distant Lights sprite bindings unavailable; retaining PC rendering.\n");
            return;
        }
        aircraftSizeBranch = reinterpret_cast<const uint8_t*>(aircraftSize + 7);
        nativeGrcBegin = reinterpret_cast<decltype(nativeGrcBegin)>(Target(begin + 8));
        nativeGrcVertex = reinterpret_cast<decltype(nativeGrcVertex)>(Target(vertex + 5));
        nativeGrcEnd = reinterpret_cast<decltype(nativeGrcEnd)>(Target(end));
        coastalCount = *reinterpret_cast<int32_t**>(data + 48);
        interiorCount = *reinterpret_cast<int32_t**>(data + 64);
        milliseconds = *reinterpret_cast<uint32_t**>(timer + 1);
        originalRenderDayModels = reinterpret_cast<decltype(originalRenderDayModels)>(dayPass);
        originalRender = reinterpret_cast<decltype(originalRender)>(spritePass);
        originalRenderNightModels = reinterpret_cast<decltype(originalRenderNightModels)>(modelPass);
        nativeRenderOutSpriteBuffer = reinterpret_cast<RenderOutSpriteBufferFn>(nativeFlush);
        nativeIsSphereVisible = reinterpret_cast<decltype(nativeIsSphereVisible)>(Target(visibility + 5));

        // Cap distance before CDistantLights::ModifyColourForDist to preserve native rounding.
        // CE stack: group +4C, camera Z +C8, sprite Z +168 (includes +12 placement).
        pcStreetlightFade = safetyhook::create_mid(streetlightFade, [](SafetyHookContext& regs)
        {
            if (*reinterpret_cast<const int*>(regs.esp + 0x4C) == 1)
                regs.xmm0.f32[0] = HeightLimitedDistance(regs.xmm0.f32[0],
                    *reinterpret_cast<const float*>(regs.esp + 0xC8),
                    *reinterpret_cast<const float*>(regs.esp + 0x168), 150.0f);
        });
        // Limit native model opacity: XMM1 by day, XMM0 at night.
        // Camera Z is ESP+138; model Z includes the +0.6 placement offset.
        pcDayTrafficFade = safetyhook::create_mid(dayTrafficFade, [](SafetyHookContext& regs)
        {
            if (*reinterpret_cast<const int*>(regs.esp + 0xE4) == 1)
                regs.xmm1.f32[0] = HeightLimitedOpacity(regs.xmm1.f32[0],
                    *reinterpret_cast<const float*>(regs.esp + 0x138),
                    *reinterpret_cast<const float*>(regs.esp + 0x108));
        });
        pcNightTrafficFade = safetyhook::create_mid(nightTrafficFade, [](SafetyHookContext& regs)
        {
            if (*reinterpret_cast<const int*>(regs.esp + 0xC8) == 1)
                regs.xmm0.f32[0] = HeightLimitedOpacity(regs.xmm0.f32[0],
                    *reinterpret_cast<const float*>(regs.esp + 0x138),
                    *reinterpret_cast<const float*>(regs.esp + 0x118));
        });
        if (!pcStreetlightFade || !pcDayTrafficFade || !pcNightTrafficFade)
        {
            pcStreetlightFade.reset();
            pcDayTrafficFade.reset();
            pcNightTrafficFade.reset();
            OutputDebugStringA("FusionFix: Distant Lights height fade unavailable; retaining PC rendering.\n");
            return;
        }
        // Replace the inland 75 m cutoffs with per-light height fades.
        injector::MakeNOP(heightGate + 22, 6, true);
        injector::MakeNOP(dayHeightGate + 25, 6, true);
        injector::MakeNOP(nightHeightGate + 25, 6, true);

        // Wrap the lookup to 0..15; JNS then skips the signed-remainder repair.
        injector::WriteMemory<uint8_t>(streetlightIndex + 4, 0x00, true);

        injector::MakeCALL(spriteFinal, RenderOutFinalSpriteBuffer, true);
        injector::MakeCALL(day + 5, RenderDayModels, true);
        injector::MakeCALL(night + 5, Render, true);
        injector::MakeJMP(night + 13, RenderNightModels, true);
        ready = true;
    }

    static struct Registration
    {
        Registration() { FusionFix::onInitEvent() += Init; }
    } registration;
}
