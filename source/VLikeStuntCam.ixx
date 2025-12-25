#if 0
module;

#include <common.hxx>

export module vlikestuntcam;

import common;
import natives;

class VLikeStuntCam
{
public:
    VLikeStuntCam()
    {
        FusionFix::onInitEvent() += []()
        {
            //128 = min speed, -128 = maxSpeed
            static float camSpeed = 128.0f;
            static bool canSwitchRadio = true;

            auto pattern = find_pattern("8D 8F D8 29 00 00 E8 ? ? ? ?");
            static auto sub_E8CB90_1 = safetyhook::create_mid(pattern.get_first(6), [](SafetyHookContext& regs)
            {
                if(!canSwitchRadio)
                    *(int8_t*)(regs.ecx + 6) = 0;
            });

            pattern = find_pattern("8D 8F E8 29 00 00 E8 ? ? ? ?");
            static auto sub_E8CB90_2 = safetyhook::create_mid(pattern.get_first(6), [](SafetyHookContext& regs)
            {
                if(!canSwitchRadio)
                    *(int8_t*)(regs.ecx + 6) = 0;
            });


            //reset back to defaults
            pattern = find_pattern("E8 ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 11 05 ? ? ? ? C3");
            static auto CIntermezzoEventStuntJump__virtual0x8Hook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            {
                camSpeed = 128.0f;
                canSwitchRadio = true;
            });

            pattern = find_pattern("F3 0F 5C 05 ? ? ? ? 0F 57 D2 0F 2F C2");
            static auto CIntermezzoEventStuntJump__virtual0x4Hook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            {
                int32_t wheel;
                Natives::GetMouseWheel(&wheel);

                camSpeed = std::clamp(camSpeed + float(wheel / 4), -128.0f, 128.0f);
                canSwitchRadio = false;

                regs.xmm0.f32[0] = camSpeed;
            });
        };
    }
} VLikeStuntCam;
#endif