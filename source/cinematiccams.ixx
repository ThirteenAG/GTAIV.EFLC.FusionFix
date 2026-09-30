module;

#include <common.hxx>

export module cinematiccams;

import common;
import settings;

namespace CIntermezzoEventVaultWall
{
    SafetyHookInline shDetectTrigger = {};
    bool __fastcall DetectTrigger(void* _this, void* edx)
    {
        static auto ActionCam = FusionFixSettings.GetRef("PREF_ACTIONCAM");
        if (!ActionCam->get())
        {
            return false;
        }

        return shDetectTrigger.unsafe_fastcall<bool>(_this, edx);
    }
}

namespace CIntermezzoEventPlayerJackVehicle
{
    SafetyHookInline shDetectTrigger = {};
    bool __fastcall DetectTrigger(void* _this, void* edx)
    {
        static auto ActionCam = FusionFixSettings.GetRef("PREF_ACTIONCAM");
        if (!ActionCam->get())
        {
            return false;
        }

        return shDetectTrigger.unsafe_fastcall<bool>(_this, edx);
    }
}

class CinematicCams
{
public:
    CinematicCams()
    {
        FusionFix::onInitEventAsync() += []()
        {
            auto pattern = find_pattern("55 8B EC 83 E4 ? 83 EC ? 56 57 8B F1 E8 ? ? ? ? 84 C0 0F 85", "55 8B EC 83 E4 ? 83 EC ? 56 57 8B F9 E8 ? ? ? ? 84 C0 0F 85 ? ? ? ? E8 ? ? ? ? 8B F0 85 F6 0F 84 ? ? ? ? F7 46");
            CIntermezzoEventVaultWall::shDetectTrigger = safetyhook::create_inline(pattern.get_first(0), CIntermezzoEventVaultWall::DetectTrigger);

            pattern = find_pattern("55 8B EC 83 E4 ? 83 EC ? 56 57 8B F1 E8 ? ? ? ? 84 C0 75", "55 8B EC 83 E4 ? 83 EC ? 56 57 8B F9 E8 ? ? ? ? 84 C0 0F 85 ? ? ? ? E8 ? ? ? ? 8B F0 85 F6 0F 84 ? ? ? ? 8B 8E");
            CIntermezzoEventPlayerJackVehicle::shDetectTrigger = safetyhook::create_inline(pattern.get_first(0), CIntermezzoEventPlayerJackVehicle::DetectTrigger);

            // Skip CCamInterface::TriggerStuntJump and the slow motion that goes with it
            pattern = hook::pattern("8D 46 ? 50 B9 ? ? ? ? E8 ? ? ? ? E8");
            if (!pattern.empty())
            {
                static auto loc_CF4008 = resolve_next_displacement(pattern.get_first(21)).value();
                static auto CStuntJumpManager__Update_Hook = safetyhook::create_mid(pattern.get_first(3), [](SafetyHookContext& regs)
                {
                    static auto StuntJumpCam = FusionFixSettings.GetRef("PREF_STUNTJUMPCAM");
                    if (!StuntJumpCam->get())
                    {
                        return_to(loc_CF4008);
                    }
                });
            }
            else
            {
                pattern = hook::pattern("8B 0D ? ? ? ? 83 C1 ? 51 B9");
                static auto loc_CC3F66 = resolve_next_displacement(pattern.get_first(27)).value();
                static auto CStuntJumpManager__Update_Hook = safetyhook::create_mid(pattern.get_first(9), [](SafetyHookContext& regs)
                {
                    static auto StuntJumpCam = FusionFixSettings.GetRef("PREF_STUNTJUMPCAM");
                    if (!StuntJumpCam->get())
                    {
                        return_to(loc_CC3F66);
                    }
                });
            }
        };
    }
} CinematicCams;