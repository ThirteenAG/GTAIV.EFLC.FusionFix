module;

#include <common.hxx>

export module centeredcam;

import common;
import comvars;
import settings;

namespace CCamFollowVehicle
{
    GameRef<bool> ms_bAllowSideOffset;
}

namespace CCamFollowPed
{
    GameRef<float> ms_targetOffset;
}

class CenteredCam
{
public:
    static inline bool bLoadedCenteredVehicleCamIVasi = false;
    static inline bool bLoadedCenteredOnFootCamIVasi = false;

    CenteredCam()
    {
        FusionFix::onInitEventAsync() += []()
        {
            auto pattern = find_pattern("80 3D ? ? ? ? ? 0F 84 ? ? ? ? 8B 87 ? ? ? ? 85 C0", "80 3D ? ? ? ? ? 0F 84 ? ? ? ? 8B 86 ? ? ? ? 85 C0");
            CCamFollowVehicle::ms_bAllowSideOffset.SetAddress(*pattern.get_first<bool*>(2));

            static auto GetSideOffsetHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
            {
                static auto cc = FusionFixSettings.GetRef("PREF_CENTEREDCAMERA");
                if (cc->get() && !bLoadedCenteredVehicleCamIVasi)
                    CCamFollowVehicle::ms_bAllowSideOffset = false;
                else
                    CCamFollowVehicle::ms_bAllowSideOffset = true;
            });

            FusionFixSettings.SetCallback("PREF_CENTEREDCAMERA", [](int32_t value)
            {
                bMenuNeedsUpdate = 4;
                bMenuNeedsUpdate2 = 4;
            });

            pattern = find_pattern("F3 0F 10 2D ? ? ? ? F3 0F 10 25 ? ? ? ? F3 0F 59 D8", "F3 0F 10 25 ? ? ? ? F3 0F 10 15 ? ? ? ? F3 0F 10 1D ? ? ? ? F3 0F 59 E6");
            CCamFollowPed::ms_targetOffset.SetAddress(*pattern.get_first<float*>(4));

            static auto CCamFollowPedWidgetSetupHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
            {
                static auto cc = FusionFixSettings.GetRef("PREF_CENTEREDCAMERAFOOT");
                if (cc->get() && !bLoadedCenteredOnFootCamIVasi)
                    CCamFollowPed::ms_targetOffset = 0.0f;
                else
                    CCamFollowPed::ms_targetOffset = 0.2f;
            });

            FusionFixSettings.SetCallback("PREF_CENTEREDCAMERAFOOT", [](int32_t value)
            {
                bMenuNeedsUpdate = 4;
                bMenuNeedsUpdate2 = 4;
            });

            auto HandleCenteredVehicleCamIVasi = []()
            {
                bLoadedCenteredVehicleCamIVasi = true;
            };

            auto HandleCenteredOnFootCamIVasi = []()
            {
                bLoadedCenteredOnFootCamIVasi = true;
            };

            CallbackHandler::RegisterCallback(L"CenteredVehicleCamIV.asi", HandleCenteredVehicleCamIVasi);
            CallbackHandler::RegisterCallback(L"CenteredOnFootCamIV.asi", HandleCenteredOnFootCamIVasi);
        };
    }
} CenteredCam;
