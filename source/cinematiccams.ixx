module;

#include <common.hxx>

export module cinematiccams;

import common;
import settings;

SafetyHookInline shVaultWallDetectTrigger = {};
bool __fastcall VaultWallDetectTrigger(void* _this, void* edx)
{
    static auto actionCam = FusionFixSettings.GetRef("PREF_ACTIONCAM");
    if (!actionCam->get())
    {
        return false;
    }
    return shVaultWallDetectTrigger.unsafe_fastcall<bool>(_this, edx);
}

SafetyHookInline shJackVehicleDetectTrigger = {};
bool __fastcall JackVehicleDetectTrigger(void* _this, void* edx)
{
    static auto actionCam = FusionFixSettings.GetRef("PREF_ACTIONCAM");
    if (!actionCam->get())
    {
        return false;
    }
    return shJackVehicleDetectTrigger.unsafe_fastcall<bool>(_this, edx);
}

class CinematicCams
{
public:
    CinematicCams()
    {
        FusionFix::onInitEvent() += []()
        {
            // CIntermezzoEventVaultWall::DetectTrigger
            auto pattern = find_pattern("55 8B EC 83 E4 F0 83 EC 28 56 57 8B F1 E8 ? ? ? ? 84 C0 0F 85 ? ? ? ? E8 ? ? ? ? 8B F8 85 FF 0F 84");
            if (!pattern.empty())
            {
                shVaultWallDetectTrigger = safetyhook::create_inline(pattern.get_first(), VaultWallDetectTrigger);
            }

            // CIntermezzoEventPlayerJackVehicle::DetectTrigger
            pattern = find_pattern("55 8B EC 83 E4 F0 83 EC 18 56 57 8B F1 E8 ? ? ? ? 84 C0 75 ? E8 ? ? ? ? 8B F8 85 FF 74");
            if (!pattern.empty())
            {
                shJackVehicleDetectTrigger = safetyhook::create_inline(pattern.get_first(), JackVehicleDetectTrigger);
            }

            // CStuntJumpManager::Update: skip CCamInterface::TriggerStuntJump and the slow motion that goes with it
            pattern = find_pattern("8D 46 40 50 B9 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 84 C0 75");
            if (!pattern.empty())
            {
                static auto loc_CF4008 = resolve_next_displacement(pattern.get_first(21)).value();
                static auto StuntJumpCamHook = safetyhook::create_mid(pattern.get_first(3), [](SafetyHookContext& regs)
                {
                    static auto stuntJumpCam = FusionFixSettings.GetRef("PREF_STUNTJUMPCAM");
                    if (!stuntJumpCam->get())
                    {
                        return_to(loc_CF4008);
                    }
                });
            }
        };
    }
} CinematicCams;
