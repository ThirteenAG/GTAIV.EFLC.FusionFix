module;

#include <common.hxx>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib") // needed for timeBeginPeriod()/timeEndPeriod()

export module framelimit;

import common;
import comvars;
import settings;

int32_t nFrameLimitType;
float fFpsLimit;
float fCutsceneFpsLimit;
float fScriptCutsceneFpsLimit;
float fScriptCutsceneFovLimit;
float fLoadingFpsLimit;

class FrameLimiter
{
public:
    enum FPSLimitMode { FPS_NONE, FPS_REALTIME, FPS_ACCURATE };
    FPSLimitMode mFPSLimitMode = FPS_NONE;
private:
    double TIME_Frequency = 0.0;
    double TIME_Ticks = 0.0;
    double TIME_Frametime = 0.0;
    float  fFPSLimit = 0.0f;
    bool   bFpsLimitWasUpdated = false;
public:
    void Init(FPSLimitMode mode, float fps_limit)
    {
        bFpsLimitWasUpdated = true;
        mFPSLimitMode = mode;
        fFPSLimit = fps_limit;

        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        static constexpr auto TICKS_PER_FRAME = 1;
        auto TICKS_PER_SECOND = (TICKS_PER_FRAME * fFPSLimit);
        if (mFPSLimitMode == FPS_ACCURATE)
        {
            TIME_Frametime = 1000.0 / (double)fFPSLimit;
            TIME_Frequency = (double)frequency.QuadPart / 1000.0; // ticks are milliseconds
        }
        else // FPS_REALTIME
        {
            TIME_Frequency = (double)frequency.QuadPart / (double)TICKS_PER_SECOND; // ticks are 1/n frames (n = fFPSLimit)
        }
        Ticks();
    }
    DWORD Sync_RT()
    {
        if (bFpsLimitWasUpdated)
        {
            bFpsLimitWasUpdated = false;
            return 1;
        }

        DWORD lastTicks, currentTicks;
        LARGE_INTEGER counter;

        QueryPerformanceCounter(&counter);
        lastTicks = (DWORD)TIME_Ticks;
        TIME_Ticks = (double)counter.QuadPart / TIME_Frequency;
        currentTicks = (DWORD)TIME_Ticks;

        return (currentTicks > lastTicks) ? currentTicks - lastTicks : 0;
    }
    DWORD Sync_SLP()
    {
        if (bFpsLimitWasUpdated)
        {
            bFpsLimitWasUpdated = false;
            return 1;
        }

        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        double millis_current = (double)counter.QuadPart / TIME_Frequency;
        double millis_delta = millis_current - TIME_Ticks;
        if (TIME_Frametime <= millis_delta)
        {
            TIME_Ticks = millis_current;
            return 1;
        }
        else if (TIME_Frametime - millis_delta > 2.0) // > 2ms
            Sleep(1); // Sleep for ~1ms
        else
            Sleep(0); // yield thread's time-slice (does not actually sleep)

        return 0;
    }
    void Sync()
    {
        if (mFPSLimitMode == FPS_REALTIME)
            while (!Sync_RT());
        else if (mFPSLimitMode == FPS_ACCURATE)
            while (!Sync_SLP());
    }
private:
    void Ticks()
    {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        TIME_Ticks = (double)counter.QuadPart / TIME_Frequency;
    }
};

bool __cdecl sub_411F50(uint32_t* a1, uint32_t* a2)
{
    bLoadingShown = false;
    if (!a1[2] && !a2[2]) {
        bLoadingShown = (*a1 == *a2) && *a1;
        return *a1 == *a2;
    }
    if (a1[2] != a2[2])
        return false;
    if (a1[0] != a2[0])
        return false;
    if (a1[1] != a2[1])
        return false;
    bLoadingShown = true;
    return true;
}

