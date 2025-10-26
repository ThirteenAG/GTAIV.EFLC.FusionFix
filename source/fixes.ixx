module;

#include <common.hxx>

export module fixes;

import common;
import comvars;
import settings;
import natives;
import shaders;

class Fixes
{
public:
    static inline int32_t nTimeToPassBeforeCenteringCameraOnFoot = 0;
    static inline int32_t nTimeToPassBeforeCenteringCameraInVehicle = 0;

    static inline bool* bIsPhoneShowing = nullptr;
    static inline injector::hook_back<int32_t(__cdecl*)()> hbsub_B2CE30;
    static int32_t sub_B2CE30() 
    {
        if ((bIsPhoneShowing && *bIsPhoneShowing))
            return 1;

        return hbsub_B2CE30.fun();
    }

    static inline injector::hook_back<void(__fastcall*)(int32_t, int32_t)> hbsub_B07600;
    static void __fastcall sub_B07600(int32_t _this, int32_t) 
    {
        hbsub_B07600.fun(_this, 0);

        static auto nCustomFOV = FusionFixSettings.GetRef("PREF_CUSTOMFOV");
        *(float*)(_this + 0x60) += nCustomFOV->get() * 5.0f;
    }

    static char sub_8D0A90()
    {
        if (bMenuNeedsUpdate2 > 0) {
            bMenuNeedsUpdate2--;
            return 0;
        }
        return *CTimer::m_UserPause;
    }

    static inline injector::hook_back<int(__fastcall*)(int* _this, void* edx, int a2)> hbsub_B64D60;
    static int __fastcall sub_B64D60(int* _this, void* edx, int a2)
    {
        auto r = hbsub_B64D60.fun(_this, edx, a2);

        if (r == 4 && _this[21] == 32) // P90
            r = hbsub_B64D60.fun(_this, edx, r);

        return r;
    }

    static inline injector::hook_back<void(*)()> hbsub_AD1240;
    static void sub_AD1240()
    {
        Cam rootCam = 0;
        Natives::GetRootCam(&rootCam);
        auto ccam = CCam::GetCamPool()->GetAt(rootCam);
        if (ccam)
        {
            auto ptr = *(uintptr_t*)((uintptr_t)ccam + 0x110);

            if (ptr)
            {
                if (*(uint8_t*)(ptr + 0x27C) == 1)
                    return;
            }
        }

        return hbsub_AD1240.fun();
    }

    static inline uint32_t* dword_1670CD0 = nullptr;
    static inline uint32_t* dwFrameCount = nullptr;
    static inline void* g_pSearchlightHeli = nullptr; // Pointer to the helicopter that has the light
    static inline int32_t g_dwSearchlightLockTime = 0; // Timestamp of when the lock expires
    static inline SafetyHookInline shCHelisub_B69D80 = {};
    static void __fastcall CHelisub_B69D80(void* _this, void* edx)
    {
        // Check if this helicopter's searchlight is supposed to be on.
        // This mirrors the check inside the original function.
        bool isLightActive = (*(int8_t*)((uintptr_t)_this + 8044) && *(float*)((uintptr_t)_this + 8036) > 0.0 && !*(int8_t*)((uintptr_t)_this + 8240));

        if (isLightActive)
        {
            auto currentTime = *CTimer::m_snTimeInMilliseconds;

            // A helicopter can acquire the lock if:
            // 1. No one has it (g_pSearchlightHeli is nullptr).
            // 2. This helicopter already has it.
            // 3. The lock from another helicopter has expired.
            if (g_pSearchlightHeli == nullptr || g_pSearchlightHeli == _this || currentTime > g_dwSearchlightLockTime)
            {
                // This helicopter gets the lock.
                g_pSearchlightHeli = _this;
                g_dwSearchlightLockTime = currentTime + 10000; // Lock it for 10 seconds.

                // To allow the original function to draw the light, we make sure its
                // "once-per-frame" check will pass. We do this by setting the "last drawn frame"
                // variable to something different than the current frame counter.
                *dword_1670CD0 = *dwFrameCount - 1;
            }
            else
            {
                // Another helicopter has the lock and it's not expired.
                // To prevent the original function from drawing, we make its
                // "once-per-frame" check fail by setting the variables to be equal.
                *dword_1670CD0 = *dwFrameCount;
            }
        }
        else
        {
            // If this helicopter's light is off and it owned the lock, release it.
            if (g_pSearchlightHeli == _this)
            {
                g_pSearchlightHeli = nullptr;
            }
        }

        // Finally, call the original function. It will now behave correctly based on
        // how we manipulated the global lock variable.
        shCHelisub_B69D80.unsafe_fastcall(_this, edx);
    }

    static inline int32_t nRadarZoomDelay = 0;
    static inline injector::hook_back<bool(*)()> hbsub_5DCA80;
    static bool sub_5DCA80()
    {
        static int32_t zoomOutEndTime = 0;
        auto currentTime = *CTimer::m_snTimeInMilliseconds;

        // Call the original function to check the actual key state.
        if (hbsub_5DCA80.fun())
        {
            // The key is pressed. Set our timer to end 3 seconds from now.
            zoomOutEndTime = currentTime + nRadarZoomDelay;
            return true; // Return true to zoom out.
        }

        // The key is not pressed. Check if we are within the 3-second delay period.
        if (currentTime < zoomOutEndTime)
        {
            // The timer has not expired yet, so we keep returning true.
            return true;
        }

        // The key is not pressed and the timer has expired.
        return false; // Return false to allow the zoom to return to normal.
    }

    static inline injector::hook_back<decltype(&Natives::CompareString)> hbCOMPARE_STRING;
    static bool __cdecl NATIVE_COMPARE_STRING(const char* a1, const char* a2)
    {
        return a1 && a2 && IsBadReadPtr(a1, 1) == 0 && IsBadReadPtr(a2, 1) == 0 && strcmp(a1, a2) == 0;
    }

    static inline char(__cdecl* GET_NTH_CLOSEST_WATER_NODE_WITH_HEADING)(float x, float y, float z, int flag0, int flag1, Vector4* node, float* heading) = nullptr;

