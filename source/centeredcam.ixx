module;

#include <common.hxx>

export module centeredcam;

import common;
import comvars;
import settings;

class CenteredCam
{
public:
    static inline bool bLoadedCenteredVehicleCamIVasi = false;
    static inline float fRightMult = 0.5f;
    static inline float fForwardMult = 0.2f;
    static inline float fUpMult = 0.2f;

    using CVehicle = void;
    using CPed = void;
    using CPlayerPed = void;

    static inline ptrdiff_t VehicleTypeOffset = 0x1304;
    static inline CVehicle* (*GetVehiclePedWouldEnter)(CPed* ped, rage::Vector3* pos, bool arg2) = nullptr;

    static bool IsVehicleTypeOffCenter(CVehicle const* veh)
    {
        uint32_t m_nVehicleType = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(veh) + VehicleTypeOffset);
        return m_nVehicleType == VEHICLETYPE_AUTOMOBILE || m_nVehicleType == VEHICLETYPE_PLANE || m_nVehicleType == VEHICLETYPE_HELI;
    }

    static inline injector::hook_back<void(__fastcall*)(rage::Matrix44*, void*, void*)> hbCopyMatFront;
    static void __fastcall CopyMatFront(rage::Matrix44* mat, void*, void* arg2)
    {
        hbCopyMatFront.fun(mat, 0, arg2);

        static auto cc = FusionFixSettings.GetRef("PREF_CENTEREDCAMERA");
        if (cc->get() && !bLoadedCenteredVehicleCamIVasi)
        {
            auto playa = FindPlayerPed(0);
            auto vehicle = FindPlayerVehicle(0);
            auto m_pMatrix = &(*reinterpret_cast<rage::Matrix44*>(reinterpret_cast<uintptr_t>(playa) + 0x1C));
            if (!vehicle)
                vehicle = GetVehiclePedWouldEnter(playa, (rage::Vector3*)&m_pMatrix->up, 0); //0x1C + 0x10

            if (vehicle && IsVehicleTypeOffCenter(vehicle))
            {
                mat->pos += mat->right * fRightMult;
                mat->pos += mat->at * fUpMult;
                mat->pos -= mat->up * fForwardMult;
            }
        }
    }

    static inline injector::hook_back<void(__fastcall*)(rage::Matrix44*, void*, void*)> hbCopyMatBehind;
    static void __fastcall CopyMatBehind(rage::Matrix44* mat, void*, void* arg2)
    {
        hbCopyMatBehind.fun(mat, 0, arg2);

        static auto cc = FusionFixSettings.GetRef("PREF_CENTEREDCAMERA");
        if (cc->get() && !bLoadedCenteredVehicleCamIVasi)
        {
            auto playa = FindPlayerPed(0);
            auto vehicle = FindPlayerVehicle(0);
            auto m_pMatrix = &(*reinterpret_cast<rage::Matrix44*>(reinterpret_cast<uintptr_t>(playa) + 0x1C));
            if (!vehicle)
                vehicle = GetVehiclePedWouldEnter(playa, (rage::Vector3*)&m_pMatrix->up, 0);

            if (vehicle && IsVehicleTypeOffCenter(vehicle))
            {
                mat->pos -= mat->right * fRightMult;
            }
        }
    }

    CenteredCam()
    {
        FusionFix::onInitEventAsync() += []()
        {
            auto pattern = hook::pattern("55 8B EC 83 E4 F0 83 EC 78 56 8B 75 08 57 F7 86");
            if (!pattern.empty())
                GetVehiclePedWouldEnter = (decltype(GetVehiclePedWouldEnter))pattern.get_first();
            else
            {
                pattern = hook::pattern("55 8B EC 83 E4 ? 83 EC ? 56 8B 75 ? F7 86");
                VehicleTypeOffset = 0x1350;
                GetVehiclePedWouldEnter = (decltype(GetVehiclePedWouldEnter))pattern.get_first();
            }

            pattern = find_pattern("E8 ? ? ? ? 80 A7 ? ? ? ? ? 80 A7 ? ? ? ? ? 80 7C 24", "E8 ? ? ? ? 80 A6 ? ? ? ? ? 80 A6 ? ? ? ? ? 80 BC 24");
            hbCopyMatFront.fun = injector::MakeCALL(pattern.get_first(0), CopyMatFront, true).get();

            pattern = find_pattern("F3 0F 11 44 24 ? E8 ? ? ? ? 5F B0", "F3 0F 11 44 24 ? E8 ? ? ? ? 5F 5E B0 ? 5B 8B E5 5D C2");
            hbCopyMatBehind.fun = injector::MakeCALL(pattern.get_first(6), CopyMatBehind, true).get();

            FusionFixSettings.SetCallback("PREF_CENTEREDCAMERA", [](int32_t value)
            {
                bMenuNeedsUpdate = 4;
                bMenuNeedsUpdate2 = 4;
            });

            auto HandleCenteredVehicleCamIVasi = []()
            {
                bLoadedCenteredVehicleCamIVasi = true;
            };

            CallbackHandler::RegisterCallback(L"CenteredVehicleCamIV.asi", HandleCenteredVehicleCamIVasi);
        };
    }
} CenteredCam;