FrameLimiter FpsLimiter;
FrameLimiter CutsceneFpsLimiter;
FrameLimiter ScriptCutsceneFpsLimiter;
FrameLimiter LoadingFpsLimiter;
FrameLimiter LoadingFpsLimiter2;
bool(*CCutscenes__hasCutsceneFinished)();
bool(*CCamera__isWidescreenBordersActive)();
bool bUnlockFramerateDuringLoadscreens = true;
void __cdecl sub_855640()
{
    static auto preset = FusionFixSettings.GetRef("PREF_FPS_LIMIT_PRESET");

    if ((CMenuManager::bLoadscreenShown && !*CMenuManager::bLoadscreenShown && !bLoadingShown) || !bUnlockFramerateDuringLoadscreens)
    {
        if (preset && *preset >= FusionFixSettings.FpsCaps.eCustom) {
            if (fFpsLimit > 0.0f || (*preset > FusionFixSettings.FpsCaps.eCustom && *preset < int32_t(FusionFixSettings.FpsCaps.data.size())))
                FpsLimiter.Sync();
        }
    }

    if (CCamera__isWidescreenBordersActive())
    {
        if (CCutscenes__hasCutsceneFinished())
        {
            if (fCutsceneFpsLimit)
                CutsceneFpsLimiter.Sync();
            else if (fScriptCutsceneFpsLimit)
                ScriptCutsceneFpsLimiter.Sync();

            // To avoid more softlocks with high fps
            if (!fCutsceneFpsLimit && CCutscenes::m_dwCutsceneState)
            {
                if (*CCutscenes::m_dwCutsceneState == 9 || *CCutscenes::m_dwCutsceneState == 10)
                    LoadingFpsLimiter2.Sync();
            }
        }
    }
}

injector::hook_back<void(__cdecl*)(void*)> hbsub_C64CB0;
void __cdecl sub_C64CB0(void* a1)
{
    LoadingFpsLimiter.Sync();
    return hbsub_C64CB0.fun(a1);
}

class Framelimit
{
public:
    Framelimit()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");

            //[FRAMELIMIT]
            nFrameLimitType = iniReader.ReadInteger("FRAMELIMIT", "FrameLimitType", 2);
            fFpsLimit = static_cast<float>(iniReader.ReadInteger("FRAMELIMIT", "FpsLimit", 0));
            fCutsceneFpsLimit = static_cast<float>(iniReader.ReadInteger("FRAMELIMIT", "CutsceneFpsLimit", 0));
            fScriptCutsceneFpsLimit = static_cast<float>(iniReader.ReadInteger("FRAMELIMIT", "ScriptCutsceneFpsLimit", 0));
            fScriptCutsceneFovLimit = static_cast<float>(iniReader.ReadInteger("FRAMELIMIT", "ScriptCutsceneFovLimit", 0));
            fLoadingFpsLimit = static_cast<float>(iniReader.ReadInteger("FRAMELIMIT", "LoadingFpsLimit", 30));
            bUnlockFramerateDuringLoadscreens = iniReader.ReadInteger("FRAMELIMIT", "UnlockFramerateDuringLoadscreens", 0) != 0;