    static inline bool bAnyVisibleNearbyLightOnScreen = false;
    static inline injector::hook_back<int32_t(__fastcall*)(rage::grcViewport*, void*, float, float, float, float, float*)> hbIsSphereVisible;
    static int32_t __fastcall AddLightIsSphereVisible(rage::grcViewport* pViewport, void* edx, float X, float Y, float Z, float radius, float* a6)
    {
        auto ret = hbIsSphereVisible.fun(pViewport, edx, X, Y, Z, radius, a6);

        if (!bAnyVisibleNearbyLightOnScreen)
        {
            constexpr float selectedDistance = 3.5f;
            constexpr float selectedDistSqr = selectedDistance * selectedDistance;
            float dx = pViewport->mCameraMatrix[3][0] - X;
            float dy = pViewport->mCameraMatrix[3][1] - Y;
            float dz = pViewport->mCameraMatrix[3][2] - Z;
            float distSqr = dx * dx + dy * dy + dz * dz;

            if (ret && distSqr <= selectedDistSqr)
            {
                bAnyVisibleNearbyLightOnScreen = true;
            }
        }

        return ret;
    }

    static inline bool bAnyVisibleNearbyWaterOnScreen = false;
    static inline int32_t WaterQuadsCount = 0;
    static int32_t __fastcall DrawWaterIsSphereVisible(rage::grcViewport* pViewport, void* edx, float X, float Y, float Z, float radius, float* a6)
    {
        auto ret = hbIsSphereVisible.fun(pViewport, edx, X, Y, Z, radius, a6);

        if (ret)
        {
            constexpr float selectedDistance = 1100.0f;
            constexpr float selectedDistSqr = selectedDistance * selectedDistance;

            Vector4 out = {};
            float heading = 0.0f;
            float camX = pViewport->mCameraMatrix[3][0];
            float camY = pViewport->mCameraMatrix[3][1];
            float camZ = pViewport->mCameraMatrix[3][2];
            if (GET_NTH_CLOSEST_WATER_NODE_WITH_HEADING(camX, camY, camZ, 1, 0, &out, &heading))
            {
                float dx = camX - out.fX;
                float dy = camY - out.fY;
                float distSqr = dx * dx + dy * dy;

                if (WaterQuadsCount >= 90 && distSqr <= selectedDistSqr)
                    bAnyVisibleNearbyWaterOnScreen = true;
            }
            else if (WaterQuadsCount >= 90)
                bAnyVisibleNearbyWaterOnScreen = true;
        }

        return ret;
    }

    Fixes()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");

            // [MAIN]
            int32_t nAimingZoomFix = iniReader.ReadInteger("MAIN", "AimingZoomFix", 1);
            bool bRecoilFix = iniReader.ReadInteger("MAIN", "RecoilFix", 1) != 0;
            //bool bForceNoMemRestrict = iniReader.ReadInteger("MAIN", "ForceNoMemRestrict", 1) != 0;

            // [MISC]
            bool bDefaultCameraAngleInTLaD = iniReader.ReadInteger("MISC", "DefaultCameraAngleInTLaD", 0) != 0;
            bool bPedDeathAnimFixFromTBoGT = iniReader.ReadInteger("MISC", "PedDeathAnimFixFromTBoGT", 1) != 0;
            bool bDisableCameraCenteringInCover = iniReader.ReadInteger("MISC", "DisableCameraCenteringInCover", 1) != 0;
            bool bAlwaysDisplayHealthOnReticle = iniReader.ReadInteger("MISC", "AlwaysDisplayHealthOnReticle", 1) != 0;
            int nMenuEnteringDelay = std::clamp(iniReader.ReadInteger("MISC", "MenuEnteringDelay", 0), 20, 400);
            int nMenuExitingDelay = std::clamp(iniReader.ReadInteger("MISC", "MenuExitingDelay", 0), 0, 800);
            int nMenuAccessDelayOnStartup = std::clamp(iniReader.ReadInteger("MISC", "MenuAccessDelayOnStartup", 0), 300, 3000);
            nRadarZoomDelay = std::clamp(iniReader.ReadInteger("MISC", "RadarZoomDelay", 3000), 0, 60000);

