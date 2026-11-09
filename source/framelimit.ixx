module;

#include <common.hxx>
#include <regex>
#include <atomic>

export module framelimit;

import common;
import comvars;
import settings;
import natives;

float fFpsLimit;
float fCutsceneFpsLimit;
float fMinigamesFpsLimit;
std::vector<std::string> minigamesNames;

// Sleeps on a high resolution timer until shortly before each frame's time slot, then spins the rest, so the wait is
// exact without keeping a CPU core busy. Each slot follows the one before, so the pace doesn't drift; a frame that comes
// later than a whole slot starts the count anew instead of rushing the next ones.
class FrameLimiter
{
    std::atomic<LONGLONG> period{ 0 };
    std::atomic<bool> restart{ true };
    LONGLONG next = 0;

    static LONGLONG Frequency()
    {
        static const LONGLONG frequency = []
        {
            LARGE_INTEGER value{};
            QueryPerformanceFrequency(&value);
            return value.QuadPart;
        }();
        return frequency;
    }

    static LONGLONG Now()
    {
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        return now.QuadPart;
    }

    static void WaitUntil(LONGLONG target)
    {
        static const HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        constexpr double SpinTime = 0.0015;
        const double seconds = double(target - Now()) / double(Frequency());
        if (seconds > SpinTime)
        {
            LARGE_INTEGER due{};
            due.QuadPart = -static_cast<LONGLONG>((seconds - SpinTime) * 10000000.0);
            if (timer && SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
                WaitForSingleObject(timer, INFINITE);
            else
                Sleep(static_cast<DWORD>((seconds - SpinTime) * 1000.0));
        }
        while (Now() < target)
            YieldProcessor();
    }

public:
    void Init(float fps_limit)
    {
        period = fps_limit > 0.0f ? static_cast<LONGLONG>(double(Frequency()) / double(fps_limit)) : 0;
        restart = true;
    }

    void Sync()
    {
        const LONGLONG slot = period;
        if (slot <= 0)
            return;

        const LONGLONG now = Now();
        if (restart.exchange(false) || now > next + slot)
        {
            next = now + slot;
            return;
        }
        WaitUntil(next);
        next += slot;
    }
};

bool __cdecl sub_411F50(uint32_t* a1, uint32_t* a2)
{
    bLoadingShown = false;
    if (!a1[2] && !a2[2])
    {
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
FrameLimiter MinigamesFpsLimiter;
bool bUnlockFramerateDuringLoadscreens = true;
bool bNeedsToLimitFpsForThisMinigame = false;
void __cdecl sub_855640()
{
    static auto preset = FusionFixSettings.GetRef("PREF_FPS_LIMIT_PRESET");

    if ((CMenuManager::bLoadscreenShown && !*CMenuManager::bLoadscreenShown && !bLoadingShown) || !bUnlockFramerateDuringLoadscreens)
    {
        if (preset && *preset >= FusionFixSettings.FpsCaps.eCustom)
        {
            if (fFpsLimit != 0.0f || (*preset > FusionFixSettings.FpsCaps.eCustom && *preset < int32_t(FusionFixSettings.FpsCaps.data.size())))
                FpsLimiter.Sync();
        }
    }

    if (CCamera::isWidescreenBordersActive())
    {
        if (CCutsceneManager::IsRunning())
        {
            if (fCutsceneFpsLimit)
                CutsceneFpsLimiter.Sync();
        }
    }

    // The flag is only set once a script started a minigame, so the native table is ready when the native is called
    if (fMinigamesFpsLimit && bNeedsToLimitFpsForThisMinigame && Natives::IsMinigameInProgress())
        MinigamesFpsLimiter.Sync();
}

class Framelimit
{
public:
    Framelimit()
    {
        FusionFix::onInitEventAsync() += []()
        {
            SettingsReader reader;

            //[FRAMELIMIT]
            fFpsLimit = static_cast<float>(reader.ReadInteger("FRAMELIMIT", "FpsLimit", -1));
            fCutsceneFpsLimit = static_cast<float>(reader.ReadInteger("FRAMELIMIT", "CutsceneFpsLimit", 0));
            fMinigamesFpsLimit = static_cast<float>(reader.ReadInteger("FRAMELIMIT", "MinigamesFpsLimit", 30));
            std::string minigamesList = reader.ReadString("FRAMELIMIT", "MinigamesList", "");
            try
            {
                std::regex re("[^a-zA-Z0-9_]+");
                std::sregex_token_iterator iter(minigamesList.begin(), minigamesList.end(), re, -1);
                std::sregex_token_iterator end;
                for (; iter != end; ++iter)
                {
                    std::string token = iter->str();
                    if (!token.empty())
                    {
                        minigamesNames.push_back(token);
                    }
                }
            } catch (const std::exception&)
            {
                minigamesNames = { "pool_game", "air_hockey", "arm_wrestling", "tenpinbowl", "drinking" };
            }
            bUnlockFramerateDuringLoadscreens = reader.ReadInteger("FRAMELIMIT", "UnlockFramerateDuringLoadscreens", 0) != 0;

            //if (fFpsLimit || fCutsceneFpsLimit)
            {
                if (fMinigamesFpsLimit)
                    fMinigamesFpsLimit = std::clamp(fMinigamesFpsLimit, 30.0f, FLT_MAX);

                CutsceneFpsLimiter.Init(fCutsceneFpsLimit);
                MinigamesFpsLimiter.Init(fMinigamesFpsLimit);

                auto pattern = find_pattern("A3 ? ? ? ? E8 ? ? ? ? A1 ? ? ? ? 50 8B 08");
                if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
                {
                    static auto SetRefreshRateHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                    {
                        auto preset = FusionFixSettings("PREF_FPS_LIMIT_PRESET");
                        if (preset > FusionFixSettings.FpsCaps.eCustom && preset < int32_t(FusionFixSettings.FpsCaps.data.size()))
                            FpsLimiter.Init((float)FusionFixSettings.FpsCaps.data[preset]);
                        else
                        {
                            if (fFpsLimit > 0.0f)
                                FpsLimiter.Init(fFpsLimit);
                            else
                            {
                                if (regs.eax)
                                    FpsLimiter.Init(float(regs.eax) + fFpsLimit);
                                else
                                    FpsLimiter.Init(0.0f);
                            }
                        }
                    });
                }
                else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
                {
                    pattern = find_pattern("89 0D ? ? ? ? F6 05", "89 0D ? ? ? ? E8 ? ? ? ? A1 ? ? ? ? 8B 08");
                    static auto SetRefreshRateHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                    {
                        auto preset = FusionFixSettings("PREF_FPS_LIMIT_PRESET");
                        if (preset > FusionFixSettings.FpsCaps.eCustom && preset < int32_t(FusionFixSettings.FpsCaps.data.size()))
                            FpsLimiter.Init((float)FusionFixSettings.FpsCaps.data[preset]);
                        else
                        {
                            if (fFpsLimit > 0.0f)
                                FpsLimiter.Init(fFpsLimit);
                            else
                            {
                                if (regs.ecx)
                                    FpsLimiter.Init(float(regs.ecx) + fFpsLimit);
                                else
                                    FpsLimiter.Init(0.0f);
                            }
                        }
                    });
                }

                pattern = find_pattern("8B 35 ? ? ? ? 8B 0D ? ? ? ? 8B 15 ? ? ? ? A1", "A1 ? ? ? ? 83 F8 01 8B 0D");
                injector::WriteMemory(pattern.get_first(0), 0x901CC483, true); //nop + add esp,1C
                injector::MakeJMP(pattern.get_first(4), sub_855640, true); // + jmp

                FusionFixSettings.SetCallback("PREF_FPS_LIMIT_PRESET", [](int32_t value)
                {
                    if (value > FusionFixSettings.FpsCaps.eCustom && value < int32_t(FusionFixSettings.FpsCaps.data.size()))
                        FpsLimiter.Init((float)FusionFixSettings.FpsCaps.data[value]);
                    else
                    {
                        if (fFpsLimit > 0.0f)
                            FpsLimiter.Init(fFpsLimit);
                        else
                        {
                            if (*rage::grcWindow::ms_nActiveRefreshrate)
                                FpsLimiter.Init(float(*rage::grcWindow::ms_nActiveRefreshrate) + fFpsLimit);
                            else
                                FpsLimiter.Init(0.0f);
                        }
                    }
                });

                // Unlimit in-game loading screens' FPS
                pattern = find_pattern("8B 4C 24 04 8B 54 24 08 8B 41 08", "8B 54 24 04 8B 42 08 85 C0 8B 4C 24 08 75 12");
                injector::MakeJMP(pattern.get_first(0), sub_411F50, true);

                // Unlimit initial loading screens' FPS
                static float f0 = 0.0f;
                pattern = hook::pattern("F3 0F 10 05 ? ? ? ? 2B C1 1B D6 89 44 24 18 89 54 24 1C DF 6C 24 18 D9 5C 24 18 F3 0F 10 4C 24 ? F3 0F 59 0D ? ? ? ? 0F 2F C1");
                if (!pattern.empty()) // 1.2.0.32, 1.2.0.43, 1.2.0.59
                    injector::WriteMemory(pattern.get_first(4), &f0, true);
                else // 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0
                {
                    // 1.0.7.0 and 1.1.2.0 don't cap the initial loading screens
                    pattern = hook::pattern("8B 35 ? ? ? ? 2B C1 1B D6 89 44 24 18 89 54 24 1C DF 6C 24 18 D8 0D ? ? ? ? D9 05 ? ? ? ? DF F1"); // 1.0.8.0, 1.1.3.0
                    if (!pattern.empty()) // 1.0.8.0, 1.1.3.0
                        injector::WriteMemory(pattern.get_first(2), &f0, true);
                }
            }
        };

        FusionFix::onInitEventAsync() += []()
        {
            // The infinite loading before/after some cutscenes (Off Route, Momma's Boy) used to be avoided by limiting the frame
            // rate around them; the cause (CTimer's millisecond clocks running slow) is fixed in frameratevigilante.ixx
            auto pattern = find_pattern("FF 05 ? ? ? ? C3 E8", "83 05 ? ? ? ? ? C3 E8 ? ? ? ? 8B C8");
            static auto SET_MINIGAME_IN_PROGRESS_HOOK = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            {
                auto curThread = (rage::scrThread*)regs.eax;
                if (curThread)
                {
                    std::string curScript = curThread->m_szProgramName;
                    std::transform(curScript.begin(), curScript.end(), curScript.begin(), [](unsigned char c) { return std::tolower(c); });

                    if (std::any_of(std::begin(minigamesNames), std::end(minigamesNames), [&curScript](const auto& i) { return i == curScript; }))
                        bNeedsToLimitFpsForThisMinigame = true;
                    else
                        bNeedsToLimitFpsForThisMinigame = false;
                }
            });
        };
    }
} Framelimit;