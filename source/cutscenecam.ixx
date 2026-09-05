module;

#include <common.hxx>
#include <chrono>

export module cutscenecam;

import common;
import comvars;
import natives;
import settings;

uint8_t* g_CutsceneAudioEntity = nullptr;
int* dword_12957B8 = nullptr;

bool LostFocusResyncTimerActive = false;
std::chrono::steady_clock::time_point LostFocusResyncTimerStart{};

namespace CCutsceneManager
{
    static inline float dword_1295798;
    static inline float dword_129579C;
    static inline float dword_12957A4;
    static inline float dword_12957A8;
    static inline float dword_12957B0;
    static inline float dword_12957B4;
    static inline float dword_12957A0;
    static inline float dword_12957AC;

    static inline float dword_12957BC;
    static inline float dword_12957C0;
    static inline float dword_12957C8;
    static inline float dword_12957CC;
    static inline float dword_12957D4;
    static inline float dword_12957D8;
    static inline float dword_12957C4;
    static inline float dword_12957D0;

    SafetyHookInline shGetTimeStep = {};
    void __cdecl GetTimeStep(float* a1)
    {
        float v1 = *a1;
        float v2 = 0.0f;
        int v3 = audCutsceneAudioEntity::GetPlayTimeMs(g_CutsceneAudioEntity);

        if (*_dwCurrentEpisode == 2)
        {
            int v4 = *dword_12957B8;

            if ((*dword_12957B8 & 1) == 0)
            {
                v4 = *dword_12957B8 | 1;
                *dword_12957B8 |= 1u;
                dword_1295798 = 120.0f;
                dword_129579C = 0.0f;
                dword_12957A4 = 180.0f;
                dword_12957A8 = -300.0f;
                dword_12957B0 = 350.0f;
                dword_12957B4 = -500.0f;
                dword_12957A0 = 0.016666668f;
                dword_12957AC = 0.0058823531f;
            }

            if ((v4 & 2) == 0)
            {
                *dword_12957B8 = v4 | 2;
                dword_12957BC = 45.0f;
                dword_12957C0 = 0.0f;
                dword_12957C8 = 180.0f;
                dword_12957CC = -400.0f;
                dword_12957D4 = 350.0f;
                dword_12957D8 = -400.0f;
                dword_12957C4 = 0.0074074073f;
                dword_12957D0 = 0.0058823531f;
            }

            if (!_stricmp(pszCurrentCutsceneName, "e2_int"))
            {
                float v5 = v3 * 0.001f;

                if ((v5 - dword_1295798) < 0.0f)
                    v2 = dword_129579C;
                else
                    v2 = (((v5 - dword_1295798) * dword_12957A0) * (dword_12957A8 - dword_129579C)) + dword_129579C;

                if ((v5 - dword_12957A4) >= 0.0f)
                    v2 = (((v5 - dword_12957A4) * dword_12957AC) * (dword_12957B4 - dword_12957A8)) + dword_12957A8;

                if ((v5 - dword_12957B0) >= 0.0f)
                    v2 = dword_12957B4;
            }
            else if (!_stricmp(pszCurrentCutsceneName, "GT06_AA"))
            {
                float v6 = v3 * 0.001f;

                if ((v6 - dword_12957BC) < 0.0f)
                    v2 = dword_12957C0;
                else
                    v2 = (((v6 - dword_12957BC) * dword_12957C4) * (dword_12957CC - dword_12957C0)) + dword_12957C0;

                if ((v6 - dword_12957C8) >= 0.0f)
                    v2 = (((v6 - dword_12957C8) * dword_12957D0) * (dword_12957D8 - dword_12957CC)) + dword_12957CC;

                if ((v6 - dword_12957D4) >= 0.0f)
                    v2 = dword_12957D8;
            }
            else
            {
                v2 = 0.0f;
            }
        }

        // Result
        int v7 = 0;

        if (v3 == -1)
        {
            v7 = -1;
        }
        else
        {
            v7 = v3 + (int)v2;
            if (v7 < 0)
                v7 = 0;
        }

        static auto CutsceneAudioSync = FusionFixSettings.GetRef("PREF_CUTSCENEAUDIOSYNC");
        static auto OldState = IsKeyboardKeyPressed(VK_UP);
        auto CurrentState = IsKeyboardKeyPressed(VK_UP);

        if (((OldState) == 0 && (CurrentState)) || Natives::IsButtonJustPressed(0, BUTTON_DPAD_UP))
        {
            int Mode = (CutsceneAudioSync->get() + 1) % 3;
            FusionFixSettings.Set("PREF_CUTSCENEAUDIOSYNC", Mode);

            switch (Mode)
            {
                case 0: // Off
                    Natives::PrintBig((char*)"CutscAudioSync0", 1000, 2);
                    break;

                case 1: // Alternative
                    Natives::PrintBig((char*)"CutscAudioSync1", 1000, 2);
                    break;

                case 2: // On
                    Natives::PrintBig((char*)"CutscAudioSync2", 1000, 2);
                    break;
            }
        }
        OldState = CurrentState;

        if (LostFocusResyncTimerActive)
        {
            auto Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - LostFocusResyncTimerStart);

            if (Elapsed.count() >= 1000) // One second sync, should be enough
            {
                LostFocusResyncTimerActive = false;
            }
        }

