module;

#include <common.hxx>

export module centeredcam;

import common;
import settings;

namespace rage
{
    class Vector3
    {
    public:
        float x, y, z;
    };

    class Vector4
    {
    public:
        float x, y, z, w;

        Vector4 operator+(const Vector4& other) const
        {
            return Vector4(x + other.x, y + other.y, z + other.z, w + other.w);
        }

        Vector4 operator-(const Vector4& other) const
        {
            return Vector4(x - other.x, y - other.y, z - other.z, w - other.w);
        }

        Vector4 operator*(float scalar) const
        {
            return Vector4(x * scalar, y * scalar, z * scalar, w * scalar);
        }

        Vector4 operator+(float scalar) const
        {
            return Vector4(x + scalar, y + scalar, z + scalar, w + scalar);
        }

        Vector4 operator-(float scalar) const
        {
            return Vector4(x - scalar, y - scalar, z - scalar, w - scalar);
        }

        Vector4& operator+=(const Vector4& other)
        {
            x += other.x;
            y += other.y;
            z += other.z;
            w += other.w;
            return *this;
        }

        Vector4& operator-=(const Vector4& other)
        {
            x -= other.x;
            y -= other.y;
            z -= other.z;
            w -= other.w;
            return *this;
        }
    };

    class Matrix44
    {
    public:
        Vector4 right;
        Vector4 up;
        Vector4 at;
        Vector4 pos;
    };
}

class CenteredCam
{
public:
    static inline float fRightMult = 0.5f;
    static inline float fForwardMult = 0.2f;
    static inline float fUpMult = 0.2f;

    using CVehicle = void;
    using CPed = void;
    using CPlayerPed = void;

    static inline CPlayerPed* (*FindPlayerPed)(int32_t id) = nullptr;
    static inline CVehicle* (*FindPlayerVehicle)(int32_t id) = nullptr;
    static inline CVehicle* (*GetVehiclePedWouldEnter)(CPed* ped, rage::Vector3* pos, bool arg2) = nullptr;

    static bool IsVehicleTypeOffCenter(CVehicle const* veh)
    {
        enum eVehicleType
        {
            VEHICLETYPE_AUTOMOBILE = 0x0,
            VEHICLETYPE_BIKE = 0x1,
            VEHICLETYPE_BOAT = 0x2,
            VEHICLETYPE_TRAIN = 0x3,
            VEHICLETYPE_HELI = 0x4,
            VEHICLETYPE_PLANE = 0x5,
        };

        uint32_t m_nVehicleType = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(veh) + 0x1304);
        return m_nVehicleType == VEHICLETYPE_AUTOMOBILE || m_nVehicleType == VEHICLETYPE_PLANE || m_nVehicleType == VEHICLETYPE_HELI;
    }

    static inline injector::hook_back<void(__fastcall*)(rage::Matrix44*, void*, void*)> hbCopyMatFront;
    static void __fastcall CopyMatFront(rage::Matrix44* mat, void*, void* arg2)
    {
        hbCopyMatFront.fun(mat, 0, arg2);

        static auto cc = FusionFixSettings.GetRef("PREF_CENTEREDCAMERA");
        if (cc->get())
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
        if (cc->get())
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
            auto pattern = find_pattern("8B 44 24 04 85 C0 75 18 A1", "8B 44 24 ? 85 C0 75 ? A1 ? ? ? ? 83 F8 ? 74");
            FindPlayerPed = (decltype(FindPlayerPed))pattern.get_first();

            pattern = hook::pattern("8B 44 24 04 85 C0 75 15");
            FindPlayerVehicle = (decltype(FindPlayerVehicle))pattern.get_first();

            pattern = find_pattern("55 8B EC 83 E4 F0 83 EC 78 56 8B 75 08 57 F7 86", "55 8B EC 83 E4 ? 83 EC ? 56 8B 75 ? F7 86");
            GetVehiclePedWouldEnter = (decltype(GetVehiclePedWouldEnter))pattern.get_first();

            pattern = find_pattern("E8 ? ? ? ? 80 A7 ? ? ? ? ? 80 A7 ? ? ? ? ? 80 7C 24", "E8 ? ? ? ? 80 A6 ? ? ? ? ? 80 A6 ? ? ? ? ? 80 BC 24");
            hbCopyMatFront.fun = injector::MakeCALL(pattern.get_first(0), CopyMatFront, true).get();

            pattern = find_pattern("F3 0F 11 44 24 ? E8 ? ? ? ? 5F B0", "F3 0F 11 44 24 ? E8 ? ? ? ? 5F 5E B0 ? 5B 8B E5 5D C2");
            hbCopyMatBehind.fun = injector::MakeCALL(pattern.get_first(6), CopyMatBehind, true).get();
        };
    }
} CenteredCam;