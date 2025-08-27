module;

#include <common.hxx>
#include <chrono>

export module cutscenecam;

import common;
import comvars;
import natives;
import settings;

uint32_t* dwEpisodeID1 = nullptr;
uint8_t* g_cutsceneAudio = nullptr;
int* dword_12957B8 = nullptr;
float* float_129574C = nullptr;
float* float_11735BC = nullptr;
float* float_117359C = nullptr;

namespace rage
{
    namespace audCutsceneAudioEntity
    {
        int getAudioTimeMs(uint8_t* audioEntity)
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

std::chrono::steady_clock::time_point syncStartTime{};
bool syncTimerActive = false;
bool applicationLostFocus = false;
void __cdecl sub_9C2C80(float* a1)
{
    // Initialize variables
    constexpr float flt_1295798 = 120.0f;  // Start time for e2_int
    constexpr float flt_129579C = 0.0f;    // Initial offset
    constexpr float flt_12957A4 = 180.0f;  // Second phase start
    constexpr float flt_12957A8 = -300.0f; // Second phase offset
    constexpr float flt_12957B0 = 350.0f;  // Final phase start
    constexpr float flt_12957B4 = -500.0f; // Final offset
    constexpr float flt_12957A0 = 0.016666668f; // First phase multiplier
    constexpr float flt_12957AC = 0.0058823531f; // Second phase multiplier

    constexpr float flt_12957BC = 45.0f;   // Start time for GT06_AA
    constexpr float flt_12957C0 = 0.0f;    // Initial offset
    constexpr float flt_12957C8 = 180.0f;  // Second phase start
    constexpr float flt_12957CC = -400.0f; // Second phase offset
    constexpr float flt_12957D4 = 350.0f;  // Final phase start
    constexpr float flt_12957D8 = -400.0f; // Final offset
    constexpr float flt_12957C4 = 0.0074074073f; // First phase multiplier
    constexpr float flt_12957D0 = 0.0058823531f; // Second phase multiplier

    float time_offset = 0.0; // Animation timing offset
    int audio_time_ms = rage::audCutsceneAudioEntity::getAudioTimeMs(g_cutsceneAudio);
    float initial_time = *a1; // Store initial output time
    int adjusted_time; // Adjusted time to return
    float final_time; // Final output time

    // Apply timing adjustments for Episode ID 2
    if (*dwEpisodeID1 == 2)
    {
        int flags = *dword_12957B8;
        if (!(flags & 1))
        {
            *dword_12957B8 |= 1;
        }
        if (!(flags & 2))
        {
            *dword_12957B8 |= 2;
        }
    
        // Convert audio time to seconds
        float audio_time_sec = audio_time_ms * 0.001f;
    
        // Adjust offset for cutscene "e2_int"
        if (!_stricmp(pszCurrentCutsceneName, "e2_int"))
        {
            if (audio_time_sec < flt_1295798)
                time_offset = flt_129579C;
            else
                time_offset = ((audio_time_sec - flt_1295798) * flt_12957A0) * (flt_12957A8 - flt_129579C) + flt_129579C;
            if (audio_time_sec >= flt_12957A4)
                time_offset = ((audio_time_sec - flt_12957A4) * flt_12957AC) * (flt_12957B4 - flt_12957A8) + flt_12957A8;
            if (audio_time_sec >= flt_12957B0)
                time_offset = flt_12957B4;
        }
        // Adjust offset for cutscene "GT06_AA"
        else if (!_stricmp(pszCurrentCutsceneName, "GT06_AA"))
        {
            if (audio_time_sec < flt_12957BC)
                time_offset = flt_12957C0;
            else
                time_offset = ((audio_time_sec - flt_12957BC) * flt_12957C4) * (flt_12957CC - flt_12957C0) + flt_12957C0;
            if (audio_time_sec >= flt_12957C8)
                time_offset = ((audio_time_sec - flt_12957C8) * flt_12957D0) * (flt_12957D8 - flt_12957CC) + flt_12957CC;
            if (audio_time_sec >= flt_12957D4)
                time_offset = flt_12957D8;
        }
    }

    // Handle invalid audio time
    if (audio_time_ms == -1)
    {
        adjusted_time = -1;
    }
    else
    {
        // Calculate adjusted time with clamping
        adjusted_time = audio_time_ms + (int)time_offset;
        if (adjusted_time < 0)
            adjusted_time = 0;
    }

    static auto cas = FusionFixSettings.GetRef("PREF_CUTSCENEAUDIOSYNC");
    constexpr auto BUTTON_DPAD_UP = 8;
    static auto oldState = GetAsyncKeyState(VK_UP);
    auto curState = GetAsyncKeyState(VK_UP);
    if (((oldState & 0x8000) == 0 && (curState & 0x8000)) || Natives::IsButtonJustPressed(0, BUTTON_DPAD_UP))
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
            final_time = *float_11735BC * 1000.0f + initial_time;
        }
        else if (adjusted_time == -1)
        {
            final_time = *float_117359C * 1000.0f + initial_time;
        }
        else
        {
            final_time = (float)adjusted_time - *float_129574C;
            if (final_time < 0.0f)
                final_time = 0.0f;
        }
    }
    else
    {
        if ((adjusted_time + audio_time_ms != 0) || ((adjusted_time | audio_time_ms) != 0)) // the hell is this? equivalent to noping `cmp     CCutscenes__m_dwCutsceneState, 8`
        {
            final_time = *float_11735BC * 1000.0f + initial_time;
        }
        else
        {
            if (adjusted_time == -1)
            {
                final_time = *float_117359C * 1000.0f + initial_time;
            }
            else
            {
                final_time = (float)adjusted_time - *float_129574C;
                if (final_time < 0.0f)
                    final_time = 0.0f;
            }
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
            // By Sergeanur
            auto pattern = find_pattern("74 20 83 FF 03 74 1B 83", "74 24 8B 44 24 2C");
            injector::WriteMemory<uint8_t>(pattern.get_first(), 0xEB, true);

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
            g_cutsceneAudio = *pattern.get_first<uint8_t*>(1);

            pattern = find_pattern("8B 0D ? ? ? ? F6 C1 ? 75 ? 83 C9 ? 89 0D ? ? ? ? C7 05");
            if (!pattern.empty())
                dword_12957B8 = *pattern.get_first<int*>(2);
            else
            {
                pattern = hook::pattern("A1 ? ? ? ? A8 01 F3 0F 10 05 ? ? ? ? F3 0F 10 0D ? ? ? ? F3 0F 10 15 ? ? ? ? 75 68");
                dword_12957B8 = *pattern.get_first<int*>(1);
            }

            pattern = find_pattern("F3 0F 5C 05 ? ? ? ? 0F 2F D0 76 ? 5F", "F3 0F 5C 05 ? ? ? ? 0F 2F E8 76 1E F3 0F 11 2F 5F 5E 59 C3");
            float_129574C = *pattern.get_first<float*>(4);

            pattern = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 59 05 ? ? ? ? F3 0F 58 44 24 ? 5F", "F3 0F 10 05 ? ? ? ? F3 0F 59 05 ? ? ? ? F3 0F 58 44 24 ? F3 0F 11 07 5F 5E 59 C3");
            float_11735BC = *pattern.get_first<float*>(4);

            pattern = find_pattern("F3 0F 10 05 ? ? ? ? EB ? 66 0F 6E C0", "F3 0F 10 05 ? ? ? ? EB 21 F3 0F 2A C0 F3 0F 5C 05 ? ? ? ? 0F 2F E8 76 1E F3 0F 11 2F 5F 5E 59 C3");
            float_117359C = *pattern.get_first<float*>(4);

            pattern = find_pattern("83 3D ? ? ? ? ? 0F 57 D2", "83 3D ? ? ? ? ? 0F 57 ED 8B F0");
            dwEpisodeID1 = *pattern.get_first<uint32_t*>(2);

            pattern = find_pattern("51 56 8B 74 24 ? 57 F3 0F 10 06", "51 56 57 8B 7C 24 10 F3 0F 10 07 B9 ? ? ? ? F3 0F 11 44 24 ? E8 ? ? ? ? 83 3D ? ? ? ? ? 0F 57 ED 8B F0");
            static auto shCutscAudioSync = safetyhook::create_inline(pattern.get_first(), sub_9C2C80);

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