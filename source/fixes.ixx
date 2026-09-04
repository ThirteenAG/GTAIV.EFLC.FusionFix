module;

#include <common.hxx>

export module fixes;

import common;
import comvars;
import natives;
import settings;
import shaders;

int nRadarZoomDelay = 0;

namespace CTaskComplexGangDriveby
{
    SafetyHookInline shPlayerWantsToDoDriveby = {};
    char __cdecl PlayerWantsToDoDriveby(int a1)
    {
        if (CPhoneMgr::bDisplayMobile && *CPhoneMgr::bDisplayMobile)
            return 0;

        return shPlayerWantsToDoDriveby.unsafe_ccall<char>(a1);
    }
}

namespace CCamGame
{
    SafetyHookInline shPostProcessHeightBasedFOV = {};
    void __fastcall PostProcessHeightBasedFOV(int _this, void* edx)
    {
        shPostProcessHeightBasedFOV.unsafe_fastcall<void>(_this, edx);

        static auto FieldOfView = FusionFixSettings.GetRef("PREF_CUSTOMFOV");

        *(float*)(_this + 0x60) += FieldOfView->get() * 5.0f;
    }
}

namespace CPedWeaponMgr
{
    injector::hook_back<int(__fastcall*)(char* _this, void* edx, int a2)> hbGetNextActiveSlot;
    int __fastcall GetNextActiveSlot(char* _this, void* edx, int a2)
    {
        auto ret = hbGetNextActiveSlot.fun(_this, edx, a2);

        if (ret == 4 && _this[21] == 32) // P90
            ret = hbGetNextActiveSlot.fun(_this, edx, ret);

        return ret;
    }
}

namespace CDeferredLightingHelper
{
    injector::hook_back<void(*)()> hbBeginMotionBlurMask;
    void BeginMotionBlurMask()
    {
        Cam rootCam = 0;
        Natives::GetRootCam(&rootCam);
        auto CCam = CCam::GetCamPool()->GetAt(rootCam);

        if (CCam)
        {
            auto ptr = *(uintptr_t*)((uintptr_t)CCam + 0x110);

            if (ptr)
            {
                if (*(uint8_t*)(ptr + 0x27C) == 1)
                    return;
            }
        }

        return hbBeginMotionBlurMask.fun();
    }
}

SafetyHookInline shsub_5ADB20 = {};
int sub_5ADB20()
{
    if (Natives::IsUsingController())
        return 0;

    return shsub_5ADB20.unsafe_ccall<int>();
}

namespace CRadarNY
{
    injector::hook_back<bool(*)()> hbsub_5DCA80;
    bool sub_5DCA80()
    {
        static int ZoomOutEndTime = 0;
        int CurrentTime = *CTimer::m_snTimeInMilliseconds;

        // Call the original function to check the zoom key state
        if (hbsub_5DCA80.fun())
        {
            // The zoom key is pressed, start the timer
            ZoomOutEndTime = CurrentTime + nRadarZoomDelay;

            // Zoom out
            return true;
        }

        // After e.g. loading a save, ZoomOutEndTime would become garbage.
        // If the remaining time is larger than the configured delay, force a reset, so that the radar doesn't stay zoomed out forever.
        if (ZoomOutEndTime - CurrentTime >= nRadarZoomDelay)
        {
            ZoomOutEndTime = 0;

            // Zoom in
            return false;
        }

        // The zoom key is not pressed and the timer has not expired, keep zooming out
        if (CurrentTime < ZoomOutEndTime)
        {
            // Zoom out
            return true;
        }

        // Timer has expired, zoom in
        return false;
    }
}

namespace CHeli
{
    static inline uint32_t* dword_1670CD0 = nullptr; // Light's last drawn frame
    static inline void* pActiveSearchlight = nullptr; // Pointer to the helicopter that draws the light
    static inline int SearchlightLockTime = 0; // Timestamp of when the lock expires

    SafetyHookInline shPreRender2 = {};
    void __fastcall PreRender2(void* _this, void* edx)
    {
        // Check if a searchlight is active
        if (*(int8_t*)((uintptr_t)_this + 8044) != 0 && *(float*)((uintptr_t)_this + 8036) > 0.0f && *(int8_t*)((uintptr_t)_this + 8240) == 0)
        {
            auto CurrentTime = *CTimer::m_snTimeInMilliseconds;

            // A helicopter can acquire the lock if:
            // - No one has it (pActiveSearchlight is nullptr)
            // - This helicopter already has it
            // - The lock from another helicopter has expired
            if (pActiveSearchlight == nullptr || pActiveSearchlight == _this || CurrentTime > SearchlightLockTime)
            {
                // This helicopter gets the lock.
                pActiveSearchlight = _this;

                // Lock for maximum 10 seconds
                SearchlightLockTime = CurrentTime + 10000;

                // To allow the original function to draw the light, we make sure its "once-per-frame" check will pass.
                // We do this by setting the "last drawn frame" dword to something different than the current frame counter.
                *dword_1670CD0 = *CTimer::m_frameCount - 1;
            }
            else
            {
                // Another helicopter has the lock and it's not expired.
                // To prevent the original function from drawing, we make its "once-per-frame" check fail by setting the variables to be equal.
                *dword_1670CD0 = *CTimer::m_frameCount;
            }
        }
        else
        {
            // If this helicopter's light is off and it owned the lock, release it
            if (pActiveSearchlight == _this)
            {
                pActiveSearchlight = nullptr;
            }
        }

        shPreRender2.unsafe_fastcall(_this, edx);
    }
}

injector::hook_back<decltype(&Natives::CompareString)> hbCOMPARE_STRING;
bool __cdecl NATIVE_COMPARE_STRING_1(const char* a1, const char* a2)
{
    return a1 && a2 && IsBadReadPtr(a1, 1) == 0 && IsBadReadPtr(a2, 1) == 0 && strcmp(a1, a2) == 0;
}

SafetyHookInline shNATIVE_COMPARE_STRING = {};
bool __cdecl NATIVE_COMPARE_STRING_2(const char* a1, const char* a2)
{
    return a1 && a2 && IsBadReadPtr(a1, 1) == 0 && IsBadReadPtr(a2, 1) == 0 && strcmp(a1, a2) == 0;
}

class Fixes
{
public:
    /*static inline char(__cdecl* GET_NTH_CLOSEST_WATER_NODE_WITH_HEADING)(float x, float y, float z, int flag0, int flag1, Vector4* node, float* heading) = nullptr;

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
    }*/

    Fixes()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");

