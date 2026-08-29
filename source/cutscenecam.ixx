module;

#include <common.hxx>
#include <chrono>

export module cutscenecam;

import common;
import comvars;
import natives;
import settings;

uint32_t* dwEpisodeID1 = nullptr;
uint8_t* g_CutsceneAudioEntity = nullptr;
float* CCutsceneManager_ms_fTimePassedSinceLastAudioStart = nullptr;
float* CTimer_m_systemTime_fTimeStep = nullptr;

namespace rage
{
    namespace audCutsceneAudioEntity
    {
        int GetPlayTimeMs(uint8_t* audioEntity)
        {
            // Get the next buffer index in the ping-pong buffer system
            uint8_t nextBufferIndex = (audioEntity[160] + 1) % 2;

            // Get pointer to the audio buffer using the calculated index
            uint32_t* audioBuffer = *(uint32_t**)(&audioEntity[8 + (nextBufferIndex * 4)]);

            // Return audio time from buffer or -1 if buffer is null
            return audioBuffer ? *(uint32_t*)((uint8_t*)audioBuffer + 184) : -1;
        }
    }
}

struct AudioDriftRamp
{
    float t0, off0, rate0; // held at off0 until t0, then ramps towards off1
    float t1, off1, rate1; // from t1 ramps from off1 towards off2
    float t2, off2;        // pinned at off2 from t2 onwards

    constexpr float operator()(float t) const
    {
        float off = off0;
        if (t >= t0)
            off = (t - t0) * rate0 * (off1 - off0) + off0;
        if (t >= t1)
            off = (t - t1) * rate1 * (off2 - off1) + off1;
        if (t >= t2)
            off = off2;
        return off;
    }
};

static constexpr AudioDriftRamp rampE2Int  { 120.0f, 0.0f, 0.016666668f,  180.0f, -300.0f, 0.0058823531f, 350.0f, -500.0f };
static constexpr AudioDriftRamp rampGT06AA {  45.0f, 0.0f, 0.0074074073f, 180.0f, -400.0f, 0.0058823531f, 350.0f, -400.0f };