            //if (fFpsLimit || fCutsceneFpsLimit || fScriptCutsceneFpsLimit)
            {
                auto mode = (nFrameLimitType == 2) ? FrameLimiter::FPSLimitMode::FPS_ACCURATE : FrameLimiter::FPSLimitMode::FPS_REALTIME;
                if (mode == FrameLimiter::FPSLimitMode::FPS_ACCURATE)
                    timeBeginPeriod(1);

                auto preset = FusionFixSettings("PREF_FPS_LIMIT_PRESET");
                if (preset > FusionFixSettings.FpsCaps.eCustom && preset < int32_t(FusionFixSettings.FpsCaps.data.size()))
                    FpsLimiter.Init(mode, (float)FusionFixSettings.FpsCaps.data[preset]);
                else
                    FpsLimiter.Init(mode, fFpsLimit);
                CutsceneFpsLimiter.Init(mode, fCutsceneFpsLimit);
                ScriptCutsceneFpsLimiter.Init(mode, fScriptCutsceneFpsLimit);
                LoadingFpsLimiter.Init(mode, std::clamp(fLoadingFpsLimit, 30.0f, FLT_MAX));
                LoadingFpsLimiter2.Init(mode, 240.0f);

                auto pattern = find_pattern("E8 ? ? ? ? 84 C0 75 89", "E8 ? ? ? ? 84 C0 75 15 38 05");
                CCutscenes__hasCutsceneFinished = (bool(*)()) injector::GetBranchDestination(pattern.get_first(0)).get();
                pattern = find_pattern("E8 ? ? ? ? 84 C0 75 44 38 05 ? ? ? ? 74 26", "E8 ? ? ? ? 84 C0 75 42 38 05");
                CCamera__isWidescreenBordersActive = (bool(*)()) injector::GetBranchDestination(pattern.get_first(0)).get();

                pattern = find_pattern("8B 35 ? ? ? ? 8B 0D ? ? ? ? 8B 15 ? ? ? ? A1", "A1 ? ? ? ? 83 F8 01 8B 0D");
                injector::WriteMemory(pattern.get_first(0), 0x901CC483, true); //nop + add esp,1C
                injector::MakeJMP(pattern.get_first(4), sub_855640, true); // + jmp

                FusionFixSettings.SetCallback("PREF_FPS_LIMIT_PRESET", [](int32_t value) {
                    auto mode = (nFrameLimitType == 2) ? FrameLimiter::FPSLimitMode::FPS_ACCURATE : FrameLimiter::FPSLimitMode::FPS_REALTIME;
                    if (value > FusionFixSettings.FpsCaps.eCustom && value < int32_t(FusionFixSettings.FpsCaps.data.size()))
                        FpsLimiter.Init(mode, (float)FusionFixSettings.FpsCaps.data[value]);
                    else
                        FpsLimiter.Init(mode, fFpsLimit);
                });

                pattern = find_pattern("8B 4C 24 04 8B 54 24 08 8B 41 08");
                if (!pattern.empty())
                    injector::MakeJMP(pattern.get_first(0), sub_411F50, true);

                //unlimit loadscreens fps
                static float f0 = 0.0f;
                pattern = hook::pattern("F3 0F 10 05 ? ? ? ? 2B C1 1B D6 89 44 24 18 89 54 24 1C DF 6C 24 18 D9 5C 24 18 F3 0F 10 4C 24 ? F3 0F 59 0D ? ? ? ? 0F 2F C1");
                if (!pattern.empty())
                    injector::WriteMemory(pattern.get_first(4), &f0, true);
            }

            if (fScriptCutsceneFovLimit)
            {
                auto pattern = find_pattern("8B 4E 0C 89 48 60 5E C3", "D9 46 0C D9 58 60");
                struct SetFOVHook
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        float f = *(float*)(regs.esi + 0x0C);
                        *(float*)(regs.eax + 0x60) = f > fScriptCutsceneFovLimit ? f : fScriptCutsceneFovLimit;
                    }
                }; injector::MakeInline<SetFOVHook>(pattern.get_first(0), pattern.get_first(6));
            }
        };

        FusionFix::onInitEventAsync() += []()
        {
            // Off Route infinite loading (CCutsceneObject method causes CRenderer::removeAllTexturesFromDictionary to softlock for unidentified reason)
            auto pattern = hook::pattern("E8 ? ? ? ? 83 C4 0C C7 04 B5 ? ? ? ? ? ? ? ? 4E");
            if (!pattern.empty())
                hbsub_C64CB0.fun = injector::MakeCALL(pattern.get_first(0), sub_C64CB0).get();
        };

        FusionFix::onShutdownEvent() += []()
        {
            if (nFrameLimitType == FrameLimiter::FPSLimitMode::FPS_ACCURATE)
                timeEndPeriod(1);
        };
    }
} Framelimit;