            // [MAIN]
            int nAimingZoomFix = iniReader.ReadInteger("MAIN", "AimingZoomFix", 1);
            bool bRecoilFix = iniReader.ReadInteger("MAIN", "RecoilFix", 1) != 0;

            // [MISC]
            bool bDefaultCameraAngleInTLaD = iniReader.ReadInteger("MISC", "DefaultCameraAngleInTLaD", 1) != 0;
            bool bPedDeathAnimFixFromTBoGT = iniReader.ReadInteger("MISC", "PedDeathAnimFixFromTBoGT", 1) != 0;
            bool bDisableCameraCenteringInCover = iniReader.ReadInteger("MISC", "DisableCameraCenteringInCover", 1) != 0;
            bool bAlwaysDisplayHealthOnReticle = iniReader.ReadInteger("MISC", "AlwaysDisplayHealthOnReticle", 1) != 0;
            int bFixHelicopterSearchlights = iniReader.ReadInteger("MISC", "FixHelicopterSearchlights", 1);

            int nMenuEnteringDelay = std::clamp(iniReader.ReadInteger("MISC", "MenuEnteringDelay", 0), 20, 400);
            int nMenuExitingDelay = std::clamp(iniReader.ReadInteger("MISC", "MenuExitingDelay", 0), 0, 800);
            int nMenuAccessDelayOnStartup = std::clamp(iniReader.ReadInteger("MISC", "MenuAccessDelayOnStartup", 0), 300, 3000);
            nRadarZoomDelay = std::clamp(iniReader.ReadInteger("MISC", "RadarZoomDelay", 3000), 0, 60000);

            // Fix weapon zoom states in TBoGT
            if (nAimingZoomFix)
            {
                auto pattern = find_pattern("75 ? 8A C1 32 05", "75 ? 8A D0 32 15");
                static auto byte_103C111 = *pattern.get_first<uint8_t*>(6);

                // If 2 or higher, remove the episode ID check and allow the fixed feature to work in other episodes
                if (nAimingZoomFix > 1)
                {
                    injector::MakeNOP(pattern.get_first(0), 2, true);
                }
                // If -1 or lower, disable the feature in TBoGT
                else if (nAimingZoomFix < 0)
                {
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true); // jnz --> jmp
                }

                // Default the weapon zoom state to 0, used to be always forced to 1 (In TBoGT only)
                pattern = hook::pattern("C6 05 ? ? ? ? ? 74 ? 83 3D ? ? ? ? ? 75 ? 80 3D");
                if (!pattern.empty())
                {
                    injector::WriteMemory<uint8_t>(pattern.get_first(6), 0, true);
                }
                else
                {
                    pattern = hook::pattern("88 1D ? ? ? ? 74 ? 39 1D");
                    injector::WriteMemory<uint8_t>(pattern.get_first(1), 0x25, true);
                }