std::chrono::steady_clock::time_point syncStartTime{};
bool syncTimerActive = false;
bool applicationLostFocus = false;
void __cdecl CCutsceneManager__GetTimeStep(float* a1)
{
    float time_offset = 0.0f; // Animation timing offset, in milliseconds
    int audio_time_ms = rage::audCutsceneAudioEntity::GetPlayTimeMs(g_CutsceneAudioEntity);
    float initial_time = *a1; // Store initial output time
    int raw_time;      // Pre-clamp sum, whose flags the original jnz reads
    int adjusted_time; // Adjusted time to return
    float final_time;  // Final output time

    // Apply timing adjustments for Episode ID 2
    if (*dwEpisodeID1 == 2)
    {
        // Convert audio time to seconds
        float audio_time_sec = audio_time_ms * 0.001f;

        if (!_stricmp(pszCurrentCutsceneName, "e2_int"))
            time_offset = rampE2Int(audio_time_sec);
        else if (!_stricmp(pszCurrentCutsceneName, "GT06_AA"))
            time_offset = rampGT06AA(audio_time_sec);
    }

    // Handle invalid audio time
    if (audio_time_ms == -1)
    {
        raw_time = -1; // original: or eax, edi
        adjusted_time = -1;
    }
    else
    {
        // Calculate adjusted time with clamping
        raw_time = audio_time_ms + (int)time_offset; // original: add eax, edi
        adjusted_time = raw_time;
        if (adjusted_time < 0)
            adjusted_time = 0; // original: cmovs eax, ecx
    }

    static auto cas = FusionFixSettings.GetRef("PREF_CUTSCENEAUDIOSYNC");
    static auto oldState = IsKeyboardKeyPressed(VK_UP);
    auto curState = IsKeyboardKeyPressed(VK_UP);
    if (((oldState) == 0 && (curState)) || Natives::IsButtonJustPressed(0, BUTTON_DPAD_UP))
    {
        FusionFixSettings.Set("PREF_CUTSCENEAUDIOSYNC", cas->get() ? 0 : 1);

        if (cas->get())
            Natives::PrintBig((char*)"CutscAudioSync1", 1000, 2);
        else
            Natives::PrintBig((char*)"CutscAudioSync0", 1000, 2);
    }
    oldState = curState;

    if (syncTimerActive)
    {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - syncStartTime);

        if (elapsedTime.count() >= 5000)
        {
            syncTimerActive = false;
            syncStartTime = std::chrono::steady_clock::time_point{};
        }
    }

    if (cas->get() || syncTimerActive || applicationLostFocus)
    {
        // Original code
        // Apply cutscene state-specific timing
        if (*CCutscenes::m_dwCutsceneState != 8)
        {
            final_time = *CTimer::fTimeStep * 1000.0f + initial_time;
        }
        else if (adjusted_time == -1)
        {
            final_time = *CTimer_m_systemTime_fTimeStep * 1000.0f + initial_time;
        }
        else
        {
            final_time = (float)adjusted_time - *CCutsceneManager_ms_fTimePassedSinceLastAudioStart;
            if (final_time < 0.0f)
                final_time = 0.0f;
        }
    }
    else
    {
        // Equivalent to NOPing `cmp CCutsceneManager::ms_State, 8` at 0x9C2EFB: the
        // following `jnz` then tests the flags left by the `or eax, edi` / `add eax, edi`
        // above, i.e. ZF is set only when the pre-clamp sum is exactly zero.
        if (raw_time != 0)
        {
            final_time = *CTimer::fTimeStep * 1000.0f + initial_time;
        }
        else if (adjusted_time == -1)
        {
            final_time = *CTimer_m_systemTime_fTimeStep * 1000.0f + initial_time;
        }
        else
        {
            final_time = (float)adjusted_time - *CCutsceneManager_ms_fTimePassedSinceLastAudioStart;
            if (final_time < 0.0f)
                final_time = 0.0f;
        }
    }

    *a1 = final_time;
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

            // By Sergeanur
            pattern = find_pattern("E8 ? ? ? ? 8B 4C 24 2C 5F 5E 33 CC B0 01", "E8 ? ? ? ? 8B 4C 24 2C 5F 5E 5B");

            static void* patchOffset = pattern.get_first();

            static void* originalHookster = injector::GetBranchDestination(patchOffset).get<void*>();

            pattern = find_pattern("C6 44 24 ? ? A1 ? ? ? ? 83 FF 03", "C6 44 24 ? ? 83 F9 03");
            static void* originalHooksterBytePatch = pattern.get_first(4);
            static double incrementalTimeStep = 0.0;

            struct CutsceneCamJitterWorkaround
            {
                float data[320];

                bool OriginalHookster(float a2)
                {
                    return ((bool(__thiscall*)(CutsceneCamJitterWorkaround*, float))originalHookster)(this, a2);
                }

                bool Hookster(float a2)
                {
                    #if 1
                    incrementalTimeStep += *CTimer::fTimeStep;

                    CutsceneCamJitterWorkaround temp = *this;

                    injector::WriteMemory<uint8_t>(originalHooksterBytePatch, 1, true);
                    bool result = OriginalHookster(a2) != 0.0;

                    CutsceneCamJitterWorkaround temp2 = *this;

                    if (incrementalTimeStep < 0.3333)
                        return result;

                    *this = temp;

                    injector::WriteMemory<uint8_t>(originalHooksterBytePatch, 0, true);
                    bool result2 = OriginalHookster(a2) != 0.0;

                    temp = *this;

                    if (fabs(temp.data[8] - temp2.data[8]) > 0.03333f
                        || fabs(temp.data[9] - temp2.data[9]) > 0.03333f
                        || fabs(temp.data[10] - temp2.data[10]) > 0.03333f
                        || fabs(temp.data[16] - temp2.data[16]) > 0.3333f
                        || fabs(temp.data[17] - temp2.data[17]) > 0.3333f
                        || fabs(temp.data[18] - temp2.data[18]) > 0.3333f)
                    {
                        incrementalTimeStep = 0.0;
                        *this = temp2;
                        return result;
                    }
                    return result2;
                    #else
                    return OriginalHookster(a2) != 0.0;
                    #endif
                }
            };

            auto dest = &CutsceneCamJitterWorkaround::Hookster;
            injector::MakeCALL(patchOffset, *(void**)&dest, true);

            pattern = find_pattern("E8 ? ? ? ? 8B CD 88 44 24 0F", "E8 ? ? ? ? 8B CF 88 44 24 0F");
            injector::MakeCALL(pattern.get_first(), *(void**)&dest, true);

            // ??? kinda affects anims idk ???
            pattern = find_pattern("F3 0F 11 86 ? ? ? ? 5E 5B 8B 4C 24 30 33 CC E8 ? ? ? ? 83 C4 34 C2 04 00", "F3 0F 11 8F ? ? ? ? 5F 5E B8 ? ? ? ? 5D 83 C4 2C C2 04 00");
            if (!pattern.empty())
                injector::MakeNOP(pattern.get_first(), 8, true);

            pattern = find_pattern("F3 0F 11 86 ? ? ? ? 5F 5E B8 ? ? ? ? 5B 8B 4C 24 30 33 CC E8 ? ? ? ? 83 C4 34 C2 04 00", "F3 0F 11 8F ? ? ? ? C6 87 ? ? ? ? ? 5F 5E B8 ? ? ? ? 5D 83 C4 2C C2 04 00");
            if (!pattern.empty())
                injector::MakeNOP(pattern.get_first(), 8, true);

            // timing? audio sync?
            pattern = find_pattern("B9 ? ? ? ? F3 0F 11 44 24 ? E8 ? ? ? ? 83 3D");
            g_CutsceneAudioEntity = *pattern.get_first<uint8_t*>(1);

            pattern = find_pattern("F3 0F 5C 05 ? ? ? ? 0F 2F D0 76 ? 5F", "F3 0F 5C 05 ? ? ? ? 0F 2F E8 76 1E F3 0F 11 2F 5F 5E 59 C3");
            CCutsceneManager_ms_fTimePassedSinceLastAudioStart = *pattern.get_first<float*>(4);

            pattern = find_pattern("F3 0F 10 05 ? ? ? ? EB ? 66 0F 6E C0", "F3 0F 10 05 ? ? ? ? EB 21 F3 0F 2A C0 F3 0F 5C 05 ? ? ? ? 0F 2F E8 76 1E F3 0F 11 2F 5F 5E 59 C3");
            CTimer_m_systemTime_fTimeStep = *pattern.get_first<float*>(4);

            pattern = find_pattern("83 3D ? ? ? ? ? 0F 57 D2", "83 3D ? ? ? ? ? 0F 57 ED 8B F0");
            dwEpisodeID1 = *pattern.get_first<uint32_t*>(2);

            pattern = find_pattern("51 56 8B 74 24 ? 57 F3 0F 10 06", "51 56 57 8B 7C 24 10 F3 0F 10 07 B9 ? ? ? ? F3 0F 11 44 24 ? E8 ? ? ? ? 83 3D ? ? ? ? ? 0F 57 ED 8B F0");
            static auto shCutscAudioSync = safetyhook::create_inline(pattern.get_first(), CCutsceneManager__GetTimeStep);

            FusionFix::onActivateApp() += [](bool wParam)
            {
                if (!wParam)
                {
                    applicationLostFocus = true;
                    syncTimerActive = false;
                }
                else
                {
                    applicationLostFocus = false;
                    syncStartTime = std::chrono::steady_clock::now();
                    syncTimerActive = true;
                }
            };
        };
    }
} CutsceneCam;