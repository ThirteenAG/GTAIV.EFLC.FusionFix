module;

#include <common.hxx>

export module cutscenecam;

import common;
import comvars;
import natives;
import settings;

uint8_t* g_CutsceneAudioEntity = nullptr;
int* pGetTimeStepStaticsInitGuard = nullptr;

// The cutscene's time, smooth and locked to its audio. The audio thread updates the audio position in steps (and
// stops it when the stream stalls), so the time follows the real frame time between those updates and is pulled
// toward the audio position, never runs further ahead of it than one update and never goes back, unless the audio
// jumped (a new section's audio, a skip).
namespace CutsceneClock
{
    constexpr double ResyncMs = 250.0;   // the audio jumped, or the game hitched: take its position as it is
    constexpr double SlowPullMs = 500.0; // the time it takes to make up a small difference
    constexpr double FastPullMs = 100.0; // ... a larger one (30 ms or more)

    bool bLocked = false;
    double fClockMs = 0.0;      // the smooth audio position
    double fLastNowMs = 0.0;
    int nLastAudioMs = -1;
    double fAudioStepMs = 0.0;  // how far the audio position moves when it moves

    double NowMs()
    {
        static LARGE_INTEGER Frequency = []() { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
        LARGE_INTEGER Counter;
        QueryPerformanceCounter(&Counter);
        return double(Counter.QuadPart) * 1000.0 / double(Frequency.QuadPart);
    }

    void Unlock()
    {
        bLocked = false;
    }

    double Update(int AudioMs)
    {
        auto Now = NowMs();
        auto FrameMs = Now - fLastNowMs;
        fLastNowMs = Now;

        if (!bLocked)
        {
            bLocked = true;
            fClockMs = AudioMs;
            nLastAudioMs = AudioMs;
            fAudioStepMs = 0.0;
            return fClockMs;
        }

        if (AudioMs != nLastAudioMs)
        {
            auto Step = double(AudioMs - nLastAudioMs);
            if (Step > 0.0 && Step < ResyncMs)
                fAudioStepMs = fAudioStepMs > 0.0 ? fAudioStepMs + (Step - fAudioStepMs) * 0.1 : Step;
            nLastAudioMs = AudioMs;
        }

        auto Error = double(AudioMs) - fClockMs;
        if (FrameMs < 0.0 || FrameMs > ResyncMs || std::abs(Error) > ResyncMs)
        {
            // A jump in the audio (take it as it is), or a long frame: loading, a pause, the window in the background
            // (catch up with the audio, without going back)
            fClockMs = std::abs(Error) > ResyncMs ? double(AudioMs) : std::max(fClockMs, double(AudioMs));
            return fClockMs;
        }

        auto Clock = fClockMs + FrameMs;
        Error = double(AudioMs) - Clock;
        Clock += Error * std::min(1.0, FrameMs / (std::abs(Error) < 30.0 ? SlowPullMs : FastPullMs));

        // While the audio doesn't move (it stalled), wait for it
        auto MaxLead = std::max(fAudioStepMs, 10.0) + 5.0;
        Clock = std::min(Clock, double(AudioMs) + MaxLead);

        fClockMs = std::max(fClockMs, Clock);
        return fClockMs;
    }

}

namespace CCutsceneManager
{
    static inline float e2_int_time0;
    static inline float e2_int_offset0;
    static inline float e2_int_rate0;
    static inline float e2_int_time1;
    static inline float e2_int_offset1;
    static inline float e2_int_rate1;
    static inline float e2_int_time2;
    static inline float e2_int_offset2;

    static inline float GT06_AA_time0;
    static inline float GT06_AA_offset0;
    static inline float GT06_AA_rate0;
    static inline float GT06_AA_time1;
    static inline float GT06_AA_offset1;
    static inline float GT06_AA_rate1;
    static inline float GT06_AA_time2;
    static inline float GT06_AA_offset2;