class CenteredCamOnFoot
{
public:
    static inline bool bLoadedCenteredOnFootCamIVasi = false;
    static inline float fRightMult = -0.1f;
    static inline float fForwardMult = 0.0f;
    static inline float fUpMult = 0.0f;

    static inline injector::hook_back<void(__fastcall*)(rage::Matrix44*, void*, void*)> hbCopyMatFront;
    static void __fastcall CopyMatFront(rage::Matrix44* mat, void*, void* arg2)
    {
        hbCopyMatFront.fun(mat, 0, arg2);

        static auto cc = FusionFixSettings.GetRef("PREF_CENTEREDCAMERAFOOT");
        if (cc->get() && !bLoadedCenteredOnFootCamIVasi)
        {
            mat->pos += mat->right * fRightMult;
            mat->pos += mat->at * fUpMult;
            mat->pos -= mat->up * fForwardMult;
        }
    }

    static inline injector::hook_back<void(__fastcall*)(rage::Matrix44*, void*, void*)> hbCopyMatBehind;
    static void __fastcall CopyMatBehind(rage::Matrix44* mat, void*, void* arg2)
    {
        hbCopyMatBehind.fun(mat, 0, arg2);

        static auto cc = FusionFixSettings.GetRef("PREF_CENTEREDCAMERAFOOT");
        if (cc->get() && !bLoadedCenteredOnFootCamIVasi)
        {
            mat->pos += mat->right * fRightMult;
            mat->pos += mat->at * fUpMult;
            mat->pos -= mat->up * fForwardMult;
        }
    }

    CenteredCamOnFoot()
    {
        FusionFix::onInitEventAsync() += []()
        {
            auto pattern = find_pattern("E8 ? ? ? ? 8A 86 ? ? ? ? 80 A6 ? ? ? ? ? 80 A6", "E8 ? ? ? ? 8A 8E ? ? ? ? 80 A6");
            hbCopyMatFront.fun = injector::MakeCALL(pattern.get_first(0), CopyMatFront, true).get();

            pattern = find_pattern("E8 ? ? ? ? 8B 8E ? ? ? ? 56 8D 44 24 74", "E8 ? ? ? ? 8B 8E ? ? ? ? 56 8D 94 24");
            hbCopyMatBehind.fun = injector::MakeCALL(pattern.get_first(0), CopyMatBehind, true).get();

            FusionFixSettings.SetCallback("PREF_CENTEREDCAMERAFOOT", [](int32_t value)
            {
                bMenuNeedsUpdate = 4;
                bMenuNeedsUpdate2 = 4;
            });

            auto HandleCenteredOnFootCamIVasi = []()
            {
                bLoadedCenteredOnFootCamIVasi = true;
            };

            CallbackHandler::RegisterCallback(L"CenteredOnFootCamIV.asi", HandleCenteredOnFootCamIVasi);
        };
    }
} CenteredCamOnFoot;