        float v8 = 0.0f;

        if (CutsceneAudioSync->get() == 2 || LostFocusResyncTimerActive)
        {
            // Vanilla code, only desyncs during section changes and fading
            if (*CCutsceneManager::ms_State != 8)
            {
                v8 = (*CTimer::m_gameTime * 1000.0f) + v1;
            }
            else if (v7 == -1)
            {
                v8 = (*CTimer::m_systemTime * 1000.0f) + v1;
            }
            else
            {
                v8 = (float)v7 - *CCutsceneManager::ms_fTimePassedSinceLastAudioStart;
                if (v8 < 0.0f)
                    v8 = 0.0f;
            }
        }
        else if (CutsceneAudioSync->get() == 1)
        {
            // Flipped vanilla code, only syncs during section changes and fading
            // NOTE: This may cause visible stutter, perhaps even half refresh frame drops (Sometimes) when sections change.
            if (*CCutsceneManager::ms_State == 8)
            {
                v8 = (*CTimer::m_gameTime * 1000.0f) + v1;
            }
            else if (v7 == -1)
            {
                v8 = (*CTimer::m_systemTime * 1000.0f) + v1;
            }
            else
            {
                v8 = (float)v7 - *CCutsceneManager::ms_fTimePassedSinceLastAudioStart;
                if (v8 < 0.0f)
                    v8 = 0.0f;
            }
        }
        else if (CutsceneAudioSync->get() == 0)
        {
            // Always runs, may inevitably result in desync, which can then be mitigated manually unfortunately
            if ((v7 + v3 != 0) || ((v7 | v3) != 0))
            {
                v8 = *CTimer::m_gameTime * 1000.0f + v1;
            }
            else if (v7 == -1)
            {
                v8 = *CTimer::m_systemTime * 1000.0f + v1;
            }
            else
            {
                v8 = (float)v7 - *CCutsceneManager::ms_fTimePassedSinceLastAudioStart;
                if (v8 < 0.0f)
                    v8 = 0.0f;
            }
        }

        *a1 = v8;
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
                injector::MakeCALL(pattern.get_first(0), *(void**)&dest, true);

                // ??? kinda affects anims idk ???
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
                    dword_12957B8 = *pattern.get_first<int*>(2);
                }
                else
                {
                    pattern = hook::pattern("A1 ? ? ? ? A8 ? F3 0F 10 05");
                    dword_12957B8 = *pattern.get_first<int*>(1);
                }

                pattern = find_pattern("51 56 8B 74 24 ? 57 F3 0F 10 06", "51 56 57 8B 7C 24 ? F3 0F 10 07");
                CCutsceneManager::shGetTimeStep = safetyhook::create_inline(pattern.get_first(0), CCutsceneManager::GetTimeStep);

                // Alt-tab handler
                FusionFix::onActivateApp() += [](bool wParam)
                {
                    if (!wParam)
                    {
                        // Lost focus
                        LostFocusResyncTimerActive = false;
                    }
                    else
                    {
                        // Regained focus
                        LostFocusResyncTimerActive = true;
                        LostFocusResyncTimerStart = std::chrono::steady_clock::now();
                    }
                };
            }
        };
    }
} CutsceneCam;