    SafetyHookInline shGetTimeStep = {};
    void __cdecl GetTimeStep(float* a1)
    {
        float InTimeMs = *a1;
        float AudioTimeOffset = 0.0f;
        int PlayTimeMs = audCutsceneAudioEntity::GetPlayTimeMs(g_CutsceneAudioEntity);

        if (*_dwCurrentEpisode == 2)
        {
            int StaticsInitGuard = *pGetTimeStepStaticsInitGuard;

            if ((*pGetTimeStepStaticsInitGuard & 1) == 0)
            {
                StaticsInitGuard = *pGetTimeStepStaticsInitGuard | 1;
                *pGetTimeStepStaticsInitGuard |= 1u;
                e2_int_time0 = 120.0f;
                e2_int_offset0 = 0.0f;
                e2_int_time1 = 180.0f;
                e2_int_offset1 = -300.0f;
                e2_int_time2 = 350.0f;
                e2_int_offset2 = -500.0f;
                e2_int_rate0 = 0.016666668f;
                e2_int_rate1 = 0.0058823531f;
            }

            if ((StaticsInitGuard & 2) == 0)
            {
                *pGetTimeStepStaticsInitGuard = StaticsInitGuard | 2;
                GT06_AA_time0 = 45.0f;
                GT06_AA_offset0 = 0.0f;
                GT06_AA_time1 = 180.0f;
                GT06_AA_offset1 = -400.0f;
                GT06_AA_time2 = 350.0f;
                GT06_AA_offset2 = -400.0f;
                GT06_AA_rate0 = 0.0074074073f;
                GT06_AA_rate1 = 0.0058823531f;
            }

            if (!_stricmp(pszCurrentCutsceneName, "e2_int"))
            {
                float PlayTimeSec = PlayTimeMs * 0.001f;

                if ((PlayTimeSec - e2_int_time0) < 0.0f)
                    AudioTimeOffset = e2_int_offset0;
                else
                    AudioTimeOffset = (((PlayTimeSec - e2_int_time0) * e2_int_rate0) * (e2_int_offset1 - e2_int_offset0)) + e2_int_offset0;

                if ((PlayTimeSec - e2_int_time1) >= 0.0f)
                    AudioTimeOffset = (((PlayTimeSec - e2_int_time1) * e2_int_rate1) * (e2_int_offset2 - e2_int_offset1)) + e2_int_offset1;

                if ((PlayTimeSec - e2_int_time2) >= 0.0f)
                    AudioTimeOffset = e2_int_offset2;
            }
            else if (!_stricmp(pszCurrentCutsceneName, "GT06_AA"))
            {
                float PlayTimeSec = PlayTimeMs * 0.001f;

                if ((PlayTimeSec - GT06_AA_time0) < 0.0f)
                    AudioTimeOffset = GT06_AA_offset0;
                else
                    AudioTimeOffset = (((PlayTimeSec - GT06_AA_time0) * GT06_AA_rate0) * (GT06_AA_offset1 - GT06_AA_offset0)) + GT06_AA_offset0;

                if ((PlayTimeSec - GT06_AA_time1) >= 0.0f)
                    AudioTimeOffset = (((PlayTimeSec - GT06_AA_time1) * GT06_AA_rate1) * (GT06_AA_offset2 - GT06_AA_offset1)) + GT06_AA_offset1;

                if ((PlayTimeSec - GT06_AA_time2) >= 0.0f)
                    AudioTimeOffset = GT06_AA_offset2;
            }
            else
            {
                AudioTimeOffset = 0.0f;
            }
        }

        // Audio playback position with the drift ramp applied
        int AudioTimeMs = 0;

        if (PlayTimeMs == -1)
        {
            AudioTimeMs = -1;
        }
        else
        {
            AudioTimeMs = PlayTimeMs + (int)AudioTimeOffset;
            if (AudioTimeMs < 0)
                AudioTimeMs = 0;
        }

        float OutTimeMs = 0.0f;

        if (*CCutsceneManager::ms_State != 8)
        {
            // Not playing yet or switching sections: the game's time step, as vanilla
            CutsceneClock::Unlock();
            OutTimeMs = (*CTimer::m_gameTime * 1000.0f) + InTimeMs;
        }
        else if (AudioTimeMs == -1)
        {
            // No audio: the unscaled frame time, as vanilla
            CutsceneClock::Unlock();
            OutTimeMs = (*CTimer::m_systemTime * 1000.0f) + InTimeMs;
        }
        else
        {
            // Vanilla takes the audio position as it is. It only moves when the audio thread updates it, in steps that
            // don't line up with the frames, so the animations jitter at any frame rate. The clock below follows it
            // smoothly instead.
            OutTimeMs = float(CutsceneClock::Update(AudioTimeMs) - *CCutsceneManager::ms_fTimePassedSinceLastAudioStart);
            if (OutTimeMs < 0.0f)
                OutTimeMs = 0.0f;
        }

        *a1 = OutTimeMs;
    }
}

class CutsceneCam
{
public:
    CutsceneCam()
    {
        FusionFix::onInitEventAsync() += []()
        {
            // Skip two additional checks in cutscene camera update code added by Toronto on PC
            // which made cutscenes zoom erratically depending on fps and their jump cuts "flickery" because of it
            auto pattern = find_pattern("83 3D ? ? ? ? ? 0F 8E ? ? ? ? 83 FF", "83 3D ? ? ? ? ? 0F 8E ? ? ? ? 83 F8");
            injector::MakeNOP(pattern.get_first(0), 27, true);

            // Cutscene jitter workarounds
            {
                // By Sergeanur
                pattern = find_pattern("E8 ? ? ? ? 8B 4C 24 2C 5F 5E 33 CC B0 01", "E8 ? ? ? ? 8B 4C 24 2C 5F 5E 5B");

                static void* patchOffset = pattern.get_first(0);

                static void* pUpdateCameraFromPhase = injector::GetBranchDestination(patchOffset).get<void*>();

                pattern = find_pattern("C6 44 24 ? ? A1 ? ? ? ? 83 FF 03", "C6 44 24 ? ? 83 F9 03");
                static void* pJumpCutFlagByte = pattern.get_first(4);
                static double incrementalTimeStep = 0.0;

                struct CCamAnimated
                {
                    uint8_t pad0[0x20];
                    rage::Vector3 m_matrixB;
                    uint8_t pad1[0x14];
                    rage::Vector3 m_matrixPos;
                    uint8_t pad2[0x4B4];

                    bool UpdateCameraFromPhase(float fPhase)
                    {
                        return ((bool(__thiscall*)(CCamAnimated*, float))pUpdateCameraFromPhase)(this, fPhase);
                    }

                    bool UpdateCameraFromPhaseHook(float fPhase)
                    {
#if 1
                        incrementalTimeStep += *CTimer::fTimeStep;

                        CCamAnimated temp = *this;

                        injector::WriteMemory<uint8_t>(pJumpCutFlagByte, 1, true);
                        bool result = UpdateCameraFromPhase(fPhase) != 0.0;

                        CCamAnimated temp2 = *this;

                        if (incrementalTimeStep < 0.3333)
                            return result;

                        *this = temp;

                        injector::WriteMemory<uint8_t>(pJumpCutFlagByte, 0, true);
                        bool result2 = UpdateCameraFromPhase(fPhase) != 0.0;

                        temp = *this;

                        if (fabs(temp.m_matrixB.x - temp2.m_matrixB.x) > 0.03333f
                            || fabs(temp.m_matrixB.y - temp2.m_matrixB.y) > 0.03333f
                            || fabs(temp.m_matrixB.z - temp2.m_matrixB.z) > 0.03333f
                            || fabs(temp.m_matrixPos.x - temp2.m_matrixPos.x) > 0.3333f
                            || fabs(temp.m_matrixPos.y - temp2.m_matrixPos.y) > 0.3333f
                            || fabs(temp.m_matrixPos.z - temp2.m_matrixPos.z) > 0.3333f)
                        {
                            incrementalTimeStep = 0.0;
                            *this = temp2;
                            return result;
                        }
                        return result2;
#else
                        return UpdateCameraFromPhase(fPhase) != 0.0;
#endif
                    }
                };

                auto dest = &CCamAnimated::UpdateCameraFromPhaseHook;
                injector::MakeCALL(patchOffset, *(void**)&dest, true);

                pattern = find_pattern("E8 ? ? ? ? 8B CD 88 44 24 0F", "E8 ? ? ? ? 8B CF 88 44 24 0F");
                injector::MakeCALL(pattern.get_first(0), *(void**)&dest, true);

                // Camera cuts: near a cut (flagged in the camera animation, in 30 fps frames) the camera keeps its last
                // phase and stores the 30 fps frame it held at (+0x1B4), then keeps using that frame. Not storing it keeps
                // the hold to the frames that are really at the cut, at any frame rate.
                pattern = find_pattern("F3 0F 11 86 ? ? ? ? 5E 5B 8B 4C 24 30 33 CC E8 ? ? ? ? 83 C4 34 C2 04 00", "F3 0F 11 8F ? ? ? ? 5F 5E B8 ? ? ? ? 5D 83 C4 2C C2 04 00");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(0), 8, true);

                pattern = find_pattern("F3 0F 11 86 ? ? ? ? 5F 5E B8 ? ? ? ? 5B 8B 4C 24 30 33 CC E8 ? ? ? ? 83 C4 34 C2 04 00", "F3 0F 11 8F ? ? ? ? C6 87 ? ? ? ? ? 5F 5E B8 ? ? ? ? 5D 83 C4 2C C2 04 00");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(0), 8, true);
            }

            // Cutscene animation-audio sync adjustments
            {
                pattern = hook::pattern("B9 ? ? ? ? F3 0F 11 44 24 ? E8 ? ? ? ? 83 3D");
                g_CutsceneAudioEntity = *pattern.get_first<uint8_t*>(1);

                pattern = hook::pattern("8B 0D ? ? ? ? F6 C1 ? 75 ? 83 C9 ? 89 0D ? ? ? ? C7 05");
                if (!pattern.empty())
                {
                    pGetTimeStepStaticsInitGuard = *pattern.get_first<int*>(2);
                }
                else
                {
                    pattern = hook::pattern("A1 ? ? ? ? A8 ? F3 0F 10 05");
                    pGetTimeStepStaticsInitGuard = *pattern.get_first<int*>(1);
                }

                pattern = find_pattern("51 56 8B 74 24 ? 57 F3 0F 10 06", "51 56 57 8B 7C 24 ? F3 0F 10 07");
                CCutsceneManager::shGetTimeStep = safetyhook::create_inline(pattern.get_first(0), CCutsceneManager::GetTimeStep);
            }
        };
    }
} CutsceneCam;