            //fix for zoom flag in tbogt
            if (nAimingZoomFix)
            {
                auto pattern = find_pattern("8A C1 32 05 ? ? ? ? 24 01 32 C1", "8A D0 32 15");
                static auto& byte_F47AB1 = **pattern.get_first<uint8_t*>(4);
                if (nAimingZoomFix > 1)
                    injector::MakeNOP(pattern.get_first(-2), 2, true);
                else if (nAimingZoomFix < 0)
                    injector::WriteMemory<uint8_t>(pattern.get_first(-2), 0xEB, true);

                pattern = find_pattern("80 8E ? ? ? ? ? EB 43");
                struct AimZoomHook1
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        *(uint8_t*)(regs.esi + 0x200) |= 1;
                        byte_F47AB1 = 1;
                    }
                };
                if (!pattern.empty())
                    injector::MakeInline<AimZoomHook1>(pattern.get_first(0), pattern.get_first(7));
                else {
                    pattern = find_pattern("08 9E ? ? ? ? E9");
                    injector::MakeInline<AimZoomHook1>(pattern.get_first(0), pattern.get_first(6));
                }

                pattern = hook::pattern("76 09 80 A6 ? ? ? ? ? EB 26");
                struct AimZoomHook2
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        *(uint8_t*)(regs.esi + 0x200) &= 0xFE;
                        byte_F47AB1 = 0;
                    }
                };
                if (!pattern.empty())
                    injector::MakeInline<AimZoomHook2>(pattern.get_first(2), pattern.get_first(9));
                else {
                    pattern = find_pattern("80 A6 ? ? ? ? ? EB 25");
                    injector::MakeInline<AimZoomHook2>(pattern.get_first(0), pattern.get_first(7));
                }

                //let's default to 0 as well
                pattern = hook::pattern("C6 05 ? ? ? ? ? 74 12 83 3D");
                if (!pattern.empty())
                    injector::WriteMemory<uint8_t>(pattern.get_first(6), 0, true);
                else {
                    pattern = hook::pattern("88 1D ? ? ? ? 74 10");
                    injector::WriteMemory<uint8_t>(pattern.get_first(1), 0x25, true); //mov ah
                }

                //gamepad handler
                pattern = find_pattern("88 8E ? ? ? ? 84 DB");
                if (!pattern.empty())
                {
                    struct AimZoomHook3
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            *(uint8_t*)(regs.esi + 0x200) = regs.ecx & 0xFF;
                            byte_F47AB1 = *(uint8_t*)(regs.esi + 0x200);
                        }
                    }; injector::MakeInline<AimZoomHook3>(pattern.get_first(0), pattern.get_first(6));
                }
            }

            if (bRecoilFix)
            {
                static float fRecMult = 0.65f;
                auto pattern = find_pattern("F3 0F 10 44 24 ? F3 0F 59 05 ? ? ? ? EB 1E E8 ? ? ? ? 84 C0", "F3 0F 10 44 24 ? F3 0F 59 05 ? ? ? ? EB ? E8");
                injector::WriteMemory(pattern.get_first(10), &fRecMult, true);
            }

            if (bDefaultCameraAngleInTLaD)
            {
                static uint32_t episode_id = 0;
                auto pattern = find_pattern<2>("83 3D ? ? ? ? ? 8B 01 0F 44 C2 89 01 B0 01 C2 08 00", "83 3D ? ? ? ? ? 75 06 C7 00 ? ? ? ? B0 01 C2 08 00");
                injector::WriteMemory(pattern.count(2).get(0).get<void>(2), &episode_id, true);
            }

            if (bPedDeathAnimFixFromTBoGT)
            {
                auto pattern = hook::pattern("8B D9 75 2E");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(2), 2, true);
                else
                {
                    pattern = hook::pattern("BB ? ? ? ? 75 29 80 7F 28 00");
                    injector::MakeNOP(pattern.get_first(5), 2, true);
                }
            }

            {
                // static constexpr float xmm_0 = FLT_MAX / 2.0f;
                // unsigned char bytes[4];
                // auto n = (uint32_t)&xmm_0;
                // bytes[0] = (n >> 24) & 0xFF;
                // bytes[1] = (n >> 16) & 0xFF;
                // bytes[2] = (n >> 8) & 0xFF;
                // bytes[3] = (n >> 0) & 0xFF;
                // 
                // auto pattern = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 58 47 ? F3 0F 11 47 ? 8B D1 89 54 24 10", "F3 0F 10 05 ? ? ? ? F3 0F 58 46 ? 89 8C 24");
                // static raw_mem CoverCB(pattern.get_first(4), { bytes[3], bytes[2], bytes[1], bytes[0] });
                // FusionFixSettings.SetCallback("PREF_COVERCENTERING", [](int32_t value) {
                //     if (value)
                //         CoverCB.Restore();
                //     else
                //         CoverCB.Write();
                // });
                // if (!FusionFixSettings("PREF_COVERCENTERING"))
                //     CoverCB.Write();

                if (bDisableCameraCenteringInCover)
                {
                    static constexpr float xmm_0 = FLT_MAX / 2.0f;
                    auto pattern = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 58 47 ? F3 0F 11 47 ? 8B D1 89 54 24 10", "F3 0F 10 05 ? ? ? ? F3 0F 58 46 ? 89 8C 24");
                    injector::WriteMemory(pattern.get_first(4), &xmm_0, true);
                }
            }

            // reverse lights fix
            {
                auto pattern = find_pattern("8B 40 64 FF D0 F3 0F 10 40 ? 8D 44 24 40 50", "8B 42 64 8B CE FF D0 F3 0F 10 40");
                injector::WriteMemory<uint8_t>(pattern.get_first(2), 0x60, true);
            }

            // animation fix for phone interaction on bikes
            {
                auto pattern = hook::pattern("83 3D ? ? ? ? 01 0F 8C 18 01 00 00");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get(0).get<int>(0), 13, true);
                else {
                    pattern = hook::pattern("80 BE 18 02 00 00 00 0F 85 36 01 00 00 80 BE");
                    injector::MakeNOP(pattern.get(0).get<int>(0x21), 6, true);
                }
            }

            //fix for lods appearing inside normal models, unless the graphics menu was opened once (draw distances aren't set properly?)
            {
                auto pattern = find_pattern("E8 ? ? ? ? 8D 4C 24 10 F3 0F 11 05 ? ? ? ? E8 ? ? ? ? 8B F0 E8 ? ? ? ? DF 2D", "E8 ? ? ? ? 8D 44 24 10 83 C4 04 50");
                auto sub_477300 = injector::GetBranchDestination(pattern.get_first(0));
                pattern = find_pattern("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 83 C4 2C C3", "E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 8B 35 ? ? ? ? E8 ? ? ? ? 25 FF FF 00 00");
                injector::MakeCALL(pattern.get_first(0), sub_477300, true);

                // related to the same issue
                //if (bForceNoMemRestrict)
                //{
                //    pattern = find_pattern("0F 85 ? ? ? ? A1 ? ? ? ? 85 C0 74 5C", "0F 85 ? ? ? ? A1 ? ? ? ? 85 C0 74 5A");
                //    if (!pattern.empty())
                //    {
                //        injector::WriteMemory<uint16_t>(pattern.get_first(0), 0xE990, true); // jnz -> jmp
                //    }
                //    else
                //    {
                //        pattern = find_pattern("75 7E A1 ? ? ? ? 85 C0");
                //        if (!pattern.empty())
                //            injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true); // jnz -> jmp
                //    }
                //}
            }

            // Make LOD lights appear at the appropriate time like on the console version (consoles: 7 PM, pc: 10 PM)
            {
                static uint32_t _dwTimeOffset = 0;
                auto pattern = find_pattern("8B 15 ? ? ? ? 8B 0D ? ? ? ? 0F 45 0D", "8B 0D ? ? ? ? 8D 51 13 3B C2 75 4D E8");
                injector::WriteMemory(pattern.get_first(2), &_dwTimeOffset, true);
                pattern = find_pattern("2B 05 ? ? ? ? 3B C8 75 6C 83 3D", "2B 0D ? ? ? ? 3B C1 75 33 E8");
                injector::WriteMemory(pattern.get_first(2), &_dwTimeOffset, true);
                // Removing episode id check that resulted in flickering LOD lights at certain camera angles in TBOGT
                pattern = hook::pattern("83 3D ? ? ? ? ? 0F 85 ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 10 8C 24");
                if (!pattern.empty())
                    injector::WriteMemory<uint16_t>(pattern.get_first(7), 0xE990, true); // jnz -> jmp
                else {
                    pattern = hook::pattern("83 3D ? ? ? ? ? 75 68 F3 0F 10 05");
                    injector::WriteMemory<uint8_t>(pattern.get_first(7), 0xEB, true); // jnz -> jmp
                }
            }

            {
                auto pattern = find_pattern("F3 0F 11 4C 24 ? 85 DB 74 1B", "F3 0F 11 44 24 ? 74 1B F6 86");
                static auto reg = *pattern.get_first<uint8_t>(5);
                static auto nTimeToWaitBeforeCenteringCameraOnFootKB = FusionFixSettings.GetRef("PREF_KBCAMCENTERDELAY");
                static auto nTimeToWaitBeforeCenteringCameraOnFootPad = FusionFixSettings.GetRef("PREF_PADCAMCENTERDELAY");
                struct OnFootCamCenteringHook 
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        static float f = 0.0f;
                        f = regs.xmm1.f32[0];

                        if (reg == 0x48)
                            f = regs.xmm0.f32[0];

                        float& posX = *(float*)(regs.esp + reg);
                        bool pad = Natives::IsUsingController();
                        int32_t x = 0;
                        int32_t y = 0;

                        if (pad) 
                        {
                            Natives::GetPadState(0, 2, &x);
                            Natives::GetPadState(0, 3, &y);
                        }
                        else
                            Natives::GetMouseInput(&x, &y);

                        if (x || y)
                            nTimeToPassBeforeCenteringCameraOnFoot = *CTimer::m_snTimeInMilliseconds + ((pad ? nTimeToWaitBeforeCenteringCameraOnFootPad->get() : nTimeToWaitBeforeCenteringCameraOnFootKB->get()) * 500);

                        if (pad && !nTimeToWaitBeforeCenteringCameraOnFootPad->get())
                            nTimeToPassBeforeCenteringCameraOnFoot = 0;

                        if (nTimeToPassBeforeCenteringCameraOnFoot < *CTimer::m_snTimeInMilliseconds)
                            posX = f;
                        else
                            posX = 0.0f;
                    }
                };
                
                if (reg != 0x48)
                    injector::MakeInline<OnFootCamCenteringHook>(pattern.get_first(0), pattern.get_first(6));
                else
                {
                    injector::MakeInline<OnFootCamCenteringHook>(pattern.get_first(-2), pattern.get_first(6));
                    injector::WriteMemory<uint16_t>(pattern.get_first(3), 0xDB85, true);
                }
            }

            {
                auto pattern = find_pattern("F3 0F 11 44 24 ? 0F 84 ? ? ? ? 80 3D");
                static auto reg = *pattern.get_first<uint8_t>(5);
                static auto nTimeToWaitBeforeCenteringCameraOnFootKBInCar = FusionFixSettings.GetRef("PREF_KBCAMCENTERDELAYVEH");
                static auto nTimeToWaitBeforeCenteringCameraOnFootPadInCar = FusionFixSettings.GetRef("PREF_PADCAMCENTERDELAYVEH");
                static auto nCameraTurnSpeedKBInCar = FusionFixSettings.GetRef("PREF_KBCAMTURNSPEEDVEH");
                static auto nCameraTurnSpeedPadInCar = FusionFixSettings.GetRef("PREF_PADCAMTURNSPEEDVEH");
                struct InVehicleHorizontalCamCenteringHook
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        bool pad = Natives::IsUsingController();
                        float f = regs.xmm0.f32[0] / (regs.xmm1.f32[0] * (1.0f - (pad ? nCameraTurnSpeedPadInCar->get() : nCameraTurnSpeedKBInCar->get()) * 0.1f));
                        float& posX = *(float*)(regs.esp + reg);
                        int32_t x = 0;
                        int32_t y = 0;

                        if (pad)
                        {
                            Natives::GetPadState(0, 2, &x);
                            Natives::GetPadState(0, 3, &y);
                        }
                        else
                            Natives::GetMouseInput(&x, &y);

                        if (x || y)
                            nTimeToPassBeforeCenteringCameraInVehicle = *CTimer::m_snTimeInMilliseconds + ((pad ? nTimeToWaitBeforeCenteringCameraOnFootPadInCar->get() : nTimeToWaitBeforeCenteringCameraOnFootKBInCar->get()) * 500);

                        if (pad && !nTimeToWaitBeforeCenteringCameraOnFootPadInCar->get())
                            nTimeToPassBeforeCenteringCameraInVehicle = 0;

                        if (nTimeToPassBeforeCenteringCameraInVehicle < *CTimer::m_snTimeInMilliseconds)
                            posX = f;
                        else
                            posX = 0.0f;
                    }
                }; injector::MakeInline<InVehicleHorizontalCamCenteringHook>(pattern.get_first(-4), pattern.get_first(6));

                pattern = find_pattern("F3 0F 11 8C 24 ? ? ? ? 0F 28 CA E8", "F3 0F 11 84 24 ? ? ? ? F3 0F 10 43 ? F3 0F 11 8C 24");
                static auto reg2 = *pattern.get_first<uint8_t>(5);
                struct InVehicleVerticalCamCenteringHook
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        static float f = 0.0f;
                        f = regs.xmm1.f32[0];

                        if (reg2 == 0xBC)
                            f = regs.xmm0.f32[0];

                        float& posX = *(float*)(regs.esp + reg2);
                        bool pad = Natives::IsUsingController();
                        int32_t x = 0;
                        int32_t y = 0;

                        if (pad)
                        {
                            Natives::GetPadState(0, 2, &x);
                            Natives::GetPadState(0, 3, &y);
                        }
                        else
                            Natives::GetMouseInput(&x, &y);

                        if (x || y)
                            nTimeToPassBeforeCenteringCameraInVehicle = *CTimer::m_snTimeInMilliseconds + ((pad ? nTimeToWaitBeforeCenteringCameraOnFootPadInCar->get() : nTimeToWaitBeforeCenteringCameraOnFootKBInCar->get()) * 500);

                        if (pad && !nTimeToWaitBeforeCenteringCameraOnFootPadInCar->get())
                            nTimeToPassBeforeCenteringCameraInVehicle = 0;

                        if (nTimeToPassBeforeCenteringCameraInVehicle < *CTimer::m_snTimeInMilliseconds)
                            posX = f;
                        else
                            posX = 0.0f;
                    }
                }; injector::MakeInline<InVehicleVerticalCamCenteringHook>(pattern.get_first(0), pattern.get_first(9));
            }

            // Disable drive-by while using cellphone
            {
                auto pattern = find_pattern("E8 ? ? ? ? 85 C0 0F 85 ? ? ? ? 84 DB 74 5A 85 F6 0F 84", "E8 ? ? ? ? 85 C0 0F 85 ? ? ? ? 84 DB 74 61");
                bIsPhoneShowing = *find_pattern("C6 05 ? ? ? ? ? E8 ? ? ? ? 6A 00 E8 ? ? ? ? 8B 80", "88 1D ? ? ? ? 88 1D ? ? ? ? E8 ? ? ? ? 6A 00").get_first<bool*>(2);
                hbsub_B2CE30.fun = injector::MakeCALL(pattern.get_first(0), sub_B2CE30, true).get();
            }

            // Custom FOV
            {
                auto pattern = find_pattern("E8 ? ? ? ? F6 87 ? ? ? ? ? 5B", "E8 ? ? ? ? 8B CE E8 ? ? ? ? F6 86 ? ? ? ? ? 5F");
                hbsub_B07600.fun = injector::MakeCALL(pattern.get_first(0), sub_B07600, true).get();

                pattern = find_pattern("E8 ? ? ? ? 84 C0 74 12 80 3D ? ? ? ? ? 0F B6 DB", "E8 ? ? ? ? 84 C0 74 0A 38 1D");
                injector::MakeCALL(pattern.get_first(0), sub_8D0A90, true);

                FusionFixSettings.SetCallback("PREF_CUSTOMFOV", [](int32_t value) {
                    bMenuNeedsUpdate = 2;
                    bMenuNeedsUpdate2 = 2;
                });
            }

            // Fix mouse cursor scale
            {
                auto pattern = hook::pattern("F3 0F 11 44 24 ? E8 ? ? ? ? D9 5C 24 20 80 3D");
                if (!pattern.empty())
                {
                    struct MouseHeightHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            *(float*)(regs.esp + 0x40 - 0x24) = 30.0f * (1.0f / 768.0f);
                        }
                    }; injector::MakeInline<MouseHeightHook>(pattern.get_first(0), pattern.get_first(6));
                }
                else
                {
                    pattern = hook::pattern("F3 0F 11 44 24 ? E8 ? ? ? ? D9 5C 24 1C 80 3D");
                    struct MouseHeightHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            *(float*)(regs.esp + 0x40 - 0x18) = 30.0f * (1.0f / 768.0f);
                        }
                    }; injector::MakeInline<MouseHeightHook>(pattern.get_first(0), pattern.get_first(6));
                }

                pattern = hook::pattern("F3 0F 11 44 24 ? FF 50 24 66 0F 6E C0 0F 5B C0 6A 01");
                if (!pattern.empty())
                {
                    struct MouseWidthHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            *(float*)(regs.esp + 0x10) = 30.0f * (1.0f / 1024.0f);
                        }
                    }; injector::MakeInline<MouseWidthHook>(pattern.get_first(0), pattern.get_first(6));
                }
                else
                {
                    pattern = hook::pattern("F3 0F 11 44 24 ? FF D2 F3 0F 2A C0 F3 0F 59 05 ? ? ? ? 6A 01");
                    struct MouseWidthHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            *(float*)(regs.esp + 0x20) = 30.0f * (1.0f / 1024.0f);
                        }
                    }; injector::MakeInline<MouseWidthHook>(pattern.get_first(0), pattern.get_first(6));
                }
            }

            // Restored a small detail regarding pedprops from the console versions that was changed on PC. Regular cops & fat cops will now spawn with their hat prop disabled when in a vehicle.
            {
                auto pattern = find_pattern("3B 05 ? ? ? ? 74 6C 3B 05 ? ? ? ? 74 64 3B 05 ? ? ? ?", "3B 05 ? ? ? ? 74 6E 3B 05 ? ? ? ? 74 66 3B 05");
                injector::MakeNOP(pattern.get_first(0), 16, true);
            }

            // Remove free cam boundary limits in the video editor.
            {
                auto pattern = hook::pattern("73 5C 56 6A 00 6A 01 E8 ? ? ? ? 83 C4 0C 84 C0 74 4B");
                if (!pattern.empty())
                {
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
             
                    pattern = hook::pattern("0F 86 ? ? ? ? 0F 2E FA 9F F6 C4 44 7A 05");
                    if (!pattern.empty())
                    {
                        injector::WriteMemory<uint16_t>(pattern.get_first(0), 0xE990, true);
                        pattern = hook::pattern("0F 82 ? ? ? ? 80 8F ? ? ? ? ? F6 87");
                        injector::WriteMemory<uint16_t>(pattern.get_first(0), 0xE990, true);
                        pattern = hook::pattern("72 48 0F 2F 44 24 ? 72 41 0F 28 C3");
                        injector::WriteMemory(pattern.get_first(0), 0x12EB, true);
                        pattern = hook::pattern("72 6D 83 3D ? ? ? ? ? 74 1A A1");
                        injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
                    }
                    else
                    {
                        pattern = hook::pattern("0F 86 ? ? ? ? F3 0F 10 54 24 ? 0F 2E D4");
                        injector::WriteMemory<uint16_t>(pattern.get_first(0), 0xE990, true);
                        pattern = hook::pattern("0F 82 ? ? ? ? 80 8E ? ? ? ? ? F6 86");
                        injector::WriteMemory<uint16_t>(pattern.get_first(0), 0xE990, true);
                        pattern = hook::pattern("72 5F 0F 2F C5 72 5A 0F 2F FA 76 0E");
                        injector::WriteMemory(pattern.get_first(0), 0x20EB, true);
                        pattern = hook::pattern("0F 82 ? ? ? ? 83 3D ? ? ? ? ? 74 1A A1");
                        injector::WriteMemory<uint16_t>(pattern.get_first(0), 0xE990, true);
                    }
                }
            }

            // Glass shards color fix
            {
                static auto veh_glass_red   = "veh_glass_red";
                static auto veh_glass_amber = "veh_glass_amber";

                auto pattern = find_pattern("68 ? ? ? ? EB E2 6A 00 68", "68 ? ? ? ? EB 07 6A 00 68 ? ? ? ? E8 ? ? ? ? 83 C4 08");
                if (!pattern.empty())
                    injector::WriteMemory(pattern.get_first(1), &veh_glass_red[0], true);

                pattern = find_pattern("68 ? ? ? ? E8 ? ? ? ? 83 C4 08 89 44 24 0C 6A 00 6A 00", "68 ? ? ? ? E8 ? ? ? ? 83 C4 08 6A 00 6A 00 50 B9 ? ? ? ? 89 44 24 18 E8 ? ? ? ? 8B F0 85 F6 0F 84");
                if (!pattern.empty())
                    injector::WriteMemory(pattern.get_first(1), &veh_glass_amber[0], true);
            }

            // P90 Selector Fix (Prev Weapon key)
            {
                auto pattern = hook::pattern("E8 ? ? ? ? 8B F0 3B 37 75 88");
                if (!pattern.empty())
                    hbsub_B64D60.fun = injector::MakeCALL(pattern.get_first(0), sub_B64D60).get();
            }

            // Disable Z-write for emmissive shaders. Fixes visual bugs e.g. strobe lights in Bahama Mamas (TBoGT) and more.
            {
                static uint32_t* dwEFB1B8 = *hook::pattern("6A 01 6A 10 89 3D").get_first<uint32_t*>(6);
                auto pattern = find_pattern("83 FF 05 74 05 83 FF 04 75 26 6A 00 6A 0C E8 ? ? ? ? 83 C4 08 85 C0 74 0B 6A 01 8B C8 E8", "83 FF 05 74 05");
                static auto loc_AE39F3 = resolve_next_displacement(pattern.get_first(5)).value();
                struct EmissiveDepthWriteHook
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        // Fix for visual bugs in QUB3D that will occur with this fix. Only enable z-write for emissives shaders when the camera/player height is 3000 or higher.
                        // This height check was present in patch 1.0.4.0 and was removed in patch 1.0.6.0+, in addition Z-write for emissive shaders was enabled permanently.
                        const bool isEmissiveShader = regs.edi == 5 || regs.edi == 4;
                        const float cameraHeight = *(float*)(*dwEFB1B8 + 296);

                        if (!_stricmp(pszCurrentCutsceneName, "intro"))
                        {
                            return_to(loc_AE39F3);
                        }

                        if (isEmissiveShader && cameraHeight < 3000.0f)
                        {
                            return;
                        }

                        if (isEmissiveShader && cameraHeight >= 3000.0f)
                        {
                            bIsQUB3D = true;
                        }

                        return_to(loc_AE39F3);
                    }
                }; injector::MakeInline<EmissiveDepthWriteHook>(pattern.get_first(0), pattern.get_first(10));

                pattern = find_pattern("6A 01 8B C8 E8 ? ? ? ? EB 02 33 C0 50 E8 ? ? ? ? 83 C4 04 8B 45 0C 8B 4C 24 18", "6A 01 8B C8 E8 ? ? ? ? EB 02 33 C0 8B C8 E8 ? ? ? ? 8B 4D 0C");
                injector::WriteMemory<uint8_t>(pattern.get_first(1), 0, true);
            }

            // Render LOD lights during cutscenes (console behavior)
            {
                auto pattern = hook::pattern("E8 ? ? ? ? 84 C0 0F 85 ? ? ? ? A1 ? ? ? ? 83 F8 02 0F 84");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(0), 13, true);
                else
                {
                    pattern = hook::pattern("E8 ? ? ? ? 84 C0 75 7A A1 ? ? ? ? 83 F8 02 74 70 83 F8 03");
                    injector::MakeNOP(pattern.get_first(0), 9, true);
                }
            }

            // Bike standing still feet fix
            {
                auto pattern = hook::pattern("83 3D ? ? ? ? ? 0F 85 ? ? ? ? 68 ? ? ? ? 68");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(7), 6, true);
                else
                {
                    pattern = hook::pattern("39 05 ? ? ? ? 0F 85 ? ? ? ? 68");
                    injector::MakeNOP(pattern.get_first(6), 6, true);
                }
            }

            // Skybox Black Bottom Fix -- causes holes in the map to be more visible.
            // {
            //     auto PatchVertices = [](float* ptr)
            //     {
            //         for (auto i = 0; i < ((6156 / 4) / 3); i++)
            //         {
            //             auto pVertex = (ptr + i * 3);
            //             if (i < 32)
            //             {
            //                 if (pVertex[1] < 0.0f)
            //                 {
            //                     injector::WriteMemory<float>(pVertex, 0.0f, true);
            //                     injector::WriteMemory<float>(pVertex + 2, 0.0f, true);
            //                 }
            //             }
            //             else
            //             {
            //                 injector::WriteMemory<float>(pVertex + 1, (pVertex[1] - 0.16037700f) * 1.78f - 1.0f, true);
            //             }
            //         }
            //     };

            //     auto pattern = hook::pattern("C7 44 24 ? ? ? ? ? C7 44 24 ? ? ? ? ? E8 ? ? ? ? 8B 44 24 44");
            //     if (!pattern.empty())
            //     {
            //         auto addr = pattern.get_first<float*>(4);
            //         PatchVertices(*addr);
            //     }
            //     else
            //     {
            //         pattern = hook::pattern("C7 46 ? ? ? ? ? C7 46 ? ? ? ? ? 5E 59 C3");
            //         if (!pattern.empty())
            //         {
            //             auto addr = pattern.get_first<float*>(3);
            //             PatchVertices(*addr);
            //         }
            //     }
            // }

            // HACK: Visually hide the mouse cursor when using a gamepad, doesn't actually disable the cursor so it might still interact with UI, also still shows in the start menu because...??
            {
                auto pattern = hook::pattern("75 1B 83 3D ? ? ? ? ? 75 12 6A 00 E8");
                if (!pattern.empty())
                {
                    auto ptr = (uintptr_t)hook::get_pattern("C6 05 ? ? ? ? ? 5F 5E 5D 5B 83 C4 2C C3", 7) - (uintptr_t)pattern.get_first(6);
                    injector::WriteMemory<uint16_t>(pattern.get_first(0), 0x840F, true); // jnz short -> jz long
                    injector::WriteMemory(pattern.get_first(2), ptr, true);
                    injector::MakeNOP(pattern.get_first(6), 23, true);
                }
            }

            // Always display the ped health on the reticle with free-aim while on foot, used to be a gamepad + multiplayer only feature (PC is always free-aim unless it's melee combat).
            if (bAlwaysDisplayHealthOnReticle)
            {
                auto pattern = hook::pattern("80 BB ? ? ? ? ? 74 61 56 57 E8");
                if (!pattern.empty())
                {
                    static auto loc_5C8E93 = resolve_next_displacement(pattern.get_first(0)).value();
                    struct ReticleHealthHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            if (!(*(uint8_t*)(regs.ebx + 12941)) || !(*(uint8_t*)(regs.ebx + 12940)))
                                force_return_address(loc_5C8E93);
                        }
                    }; injector::MakeInline<ReticleHealthHook>(pattern.get_first(0), pattern.get_first(9));

                    pattern = hook::pattern("75 0C 38 83 ? ? ? ? 0F 84");
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
                }
                else
                {
                    pattern = hook::pattern("80 B9 ? ? ? ? ? 74 6D 56 57 E8");
                    static auto loc_5C8E93 = resolve_next_displacement(pattern.get_first(0)).value();
                    struct ReticleHealthHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            if (!(*(uint8_t*)(regs.ecx + 12941)) || !(*(uint8_t*)(regs.ecx + 12940)))
                                force_return_address(loc_5C8E93);
                        }
                    }; injector::MakeInline<ReticleHealthHook>(pattern.get_first(0), pattern.get_first(9));

                    pattern = hook::pattern("75 0C 38 86 ? ? ? ? 0F 84");
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
                }
            }

            // Radio reset fix
            {
                auto pattern = find_pattern("74 ? 85 C9 75 ? 32 C0 50", "74 16 85 C0 75 12 D9 EE");
                if (!pattern.empty())
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true); // jz -> jmp
            }

            // Radar zoom (T hotkey) 30fps cap fix
            {
                auto pattern = find_pattern("83 F9 ? 0F 86 ? ? ? ? F3 0F 10 15", "83 F8 1E 0F 86 ? ? ? ? F3 0F 10 0D ? ? ? ? 0F 2E C1");
                if (!pattern.empty())
                    injector::WriteMemory<uint8_t>(pattern.get_first(2), 15, true);
            }

            // Radar zoom stays for a bit
            {
                auto pattern = find_pattern("E8 ? ? ? ? 84 C0 74 ? F3 0F 10 05 ? ? ? ? F3 0F 11 44 24 ? 6A");
                if (!pattern.empty())
                    hbsub_5DCA80.fun = injector::MakeCALL(pattern.get_first(0), sub_5DCA80, true).get();
            }

            // Workaround for drunk cam lights issue
            {
                auto pattern = find_pattern("E8 ? ? ? ? 6A ? FF 74 24 ? FF 74 24 ? 6A", "E8 ? ? ? ? 83 FF ? 6A");
                if (!pattern.empty())
                    hbsub_AD1240.fun = injector::MakeCALL(pattern.get_first(0), sub_AD1240, true).get();
            }

            // Helicopter lights fix
            {
                auto pattern = find_pattern("8B 0D ? ? ? ? 84 C0 8B 47", "8B 15 ? ? ? ? 3B 15 ? ? ? ? 0F 84 ? ? ? ? E8");
                dword_1670CD0 = *pattern.get_first<uint32_t*>(2);

                pattern = find_pattern("3B 05 ? ? ? ? 0F 84 ? ? ? ? E8 ? ? ? ? F3 0F 10 87", "3B 15 ? ? ? ? 0F 84 ? ? ? ? E8");
                dwFrameCount = *pattern.get_first<uint32_t*>(2);

                pattern = find_pattern("55 8B EC 83 E4 ? 81 EC ? ? ? ? 57 8B F9", "55 8B EC 83 E4 ? 81 EC ? ? ? ? 56 57 8B F1 E8 ? ? ? ? 8B 46");
                shCHelisub_B69D80 = safetyhook::create_inline(pattern.get_first(), CHelisub_B69D80);
            }

            // Menu input lag
            {
                auto pattern = hook::pattern("EB 05 68 ? ? ? ? E8 ? ? ? ? 6A 08");
                if (!pattern.empty())
                    injector::WriteMemory(pattern.get_first(3), nMenuEnteringDelay, true);

                pattern = hook::pattern("68 ? ? ? ? EB ? 6A ? 68 ? ? ? ? 6A");
                if (!pattern.empty())
                    injector::WriteMemory(pattern.get_first(1), nMenuExitingDelay, true);

                pattern = hook::pattern("81 F9 ? ? ? ? 72 ? EB ? E8");
                if (!pattern.empty())
                    injector::WriteMemory(pattern.get_first(2), nMenuAccessDelayOnStartup, true);
            }

            // Bullet traces
            {
                auto pattern = find_pattern("0F 2F C8 72 ? FF 76");
                if (!pattern.empty())
                {
                    static auto loc_B5D8D8 = resolve_next_displacement(pattern.get_first(0)).value();
                    injector::MakeNOP(pattern.get_first(0), 5);
                    static auto BulletTracesHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        static auto esc = FusionFixSettings.GetRef("PREF_BULLETTRACES");
                        if (esc->get())
                        {
                            return;
                        }

                        if (regs.xmm0.f32[0] >= regs.xmm1.f32[0])
                            return_to(loc_B5D8D8);
                    });
                }
                else
                {
                    pattern = find_pattern("72 ? 8B 56 ? 52");
                    static auto loc_B5D8D8 = resolve_displacement(pattern.get_first(0)).value();
                    injector::MakeNOP(pattern.get_first(0), 5);
                    static auto BulletTracesHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs) {
                        static auto esc = FusionFixSettings.GetRef("PREF_BULLETTRACES");
                        if (esc->get())
                        {
                            regs.edx = *(uint32_t*)(regs.esi + 0x18);
                            return;
                        }

                        //fcomip  st, st(1)
                        constexpr uint32_t FLAG_CF = 1u << 0;
                        if (regs.eflags & FLAG_CF)
                            return_to(loc_B5D8D8);

                        regs.edx = *(uint32_t*)(regs.esi + 0x18);
                    });
                }

                // Force IV/TLAD bullet tracer particles, TBoGT tracer particles have weird/wrong positioning
                pattern = find_pattern("75 ? 68 ? ? ? ? EB ? 68 ? ? ? ? E8 ? ? ? ? 83 C4 ? 8B F8", "75 07 68 ? ? ? ? EB 05 68 ? ? ? ? E8 ? ? ? ? 83 C4 08 6A 00 6A 00 8B F8 57");
                if (!pattern.empty())
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true); // jnz -> jmp
            }

            // Enable the "first person" reticle (Annihilator, Buzzard) on gamepads as well, this used to be a keyboard & mouse only feature.
            {
                auto pattern = find_pattern("0F 84 ? ? ? ? 85 C9 0F 84 ? ? ? ? 8B 89", "0F 84 ? ? ? ? 85 C0 0F 84 ? ? ? ? 8B 88");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(0), 6, true);
            }

            // TLAD phone calls crashing (https://github.com/GTAmodding/GTAIV-Issues-List/issues/232)
            {
                hbCOMPARE_STRING.fun = NativeOverride::Register(Natives::NativeHashes::COMPARE_STRING, NATIVE_COMPARE_STRING, "E8 ? ? ? ? ? ? ? ? ? 83 C4 ? 89 ? 5E C3", 30);
            }

            // TLAD Marta Full of Grace's crash (https://github.com/GTAmodding/GTAIV-Issues-List/issues/235)
            {
                auto pattern = find_pattern("F6 80 ? ? ? ? ? 74 ? 8B 80 ? ? ? ? 56");
                if (!pattern.empty())
                {
                    static auto loc_BA91F2 = resolve_next_displacement(pattern.get_first(0)).value();
                    injector::MakeNOP(pattern.get_first(0), 9);
                    static auto IS_CHAR_SITTING_IN_ANY_CAR_HOOK = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        if (!regs.eax || (*(uint8_t*)(regs.eax + 0x26C) & 4) == 0)
                            return_to(loc_BA91F2);
                    });
                }
            }

            // Water flicker mitigation experiment
            {
                //auto pattern = find_pattern("83 EC ? F3 0F 10 4D ? F3 0F 10 05 ? ? ? ? 0F 2F C1 F3 0F 10 55 ? F3 0F 10 5D ? F3 0F 11 54 24 ? F3 0F 11 5C 24 ? F3 0F 11 4C 24 ? 72 ? 6A ? 83 EC ? F3 0F 11 5C 24 ? F3 0F 11 14 24 E8 ? ? ? ? D9 5C 24 ? 83 C4 ? 8B 45", "83 EC ? F3 0F 10 45 ? F3 0F 10 0D ? ? ? ? F3 0F 11 44 24 ? F3 0F 10 45 ? F3 0F 11 44 24 ? F3 0F 10 45 ? 0F 2F C8 F3 0F 11 44 24 ? 72 ? D9 45 ? 6A ? 83 EC ? D9 5C 24 ? D9 45 ? D9 1C 24 E8 ? ? ? ? D9 5C 24 ? 83 C4 ? D9 EE");
                //GET_NTH_CLOSEST_WATER_NODE_WITH_HEADING = (decltype(GET_NTH_CLOSEST_WATER_NODE_WITH_HEADING))pattern.get_first(-6);
                //
                //pattern = find_pattern("E8 ? ? ? ? 85 C0 0F 84 ? ? ? ? 80 7D ? ? C6 44 24 ? ? 74 ? FF 77", "E8 ? ? ? ? 85 C0 0F 84 ? ? ? ? 80 7D ? ? C6 44 24 ? ? 74 ? 8B 4E");
                //hbIsSphereVisible.fun = injector::MakeCALL(pattern.get_first(0), AddLightIsSphereVisible, true).get();
                //
                //pattern = find_pattern("E8 ? ? ? ? 85 C0 0F 84 ? ? ? ? 8B 4C 24 ? 3B F9", "E8 ? ? ? ? 85 C0 0F 84 ? ? ? ? 3B F7");
                //hbIsSphereVisible.fun = injector::MakeCALL(pattern.get_first(0), DrawWaterIsSphereVisible, true).get();
                //
                //pattern = find_pattern("E8 ? ? ? ? 8B D0 8B B4 BA", "E8 ? ? ? ? 8B BC B0 ? ? ? ? 8B 94 B0");
                //static auto RenderWaterHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                //{
                //    bAnyVisibleNearbyWaterOnScreen = false;
                //    WaterQuadsCount = 0;
                //});
                //
                //pattern = find_pattern("0F B7 0C 72 C1 E1 ? 0F BF 81", "0F B7 04 78 C1 E0 04 0F BF 88 ? ? ? ? 8B 14 CD ? ? ? ? 0F BF 88 ? ? ? ? 03 C9");
                //static auto RenderWaterCounterHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                //{
                //    WaterQuadsCount++;
                //});
                //
                ////pattern = find_pattern("F3 0F 11 44 24 ? FF 74 24 ? F3 0F 11 44 24", "F3 0F 11 44 24 ? 8B 54 24 ? 52 F3 0F 11 44 24");
                ////static auto RenderWaterCounterHook2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                ////{
                ////    WaterQuadsCount++;
                ////});
                //
                //pattern = find_pattern("8B 35 ? ? ? ? F3 0F 11 44 24 ? F3 0F 10 80", "8B 1D ? ? ? ? F3 0F 10 83");
                //static auto RenderLightsHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                //{
                //    bAnyVisibleNearbyLightOnScreen = false;
                //});

                auto pattern = hook::pattern("A8 ? 0F 84 ? ? ? ? 8B C8");
                static auto loc_927DE0 = resolve_next_displacement(pattern.get_first(0)).value();
                injector::MakeNOP(pattern.get_first(2), 6);
                static auto LightCounterHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    static auto ExtraNightShadows = FusionFixSettings.GetRef("PREF_EXTRANIGHTSHADOWS");
                    if (ExtraNightShadows->get())
                    {
                        if ((regs.eax & 6) != 0)
                        {
                            return;

                            //if (Natives::IsInteriorScene())
                            //{
                            //    return; // Flicker - Always in interiors
                            //}
                            //
                            //if (!bAnyVisibleNearbyWaterOnScreen)
                            //{
                            //    return; // Flicker - No water on screen
                            //}
                            //else if (bAnyVisibleNearbyLightOnScreen)
                            //{
                            //    return; // Flicker - Water and lights
                            //}
                        }
                    }
                    else
                    {
                        if ((regs.eax & 6) != 0 && Natives::IsInteriorScene())
                        {
                            return; // Flicker
                        }
                    }

                    return_to(loc_927DE0);
                });
            }
        };
    }
} Fixes;