                pattern = hook::pattern("80 8E ? ? ? ? ? EB ? 84 D2 74 ? 8A 81");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 7, true);
                    static auto AimZoomHook1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        *(uint8_t*)(regs.esi + 0x200) |= 1;
                        *byte_103C111 = 1;
                    });
                }
                else
                {
                    pattern = hook::pattern("08 9E ? ? ? ? E9");
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto AimZoomHook1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        *(uint8_t*)(regs.esi + 0x200) |= 1;
                        *byte_103C111 = 1;
                    });
                }

                pattern = hook::pattern("80 A6 ? ? ? ? ? EB ? 81 C1");
                injector::MakeNOP(pattern.get_first(0), 7, true);
                static auto AimZoomHook2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    *(uint8_t*)(regs.esi + 0x200) &= 0xFE;
                    *byte_103C111 = 0;
                });

                pattern = find_pattern("88 8E ? ? ? ? 84 DB", "88 8E ? ? ? ? 80 7C 24");
                injector::MakeNOP(pattern.get_first(0), 6, true);
                static auto AimZoomHook3 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    *(uint8_t*)(regs.esi + 0x200) = regs.ecx & 0xFF;
                    *byte_103C111 = *(uint8_t*)(regs.esi + 0x200);
                });
            }

            // Fix nerfed weapon recoil on mouse
            // NOTE: This still isn't enough to bring it to par with the gamepad path it seems, Xbox code also seems entirely different.
            if (bRecoilFix)
            {
                auto pattern = hook::pattern("F3 0F 59 05 ? ? ? ? EB ? E8 ? ? ? ? 84 C0");
                injector::MakeNOP(pattern.get_first(0), 8, true);
                static auto CWeapon__DoAccuracy_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    regs.xmm0.f32[0] *= 0.65f;
                });
            }

            // Disable the different camera offset on bikes in TLAD
            if (bDefaultCameraAngleInTLaD)
            {
                auto pattern = hook::pattern("0F 44 C2 89 01 B0 ? C2 ? ? 8B 4C 24");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 5, true);
                }
                else
                {
                    pattern = hook::pattern("C7 00 ? ? ? ? B0 ? C2 ? ? 8B 44 24 ? C7 00 ? ? ? ? 83 3D");
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                }
            }

            // Disable the legacy 3D-era death animation played after an enemy falls and dies from a melee counter attack (TBoGT exclusive fix enabled for all episodes)
            if (bPedDeathAnimFixFromTBoGT)
            {
                auto pattern = hook::pattern("75 ? 80 7B ? ? 75 ? 51");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 2, true); // jnz --> nop
                }
                else
                {
                    pattern = hook::pattern("75 ? 80 7F ? ? 75 ? D9 EE");
                    injector::MakeNOP(pattern.get_first(0), 2, true); // jnz --> nop
                }
            }

            // Disable the forced camera auto centering when getting to corners in cover
            if (bDisableCameraCenteringInCover)
            {
                auto pattern = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 58 47 ? F3 0F 11 47 ? 8B D1", "F3 0F 10 05 ? ? ? ? F3 0F 58 46 ? 89 8C 24");
                injector::MakeNOP(pattern.get_first(0), 8, true);
                static auto CTaskComplexNewUseCover__UpdateCoverStatus_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    // This operates on or replaces CTimer::fTimeStep. Is this safe? At least for other related aspects; It works just fine to disable the centering for instance.
                    regs.xmm0.f32[0] = FLT_MAX / 2.0f;
                });
            }

            // Fix centered/twin reversing lights being attached to the headlights bone instead of the taillights bone
            {
                auto pattern = find_pattern("8B 40 ? FF D0 F3 0F 10 40 ? 8D 44 24 ? 50 FF 74 24", "8B 42 ? 8B CE FF D0 F3 0F 10 40 ? 8D 4C 24");
                injector::WriteMemory<uint8_t>(pattern.get_first(2), 0x60, true);
            }

            // Fix for the player not putting a foot down on bikes when stationary while talking on cellphone (TLAD/TBoGT fix enabled for IV)
            {
                auto pattern = find_pattern("83 3D ? ? ? ? ? 0F 8C ? ? ? ? F6 87", "83 3D ? ? ? ? ? 0F 8C ? ? ? ? F6 86");
                injector::MakeNOP(pattern.get_first(0), 13, true);
            }

            // Fix for the player not using feet for reversing on bikes (TLAD exclusive fix enabled for all episodes)
            {
                auto pattern = hook::pattern("0F 85 ? ? ? ? 68 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 83 C4");
                injector::MakeNOP(pattern.get_first(0), 6, true); // jnz --> nop
            }

            // Fix draw distance sliders not using the actual set values in the Graphics menu unless its opened at least once after game boot
            {
                auto pattern = find_pattern("E8 ? ? ? ? 8D 4C 24 ? F3 0F 11 05", "E8 ? ? ? ? 8D 44 24 ? 83 C4 ? 50 F3 0F 11 05");
                auto sub_59F200 = injector::GetBranchDestination(pattern.get_first(0));

                // The fix should consist of initializing the graphics menu settings in CGame::Init in the place of some random nullsub which does nothing.
                // Thus, somehow it avoids the issue.
                pattern = find_pattern("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 83 C4", "E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 8B 35 ? ? ? ? E8 ? ? ? ? 25");
                injector::MakeCALL(pattern.get_first(0), sub_59F200, true);
            }

            // Make LOD lights/vehicle impostors appear at the appropriate time like on the console versions (Consoles: 7 PM, PC: 10 PM)
            {
                static uint32_t TimeOffset = 0;

                auto pattern = find_pattern("8B 15 ? ? ? ? 8B 0D ? ? ? ? 0F 45 0D", "8B 0D ? ? ? ? 8D 51 ? 3B C2");
                injector::WriteMemory(pattern.get_first(2), &TimeOffset, true);

                pattern = find_pattern("2B 05 ? ? ? ? 3B C8 75", "2B 0D ? ? ? ? 3B C1");
                injector::WriteMemory(pattern.get_first(2), &TimeOffset, true);

                // Remove the episode ID check that resulted in flickering LOD lights at certain camera angles in TBoGT
                pattern = hook::pattern("0F 85 ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 10 8C 24");
                if (!pattern.empty())
                {
                    injector::WriteMemory<uint16_t>(pattern.get_first(0), 0xE990, true); // jnz --> jmp
                }
                else
                {
                    pattern = hook::pattern("75 ? F3 0F 10 05 ? ? ? ? F3 0F 10 4C 24 ? F3 0F 59 C8");
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true); // jnz --> jmp
                }
            }

            // Enable LOD lights/vehicle impostors during cutscenes, like on the console versions
            {
                auto pattern = hook::pattern("E8 ? ? ? ? 84 C0 0F 85 ? ? ? ? A1 ? ? ? ? 83 F8 ? 0F 84");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 13, true);
                }
                else
                {
                    pattern = hook::pattern("E8 ? ? ? ? 84 C0 75 ? A1 ? ? ? ? 83 F8 ? 74 ? 83 F8 ? 74 ? 83 F8");
                    injector::MakeNOP(pattern.get_first(0), 9, true);
                }
            }

            // Camera centering delay/turn speed
            {
                // Timers
                static int nTimeToPassBeforeCenteringCameraFollowPed = 0;
                static int nTimeToPassBeforeCenteringCameraFollowVehicle = 0;

                // Settings
                static auto nTimeToWaitBeforeCenteringCameraFollowPed_KB = FusionFixSettings.GetRef("PREF_KBCAMCENTERDELAY");
                static auto nTimeToWaitBeforeCenteringCameraFollowPed_Pad = FusionFixSettings.GetRef("PREF_PADCAMCENTERDELAY");

                static auto nTimeToWaitBeforeCenteringCameraFollowVehicle_KB = FusionFixSettings.GetRef("PREF_KBCAMCENTERDELAYVEH");
                static auto nTimeToWaitBeforeCenteringCameraFollowVehicle_Pad = FusionFixSettings.GetRef("PREF_PADCAMCENTERDELAYVEH");

                static auto nCameraTurnSpeedFollowVehicle_KB = FusionFixSettings.GetRef("PREF_KBCAMTURNSPEEDVEH");
                static auto nCameraTurnSpeedFollowVehicle_Pad = FusionFixSettings.GetRef("PREF_PADCAMTURNSPEEDVEH");

                static auto ShouldCenter = [&](int& DelayTime, int DelaySetting, bool IsUsingPad) -> bool
                {
                    static int LastTime = 0;
                    int CurrentTime = *CTimer::m_snTimeInMilliseconds;

                    if (CurrentTime < LastTime)
                        DelayTime = 0;

                    LastTime = CurrentTime;

                    int32_t X = 0, Y = 0;

                    if (IsUsingPad)
                    {
                        Natives::GetPadState(0, 2, &X);
                        Natives::GetPadState(0, 3, &Y);
                    }
                    else
                        Natives::GetMouseInput(&X, &Y);

                    if (X || Y)
                    {
                        DelayTime = CurrentTime + (DelaySetting * 500);
                    }

                    if (IsUsingPad && DelaySetting == 0)
                        DelayTime = 0;

                    return DelayTime < CurrentTime;
                };

                // CCamFollowPed
                {
                    auto pattern = find_pattern("F3 0F 11 4C 24 ? 85 DB 74", "F3 0F 11 44 24 ? 74 ? F6 86 ? ? ? ? ? 74");
                    static auto reg = *pattern.get_first<uint8_t>(5);
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto CCamFollowPed__Process_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        bool IsUsingPad = Natives::IsUsingController();
                        int DelaySetting = IsUsingPad ? nTimeToWaitBeforeCenteringCameraFollowPed_Pad->get() : nTimeToWaitBeforeCenteringCameraFollowPed_KB->get();

                        float f = (reg != 0x48) ? regs.xmm1.f32[0] : regs.xmm0.f32[0];
                        float& AutoCenterValue = *(float*)(regs.esp + reg);

                        if (ShouldCenter(nTimeToPassBeforeCenteringCameraFollowPed, DelaySetting, IsUsingPad))
                            AutoCenterValue = f;
                        else
                            AutoCenterValue = 0.0f;
                    });
                }

                // CCamFollowVehicle X axis
                {
                    auto pattern = hook::pattern("F3 0F 5E C1 F3 0F 11 44 24 ? 0F 84");
                    static auto reg = *pattern.get_first<uint8_t>(9);
                    injector::MakeNOP(pattern.get_first(0), 10, true);
                    static auto CCamFollowVehicle__Process_Hook1 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        bool IsUsingPad = Natives::IsUsingController();
                        int DelaySetting = IsUsingPad ? nTimeToWaitBeforeCenteringCameraFollowVehicle_Pad->get() : nTimeToWaitBeforeCenteringCameraFollowVehicle_KB->get();

                        int TurnSpeed = IsUsingPad ? nCameraTurnSpeedFollowVehicle_Pad->get() : nCameraTurnSpeedFollowVehicle_KB->get();
                        float f = regs.xmm0.f32[0] / (regs.xmm1.f32[0] * (1.0f - TurnSpeed * 0.1f));

                        float& AutoCenterValue = *(float*)(regs.esp + reg);

                        if (ShouldCenter(nTimeToPassBeforeCenteringCameraFollowVehicle, DelaySetting, IsUsingPad))
                            AutoCenterValue = f;
                        else
                            AutoCenterValue = 0.0f;
                    });
                }

                // CCamFollowVehicle Y axis
                {
                    auto pattern = hook::pattern("F3 0F 5E C8 F3 0F 10 04 85");
                    if (!pattern.empty())
                    {
                        static auto dword_E9AA1C = *pattern.get_first<float*>(9);
                        injector::MakeNOP(pattern.get_first(0), 22, true);
                        static auto CCamFollowVehicle__Process_Hook2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            bool IsUsingPad = Natives::IsUsingController();
                            int DelaySetting = IsUsingPad ? nTimeToWaitBeforeCenteringCameraFollowVehicle_Pad->get() : nTimeToWaitBeforeCenteringCameraFollowVehicle_KB->get();

                            int TurnSpeed = IsUsingPad ? nCameraTurnSpeedFollowVehicle_Pad->get() : nCameraTurnSpeedFollowVehicle_KB->get();
                            float f = regs.xmm1.f32[0] / (regs.xmm0.f32[0] * (1.0f - TurnSpeed * 0.1f));
                            regs.xmm0.f32[0] = dword_E9AA1C[regs.eax];

                            float& AutoCenterValue = *(float*)(regs.esp + 0x110 - 0x90);

                            if (ShouldCenter(nTimeToPassBeforeCenteringCameraFollowVehicle, DelaySetting, IsUsingPad))
                                AutoCenterValue = f;
                            else
                                AutoCenterValue = 0.0f;
                        });
                    }
                    else
                    {
                        pattern = hook::pattern("F3 0F 5E C2 F3 0F 11 84 24 ? ? ? ? F3 0F 10 43");
                        injector::MakeNOP(pattern.get_first(0), 13, true);
                        static auto CCamFollowVehicle__Process_Hook2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            bool IsUsingPad = Natives::IsUsingController();
                            int DelaySetting = IsUsingPad ? nTimeToWaitBeforeCenteringCameraFollowVehicle_Pad->get() : nTimeToWaitBeforeCenteringCameraFollowVehicle_KB->get();

                            int TurnSpeed = IsUsingPad ? nCameraTurnSpeedFollowVehicle_Pad->get() : nCameraTurnSpeedFollowVehicle_KB->get();
                            float f = regs.xmm0.f32[0] / (regs.xmm2.f32[0] * (1.0f - TurnSpeed * 0.1f));

                            float& AutoCenterValue = *(float*)(regs.esp + 0x100 - 0x44);

                            if (ShouldCenter(nTimeToPassBeforeCenteringCameraFollowVehicle, DelaySetting, IsUsingPad))
                                AutoCenterValue = f;
                            else
                                AutoCenterValue = 0.0f;
                        });
                    }
                }
            }

            // Disable driveby while using the cellphone
            {
                auto pattern = find_pattern("51 57 8B 7C 24 ? 80 BF ? ? ? ? ? 75", "55 8B 6C 24 ? 80 BD ? ? ? ? ? 75 ? 80 BD ? ? ? ? ? 74 ? 8B 85");
                CTaskComplexGangDriveby::shPlayerWantsToDoDriveby = safetyhook::create_inline(pattern.get_first(0), CTaskComplexGangDriveby::PlayerWantsToDoDriveby);
            }

            // Custom FOV
            {
                auto pattern = hook::pattern("56 6A ? 6A ? 8B F1 E8 ? ? ? ? 85 C0 0F 85");
                CCamGame::shPostProcessHeightBasedFOV = safetyhook::create_inline(pattern.get_first(0), CCamGame::PostProcessHeightBasedFOV);

                FusionFixSettings.SetCallback("PREF_CUSTOMFOV", [](int32_t value)
                {
                    nCameraUnpauseTimer1 = 2;
                    nCameraUnpauseTimer2 = 2;
                });
            }

            // Fix mouse cursor scaling
            {
                auto pattern = hook::pattern("F3 0F 11 44 24 ? E8 ? ? ? ? D9 5C 24 ? 80 3D ? ? ? ? ? 74 ? F3 0F 10 05 ? ? ? ? EB ? F3 0F 10 44 24 ? F3 0F 59 05");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto MouseHeightHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        *(float*)(regs.esp + 0x40 - 0x24) = 30.0f * (1.0f / 768.0f);
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 11 44 24 ? E8 ? ? ? ? D9 5C 24 ? 80 3D ? ? ? ? ? 74 ? F3 0F 10 05 ? ? ? ? EB ? F3 0F 10 44 24 ? F3 0F 10 4C 24");
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto MouseHeightHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        *(float*)(regs.esp + 0x40 - 0x18) = 30.0f * (1.0f / 768.0f);
                    });
                }

                pattern = hook::pattern("F3 0F 11 44 24 ? FF 50 ? 66 0F 6E C0 0F 5B C0 6A");
                if (!pattern.empty())
                {
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto MouseWidthHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        *(float*)(regs.esp + 0x3C - 0x2C) = 30.0f * (1.0f / 1024.0f);
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 11 44 24 ? FF D2 F3 0F 2A C0 F3 0F 59 05");
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                    static auto MouseWidthHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        *(float*)(regs.esp + 0x3C - 0x1C) = 30.0f * (1.0f / 1024.0f);
                    });
                }
            }

            // Hide the mouse cursor texture when using a gamepad
            // Note: It only disables it visually, so a mouse can still be used simultaneously with a controller to select things. The start menu also uses a different cursor, so this won't also hide that one.
            // TODO: Improve this in the future? Like locking the mouse positions in place at least when a gamepad is used?
            {
                auto pattern = hook::pattern("83 EC ? 53 55 56 57 6A ? E8 ? ? ? ? 83 C4");
                shsub_5ADB20 = safetyhook::create_inline(pattern.get_first(0), sub_5ADB20);
            }

            // Pause menu map crosshair aspect ratio scaling
            // Note: Xbox code seems different enough. If PC only needs scaling by aspect ratio, this could already be right. If not, maybe Xbox code could be ported in case size is also different.
            {
                auto pattern = hook::pattern("F3 0F 10 15 ? ? ? ? F3 0F 10 5C 24 ? 0F B6 C0");
                if (!pattern.empty())
                {
                    static auto CCustomMenu__RenderMapCrosshair_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        float AspectRatio = ((float)*rage::grcDevice::ms_nActiveWidth / (float)*rage::grcDevice::ms_nActiveHeight);

                        *(float*)(regs.esp + 0x64 - 0x30) /= (AspectRatio * 0.75f);
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 10 1D ? ? ? ? F3 0F 10 54 24 ? 0F B6 C8");
                    static auto CCustomMenu__RenderMapCrosshair_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        float AspectRatio = ((float)*rage::grcDevice::ms_nActiveWidth / (float)*rage::grcDevice::ms_nActiveHeight);

                        *(float*)(regs.esp + 0x5C - 0x3C) /= (AspectRatio * 0.75f);
                    });
                }
            }

            // Hide hats for regular and fat cops in vehicles, like on consoles
            {
                auto pattern = find_pattern("3B 05 ? ? ? ? 74 ? 3B 05 ? ? ? ? 74 ? 3B 05 ? ? ? ? 74 ? 3B 05 ? ? ? ? 74 ? 3B 05 ? ? ? ? 74 ? 8B 4D",
                                            "3B 05 ? ? ? ? 74 ? 3B 05 ? ? ? ? 74 ? 3B 05 ? ? ? ? 74 ? 3B 05 ? ? ? ? 74 ? 3B 05 ? ? ? ? 74 ? 8B 7F");
                injector::MakeNOP(pattern.get_first(0), 16, true);
            }

            // Remove free camera boundary limits in the video editor
            {
                auto pattern = hook::pattern("73 ? 56 6A ? 6A");
                if (!pattern.empty())
                {
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true); // jnb --> jmp

                    pattern = hook::pattern("0F 86 ? ? ? ? 0F 2E FA");
                    if (!pattern.empty())
                    {
                        injector::WriteMemory<uint16_t>(pattern.get_first(0), 0xE990, true); // jbe --> jmp

                        pattern = hook::pattern("0F 82 ? ? ? ? 80 8F");
                        injector::WriteMemory<uint16_t>(pattern.get_first(0), 0xE990, true); // jb --> jmp

                        pattern = hook::pattern("72 ? 0F 2F 44 24 ? 72 ? 0F 28 C3");
                        injector::WriteMemory(pattern.get_first(0), 0x12EB, true); // jb --> jmp

                        pattern = hook::pattern("72 ? 83 3D ? ? ? ? ? 74 ? A1");
                        injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true); // jb --> jmp
                    }
                    else
                    {
                        pattern = hook::pattern("0F 86 ? ? ? ? F3 0F 10 54 24 ? 0F 2E D4");
                        injector::WriteMemory<uint16_t>(pattern.get_first(0), 0xE990, true); // jbe --> jmp

                        pattern = hook::pattern("0F 82 ? ? ? ? 80 8E");
                        injector::WriteMemory<uint16_t>(pattern.get_first(0), 0xE990, true); // jb --> jmp

                        pattern = hook::pattern("72 ? 0F 2F C5 72 ? 0F 2F FA");
                        injector::WriteMemory(pattern.get_first(0), 0x20EB, true); // jb --> jmp

                        pattern = hook::pattern("0F 82 ? ? ? ? 83 3D ? ? ? ? ? 74 ? A1");
                        injector::WriteMemory<uint16_t>(pattern.get_first(0), 0xE990, true); // jb --> jmp
                    }
                }
            }

            // Fix vehicle glass shard type names
            {
                static auto veh_glass_red = "veh_glass_red";
                static auto veh_glass_amber = "veh_glass_amber";

                auto pattern = find_pattern("68 ? ? ? ? EB ? 6A ? 68 ? ? ? ? E8 ? ? ? ? 83 C4 ? 89 44 24", "68 ? ? ? ? EB ? 6A ? 68 ? ? ? ? E8 ? ? ? ? 83 C4 ? 6A ? 6A ? 50");
                injector::WriteMemory(pattern.get_first(1), &veh_glass_red[0], true);

                pattern = find_pattern("68 ? ? ? ? E8 ? ? ? ? 83 C4 ? 89 44 24 ? 6A ? 6A ? 50", "68 ? ? ? ? E8 ? ? ? ? 83 C4 ? 6A ? 6A ? 50 B9 ? ? ? ? 89 44 24 ? E8 ? ? ? ? 8B F0 85 F6 0F 84");
                injector::WriteMemory(pattern.get_first(1), &veh_glass_amber[0], true);
            }

            // Fix P90 previous key selection in vehicles
            // Note: This doesn't seem to work? There is also not only one condition broken with the P90 so that might need to be looked into as well.
            {
                auto pattern = find_pattern("E8 ? ? ? ? 8B F0 3B 37 75", "E8 ? ? ? ? 8B F0 3B 77 ? 75");
                CPedWeaponMgr::hbGetNextActiveSlot.fun = injector::MakeCALL(pattern.get_first(0), CPedWeaponMgr::GetNextActiveSlot).get();
            }

            // Disable z-write for emissive shaders
            // Fixes visual bugs (e.g. strobe lights in Bahama Mamas (TBoGT)) and more.
            {
                auto pattern = hook::pattern("83 FF ? 74 ? 83 FF ? 75 ? 6A ? 6A");
                static auto loc_AE39CD = resolve_next_displacement(pattern.get_first(5)).value();
                struct CRenderer__AddToDrawListEntityListStandard_Hook
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        // Fix for visual bugs in QUB3D that would occur with this fix. Only enable z-write for emissive shaders when the camera/player height is 3000 or higher.
                        // This height check was present in patch 1.0.4.0 and was removed in patch 1.0.6.0+, in addition z-write for emissive shaders was enabled permanently.
                        const bool IsEmissiveShader = regs.edi == 5 || regs.edi == 4;
                        const float CameraHeight = *(float*)(*CRenderPhase::sm_pCurrent + 0x128 /* CRenderPhase::m_grcViewport1::m_CameraMatrix::d::z */);

                        if (!_stricmp(pszCurrentCutsceneName, "intro"))
                        {
                            return_to(loc_AE39CD);
                        }

                        if (IsEmissiveShader && CameraHeight < 3000.0f)
                        {
                            return;
                        }

                        if (IsEmissiveShader && CameraHeight >= 3000.0f)
                        {
                            bIsQUB3D = true;
                        }

                        return_to(loc_AE39CD);
                    }
                }; injector::MakeInline<CRenderer__AddToDrawListEntityListStandard_Hook>(pattern.get_first(0), pattern.get_first(10));

                // Disable z-write
                pattern = find_pattern("6A ? 8B C8 E8 ? ? ? ? EB ? 33 C0 50 E8 ? ? ? ? 83 C4 ? 8B 45", "6A ? 8B C8 E8 ? ? ? ? EB ? 33 C0 8B C8 E8 ? ? ? ? 8B 4D");
                injector::WriteMemory<uint8_t>(pattern.get_first(1), 0, true);
            }

            // Always show the "friendly"/green state on the health reticle, used to be a gamepad + multiplayer only feature
            // NOTE: In some brief moments this patch may show the "friendly" state even for enemies or police if they are aimed at while being far enough from the player.
            //       There is a chance the reason this state is hidden in the vanilla game is to hide this flaw. It was probably enabled in Multiplayer because you can't notice this as easily maybe.
            if (bAlwaysDisplayHealthOnReticle)
            {
                auto pattern = hook::pattern("74 ? 56 57 E8 ? ? ? ? 83 C4 ? 84 C0 75 ? 8B 46");
                if (!pattern.empty())
                {
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);

                    pattern = hook::pattern("75 ? 38 83 ? ? ? ? 0F 84");
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
                }
                else
                {
                    pattern = hook::pattern("74 ? 56 57 E8 ? ? ? ? 83 C4 ? 84 C0 75 ? 8B 56");
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);

                    pattern = hook::pattern("75 ? 38 86 ? ? ? ? 0F 84");
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
                }
            }

            // Enable the first-person "hood-mode" aim reticle (Active while piloting the Annihilator or Buzzard) on gamepads, this used to be a keyboard & mouse only feature
            // NOTE: This patch removes the ability to switch to the cinematic camera while scrolling through cameras with "Select" on a gamepad while in helicopters.
            //       Same thing happens on keyboard & mouse, but by default. Needs research to see if these two can be decoupled.
            {
                auto pattern = find_pattern("0F 84 ? ? ? ? 85 C9 0F 84 ? ? ? ? 8B 89", "0F 84 ? ? ? ? 85 C0 0F 84 ? ? ? ? 8B 88");
                injector::MakeNOP(pattern.get_first(0), 6, true); // jz --> nop
            }

            // Fix streamed radio stations resetting/restarting after playing a cutscene
            {
                auto pattern = find_pattern("74 ? 85 C9 75 ? 32 C0 50", "74 ? 85 C0 75 ? D9 EE");
                injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true); // jz --> jmp
            }

            // Fix missing background radio in Roman's cab depot during cutscenes (CE only)
            {
                auto pattern = hook::pattern("75 ? 80 3D ? ? ? ? ? 74 ? F3 0F 10 05 ? ? ? ? F3 0F 11 44 24 ? EB ? F3 0F 10 44 24");
                if (!pattern.empty())
                {
                    injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true); // jnz --> jmp
                }
            }

            // Radar zoom delay
            {
                auto pattern = hook::pattern("E8 ? ? ? ? 84 C0 74 ? F3 0F 10 05 ? ? ? ? F3 0F 11 44 24 ? 6A ? E8");
                CRadarNY::hbsub_5DCA80.fun = injector::MakeCALL(pattern.get_first(0), CRadarNY::sub_5DCA80, true).get();
            }

            // Workaround for drunk camera motion blur stencil madness (Many other types of camera shake trigger this)
            {
                auto pattern = find_pattern("E8 ? ? ? ? 6A ? FF 74 24 ? FF 74 24 ? 6A", "E8 ? ? ? ? 83 FF ? 6A");
                CDeferredLightingHelper::hbBeginMotionBlurMask.fun = injector::MakeCALL(pattern.get_first(0), CDeferredLightingHelper::BeginMotionBlurMask, true).get();
            }

            // Fix helicopter searchlights fighting each other when two police helicopters are active on the scene
            // Note: There are more than one approaches available here for tackling this problem, and many speculations as to why its a problem on PC.
            //       Technically, there is nothing in the code different between platforms that would make this problematic on PC.
            //       One plausible theory is that Helicopters might get closer to each other on PC, thus there is a higher chance that they'd both want a shot at the light at the same time.
            //       The Xbox debug build is also interesting, because it seems like it doesn't try to block the searchlights to one specific helicopter judging by the code.
            //       Instead, the code should allow both helicopters to set a light independently on the player.
            //       So we provide a workaround to the problem which should be more defensive and only allow one helicopter to show a light, like originally intended.
            //       And an option to disable the "limit" altogether, like the almost final build of the game. The downside in this case would be consuming another space in the lights' atlas and the night shadows' atlas (If those are enabled).
            {
                if (bFixHelicopterSearchlights == 1)
                {
                    auto pattern = hook::pattern("8B 0D ? ? ? ? 84 C0 8B 47");
                    if (!pattern.empty())
                    {
                        CHeli::dword_1670CD0 = *pattern.get_first<uint32_t*>(2);
                    }
                    else
                    {
                        pattern = hook::pattern("A3 ? ? ? ? F3 0F 10 86 ? ? ? ? 0F BF 4E");
                        CHeli::dword_1670CD0 = *pattern.get_first<uint32_t*>(1);
                    }

                    pattern = find_pattern("55 8B EC 83 E4 ? 81 EC ? ? ? ? 57 8B F9", "55 8B EC 83 E4 ? 81 EC ? ? ? ? 56 57 8B F1 E8 ? ? ? ? 8B 46");
                    CHeli::shPreRender2 = safetyhook::create_inline(pattern.get_first(0), CHeli::PreRender2);
                }
                else if (bFixHelicopterSearchlights == 2)
                {
                    auto pattern = hook::pattern("A1 ? ? ? ? 3B 05 ? ? ? ? 0F 84 ? ? ? ? E8 ? ? ? ? F3 0F 10 87");
                    if (!pattern.empty())
                    {
                        injector::MakeNOP(pattern.get_first(0), 17, true);
                    }
                    else
                    {
                        pattern = hook::pattern("8B 15 ? ? ? ? 3B 15 ? ? ? ? 0F 84 ? ? ? ? E8");
                        injector::MakeNOP(pattern.get_first(0), 18, true);
                    }
                }
            }

            // Menu input delays
            {
                auto pattern = find_pattern("68 ? ? ? ? E8 ? ? ? ? 6A ? E8 ? ? ? ? 8B 0D", "68 ? ? ? ? E8 ? ? ? ? 6A ? E8 ? ? ? ? A1");
                injector::WriteMemory(pattern.get_first(1), nMenuEnteringDelay, true);

                pattern = hook::pattern("68 ? ? ? ? EB ? 6A ? 68 ? ? ? ? 6A");
                injector::WriteMemory(pattern.get_first(1), nMenuExitingDelay, true);

                pattern = hook::pattern("81 F9 ? ? ? ? 72 ? EB ? E8");
                injector::WriteMemory(pattern.get_first(2), nMenuAccessDelayOnStartup, true);
            }

            // Bullet traces
            {
                auto pattern = hook::pattern("0F 2F C8 72 ? FF 76");
                if (!pattern.empty())
                {
                    static auto loc_B5D8D8 = resolve_next_displacement(pattern.get_first(0)).value();
                    injector::MakeNOP(pattern.get_first(0), 5, true);
                    static auto CWeapon__DoWeaponFireFx_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        static auto AlwaysShowBulletTraces = FusionFixSettings.GetRef("PREF_BULLETTRACES");
                        if (AlwaysShowBulletTraces->get())
                        {
                            return;
                        }

                        if (regs.xmm0.f32[0] >= regs.xmm1.f32[0])
                            return_to(loc_B5D8D8);
                    });
                }
                else
                {
                    pattern = hook::pattern("72 ? 8B 56 ? 52");
                    static auto loc_9D2329 = resolve_displacement(pattern.get_first(0)).value();
                    injector::MakeNOP(pattern.get_first(0), 5, true);
                    static auto CWeapon__DoWeaponFireFx_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        static auto AlwaysShowBulletTraces = FusionFixSettings.GetRef("PREF_BULLETTRACES");
                        if (AlwaysShowBulletTraces->get())
                        {
                            regs.edx = *(uint32_t*)(regs.esi + 0x18);

                            return;
                        }

                        // fcomip  st, st(1)
                        constexpr uint32_t FLAG_CF = 1u << 0;
                        if (regs.eflags & FLAG_CF)
                            return_to(loc_9D2329);

                        regs.edx = *(uint32_t*)(regs.esi + 0x18);
                    });
                }

                // Force IV/TLAD bullet trace particles, TBoGT trace particles have weird/wrong positioning
                pattern = find_pattern("75 ? 68 ? ? ? ? EB ? 68 ? ? ? ? E8 ? ? ? ? 83 C4 ? 8B F8", "75 ? 68 ? ? ? ? EB ? 68 ? ? ? ? E8 ? ? ? ? 83 C4 ? 6A");
                injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true); // jnz --> jmp
            }

            // Fix TLAD Marta Full of Grace crash (https://github.com/GTAmodding/GTAIV-Issues-List/issues/235)
            {
                auto pattern = hook::pattern("F6 80 ? ? ? ? ? 74 ? 8B 80 ? ? ? ? 56");
                static auto loc_BA91F2 = resolve_next_displacement(pattern.get_first(0)).value();
                injector::MakeNOP(pattern.get_first(0), 9, true);
                static auto IS_CHAR_SITTING_IN_ANY_CAR_HOOK = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    if (!regs.eax || (*(uint8_t*)(regs.eax + 0x26C) & 4) == 0)
                        return_to(loc_BA91F2);
                });
            }

            // Water flicker mitigation experiment
            {
                /*auto pattern = find_pattern("83 EC ? F3 0F 10 4D ? F3 0F 10 05 ? ? ? ? 0F 2F C1 F3 0F 10 55 ? F3 0F 10 5D ? F3 0F 11 54 24 ? F3 0F 11 5C 24 ? F3 0F 11 4C 24 ? 72 ? 6A ? 83 EC ? F3 0F 11 5C 24 ? F3 0F 11 14 24 E8 ? ? ? ? D9 5C 24 ? 83 C4 ? 8B 45", "83 EC ? F3 0F 10 45 ? F3 0F 10 0D ? ? ? ? F3 0F 11 44 24 ? F3 0F 10 45 ? F3 0F 11 44 24 ? F3 0F 10 45 ? 0F 2F C8 F3 0F 11 44 24 ? 72 ? D9 45 ? 6A ? 83 EC ? D9 5C 24 ? D9 45 ? D9 1C 24 E8 ? ? ? ? D9 5C 24 ? 83 C4 ? D9 EE");
                GET_NTH_CLOSEST_WATER_NODE_WITH_HEADING = (decltype(GET_NTH_CLOSEST_WATER_NODE_WITH_HEADING))pattern.get_first(-6);

                pattern = find_pattern("E8 ? ? ? ? 85 C0 0F 84 ? ? ? ? 80 7D ? ? C6 44 24 ? ? 74 ? FF 77", "E8 ? ? ? ? 85 C0 0F 84 ? ? ? ? 80 7D ? ? C6 44 24 ? ? 74 ? 8B 4E");
                hbIsSphereVisible.fun = injector::MakeCALL(pattern.get_first(0), AddLightIsSphereVisible, true).get();

                pattern = find_pattern("E8 ? ? ? ? 85 C0 0F 84 ? ? ? ? 8B 4C 24 ? 3B F9", "E8 ? ? ? ? 85 C0 0F 84 ? ? ? ? 3B F7");
                hbIsSphereVisible.fun = injector::MakeCALL(pattern.get_first(0), DrawWaterIsSphereVisible, true).get();

                pattern = find_pattern("E8 ? ? ? ? 8B D0 8B B4 BA", "E8 ? ? ? ? 8B BC B0 ? ? ? ? 8B 94 B0");
                static auto RenderWaterHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    bAnyVisibleNearbyWaterOnScreen = false;
                    WaterQuadsCount = 0;
                });

                pattern = find_pattern("0F B7 0C 72 C1 E1 ? 0F BF 81", "0F B7 04 78 C1 E0 04 0F BF 88 ? ? ? ? 8B 14 CD ? ? ? ? 0F BF 88 ? ? ? ? 03 C9");
                static auto RenderWaterCounterHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    WaterQuadsCount++;
                });

                //pattern = find_pattern("F3 0F 11 44 24 ? FF 74 24 ? F3 0F 11 44 24", "F3 0F 11 44 24 ? 8B 54 24 ? 52 F3 0F 11 44 24");
                //static auto RenderWaterCounterHook2 = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                //{
                //    WaterQuadsCount++;
                //});

                pattern = find_pattern("8B 35 ? ? ? ? F3 0F 11 44 24 ? F3 0F 10 80", "8B 1D ? ? ? ? F3 0F 10 83");
                static auto RenderLightsHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    bAnyVisibleNearbyLightOnScreen = false;
                });*/

                // TODO: Remove? It barely affects water flicker and messes with another workaround for light related stuff (guh)
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

                            /*if (Natives::IsInteriorScene())
                            {
                                return; // Flicker - Always in interiors
                            }

                            if (!bAnyVisibleNearbyWaterOnScreen)
                            {
                                return; // Flicker - No water on screen
                            }
                            else if (bAnyVisibleNearbyLightOnScreen)
                            {
                                return; // Flicker - Water and lights
                            }*/
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

            // Restore console/pre-1.0.6.0 pause menu info spacing
            {
                // These also had a %s at the start on console but the sprintf call on PC doesn't support it and replacing it is too much for such a little thing
                static auto a02d02d_0 = "  /  %02d:%02d";
                static auto a02d02d_1 = "  /  %02d:%02d  /  %d$";
                static auto a02d02d_2 = "  /  %02d:%02d  /  $%d";

                auto pattern = find_pattern("68 ? ? ? ? 64 A1 ? ? ? ? 8B 00 05 ? ? ? ? 50 E8 ? ? ? ? 83 C4 ? 64 A1", "68 ? ? ? ? 81 C1 ? ? ? ? 51 E8 ? ? ? ? 83 C4");
                injector::WriteMemory(pattern.get_first(1), a02d02d_0, true);

                pattern = find_pattern("68 ? ? ? ? EB ? 83 3D ? ? ? ? ? 8B 0D ? ? ? ? 0F 45 0D ? ? ? ? 83 3D ? ? ? ? ? 51", "68 ? ? ? ? 81 C2 ? ? ? ? 52 EB ? E8");
                injector::WriteMemory(pattern.get_first(1), a02d02d_0, true);

                pattern = find_pattern("68 ? ? ? ? 64 A1 ? ? ? ? 8B 00 05 ? ? ? ? 50 E8 ? ? ? ? 83 C4 ? E9", "68 ? ? ? ? 50 E8 ? ? ? ? 83 C4 ? EB ? A1");
                injector::WriteMemory(pattern.get_first(1), a02d02d_1, true);

                pattern = find_pattern("68 ? ? ? ? EB ? 83 3D ? ? ? ? ? 8B 0D ? ? ? ? 0F 45 0D ? ? ? ? 83 3D ? ? ? ? ? 56", "68 ? ? ? ? 51 E8 ? ? ? ? 83 C4 ? E9 ? ? ? ? 56");
                injector::WriteMemory(pattern.get_first(1), a02d02d_2, true);
            }

            // Fix NPC vehicles swerving lanes due to trains running above/below them, kind of hacky probably but it works (Clippy95)
            {
                auto pattern = hook::pattern("F3 0F 10 0D ? ? ? ? 0F 2F C8 76 ? 3B F1");
                if (!pattern.empty())
                {
                    static auto dword_FE8AFC = *pattern.get_first<float*>(4);
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CCarAI__WeaveThroughCarsSectorList_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm1.f32[0] = *dword_FE8AFC;

                        auto Vehicle = regs.esi;
                        auto m_nVehicleType = *(uint32_t*)(Vehicle + 0x1304);

                        // The delta height check is normally always 8.0f, we make it 2.5f for trains only
                        if (m_nVehicleType == VEHICLETYPE_TRAIN)
                        {
                            regs.xmm1.f32[0] = 2.5f;
                        }
                    });
                }
                else
                {
                    pattern = hook::pattern("F3 0F 10 0D ? ? ? ? 0F 2F C8 76 ? 3B F7");
                    static auto flt_D6E58C = *pattern.get_first<float*>(4);
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    static auto CCarAI__WeaveThroughCarsSectorList_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        regs.xmm1.f32[0] = *flt_D6E58C;

                        auto Vehicle = regs.esi;
                        auto m_nVehicleType = *(uint32_t*)(Vehicle + 0x1354);

                        // The delta height check is normally always 8.0f, we make it 2.5f for trains only
                        if (m_nVehicleType == VEHICLETYPE_TRAIN)
                        {
                            regs.xmm1.f32[0] = 2.5f;
                        }
                    });
                }
            }

            // Fix the date going backwards when dying or getting busted between 12pm and 11pm, and respraying between 9pm and 11:59pm (https://github.com/GTAmodding/GTAIV-Issues-List/issues/164)
            {
                auto pattern = hook::pattern("6A ? 53 55 56");
                if (!pattern.empty())
                {
                    uint8_t* ptr = (uint8_t*)pattern.get_first(0);

                    injector::scoped_unprotect protect{ ptr, 3 };

                    ptr[0] = 0x53; // Push day register
                    ptr[1] = 0x6A;
                    ptr[2] = 0xFF;
                }
                else
                {
                    pattern = hook::pattern("6A ? 56 53 55 E8 ? ? ? ? 69 FF");

                    uint8_t* ptr = (uint8_t*)pattern.get_first(0);

                    injector::scoped_unprotect protect{ ptr, 3 };

                    ptr[0] = 0x56; // Push day register
                    ptr[1] = 0x6A;
                    ptr[2] = 0xFF;
                }
            }

            // Native patches
            {
                // Fix TLAD phone calls crash (https://github.com/GTAmodding/GTAIV-Issues-List/issues/232)
                {
                    hbCOMPARE_STRING.fun = NativeOverride::Register(Natives::NativeHashes::COMPARE_STRING, NATIVE_COMPARE_STRING_1, "E8 ? ? ? ? ? ? ? ? ? 83 C4 ? 89 ? 5E C3", 30);
                    if (!hbCOMPARE_STRING.fun)
                    {
                        auto pattern = hook::pattern("8B 44 24 ? 85 C0 75 ? 32 C0 C3 8B 4C 24 ? 85 C9");
                        shNATIVE_COMPARE_STRING = safetyhook::create_inline(pattern.get_first(0), NATIVE_COMPARE_STRING_2);
                    }
                }
            }
        };
    }
} Fixes;