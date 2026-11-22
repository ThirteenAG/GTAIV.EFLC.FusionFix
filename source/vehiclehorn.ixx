module;

#include <common.hxx>

export module vehiclehorn;

import common;

namespace VehicleHorn
{
    bool (__thiscall* GetHornState)(uintptr_t audioEntity) = nullptr;

    bool __fastcall GetHornStateUnlessWrecked(uintptr_t audioEntity)
    {
        constexpr ptrdiff_t VehicleOffset = 0x820; // audVehicleAudioEntity
        constexpr ptrdiff_t StatusOffset = 0x28;   // CEntity
        constexpr uint32_t StatusMask = 0x7C00;
        constexpr uint32_t Wrecked = 3 << 10;

        auto vehicle = *reinterpret_cast<const uintptr_t*>(audioEntity + VehicleOffset);
        auto status = *reinterpret_cast<const uint32_t*>(vehicle + StatusOffset) & StatusMask;
        return status != Wrecked && GetHornState(audioEntity);
    }
}

class VehicleHornFix
{
public:
    VehicleHornFix()
    {
        FusionFix::onInitEventAsync() += []()
        {
            // audVehicleAudioEntity::UpdateHornAndSiren, where it refreshes the local horn request.
            auto update = hook::pattern("8B CF E8 ? ? ? ? 8A C8 88 8F 41 0B 00 00 EB ? 8A 8F 41 0B 00 00");
            if (update.size() != 1)
                return;

            auto call = update.get_first(2);
            auto getHornState = injector::GetBranchDestination(call).as_int();
            auto state = hook::range_pattern(getHornState, getHornState + 31,
                "56 8B F1 8B 8E 20 08 00 00 F6 81 17 0F 00 00 08 0F 85 ? ? ? ? 8B 51 28 F7 C2 00 00 80 00");
            if (state.size() != 1 || state.get_first() != reinterpret_cast<void*>(getHornState))
                return;

            // The native update stops the held horn when its request goes false.
            VehicleHorn::GetHornState = reinterpret_cast<decltype(VehicleHorn::GetHornState)>(getHornState);
            injector::MakeCALL(call, VehicleHorn::GetHornStateUnlessWrecked, true);
        };
    }
} VehicleHornFix;
