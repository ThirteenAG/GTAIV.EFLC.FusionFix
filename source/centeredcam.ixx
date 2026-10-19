module;

#include <common.hxx>

export module centeredcam;

import common;
import comvars;
import natives;
import settings;

namespace CCamFollowVehicle
{
    GameRef<bool> ms_bAllowSideOffset;
}

namespace CCamFollowPed
{
    GameRef<float> ms_targetOffset;
}

namespace CameraAutoCenter
{
    // Menu visibility is independent of saved settings; INI defaults are in settings.ixx.
    constexpr bool ShowCameraMenuOption = true;
    constexpr int32_t DefaultAutoCenterCamera = 1;
    constexpr auto MenuPreferenceName = "PREF_AUTOCENTERCAMERA";
    static bool autoCenterCamera = DefaultAutoCenterCamera != 0;

    static void InitializeSettings()
    {
        FusionFixSettings.SetCallback(MenuPreferenceName, [](int32_t value) { autoCenterCamera = value != 0; });
        FusionFixSettings.Set(MenuPreferenceName, FusionFixSettings.Get(MenuPreferenceName));

        if constexpr (ShowCameraMenuOption)
        {
            FusionFixSettings.AddToggle(CSettings::MenuScreen::ControllerOptions,
                "FF_AUTOCENTER", MenuPreferenceName, "CenterDelayFoot", true);
        }
    }

    static bool available = false;
    static SafetyHookMid shPedPitch;
    static uintptr_t (__thiscall* GetSimplestActiveMovementTask)(uintptr_t);
    static uintptr_t (__thiscall* FindActiveTaskByType)(uintptr_t, int);
    static ptrdiff_t intelligenceOffset = 0;
    static ptrdiff_t taskManagerOffset = 0;
    static ptrdiff_t pedVehicleOffset = 0xB30;
    static bool legacyLayout = false;

    static uintptr_t GetPlayerTaskManager()
    {
        // This preference applies only to the active controller, never keyboard/mouse follow.
        if (!available || autoCenterCamera || !Natives::IsUsingController() || !CPlayer::getLocalPlayerPed)
            return 0;
        const auto ped = CPlayer::getLocalPlayerPed();
        if (!ped || !Natives::IsPlayerControlOn(Natives::GetPlayerId()) || Natives::IsHintRunning())
            return 0;
        const auto intelligence = *reinterpret_cast<const uintptr_t*>(ped + intelligenceOffset);
        return intelligence ? intelligence + taskManagerOffset : 0;
    }

    static bool HasTask(uintptr_t manager, TaskID type)
    {
        return FindActiveTaskByType(manager, std::to_underlying(type)) != 0;
    }

    static bool IsGameplayFollow(uintptr_t camera)
    {
        const auto parent = *reinterpret_cast<const uintptr_t*>(camera + 0x118);
        if (!parent || *reinterpret_cast<const uint32_t*>(camera + 0x130) == 0)
            return false;
        // CCam::GetType identifies the normal CCamGame parent; scripted follow cameras are excluded.
        const auto getType = reinterpret_cast<int (__thiscall*)(uintptr_t)>((*reinterpret_cast<const uintptr_t**>(parent))[10]);
        return getType(parent) == 7;
    }

    // On foot, retain native yaw centering so sprinting does not require manual camera steering.
    static bool HoldPedPitch(uintptr_t camera, uintptr_t ped, uintptr_t control)
    {
        const auto manager = GetPlayerTaskManager();
        if (!manager || !control || ped != CPlayer::getLocalPlayerPed() || !IsGameplayFollow(camera)
            || (*reinterpret_cast<const uint8_t*>(camera + 0x38C) & 0x88)
            || (*reinterpret_cast<const uint8_t*>(camera + 0x38D) & 0x1C))
            return false;
        if (HasTask(manager, TaskID::TaskComplexPlayerInCover)
            || HasTask(manager, TaskID::TaskSimpleAimGun)
            || HasTask(manager, TaskID::TaskSimplePlayerAimProjectile))
            return false;
        // Ladder movement has its own task rather than TaskSimpleMovePlayer.
        if (HasTask(manager, TaskID::TaskComplexClimbLadder))
            return true;

        // Other task-controlled movement retains native pitch follow.
        const auto movement = GetSimplestActiveMovementTask(manager);
        if (!movement)
            return false;
        const auto getType = reinterpret_cast<int (__thiscall*)(uintptr_t)>((*reinterpret_cast<const uintptr_t**>(movement))[3]);
        return getType(movement) == std::to_underlying(TaskID::TaskSimpleMovePlayer)
            && HasTask(manager, TaskID::TaskComplexPlayerOnFoot);
    }

    static bool HoldVehicle(uintptr_t camera, uintptr_t control)
    {
        const auto manager = GetPlayerTaskManager();
        if (!manager || !control || !IsGameplayFollow(camera)
            || (*reinterpret_cast<const uint8_t*>(camera + 0x18C) & 0x60))
            return false;
        const auto ped = CPlayer::getLocalPlayerPed();
        if (!(*reinterpret_cast<const uint8_t*>(ped + 0x26C) & 4)
            || *reinterpret_cast<const uintptr_t*>(ped + pedVehicleOffset) != *reinterpret_cast<const uintptr_t*>(camera + 0x170))
            return false;
        // Entry/exit, drive-by aiming and scripted driving retain their native camera behavior.
        return HasTask(manager, TaskID::TaskComplexPlayerDrive)
            && !HasTask(manager, TaskID::TaskSimpleNewGangDriveBy)
            && !HasTask(manager, TaskID::TaskComplexNewGetInVehicle)
            && !HasTask(manager, TaskID::TaskComplexNewExitVehicle);
    }

    static void PedPitch(SafetyHookContext& regs)
    {
        if (!HoldPedPitch(regs.esi, regs.edi, regs.ebx))
            return;
        // Pad look normally moves the pitch-history target, then follows it. Apply that input
        // directly instead; zeroing the follow delta alone would also disable vertical pad look.
        const float pitch = *reinterpret_cast<const float*>(regs.esi + 0x304);
        const float velocity = *reinterpret_cast<const float*>(regs.esi + 0x310);
        const float timeStep = *reinterpret_cast<const float*>(regs.esp + 0x2C);
        *reinterpret_cast<float*>(regs.esp + 0x1C) = pitch;
        *reinterpret_cast<float*>(regs.esp + 0x24) = velocity * timeStep;
    }

    static void Init()
    {
        auto movement = find_pattern("56 8B F1 33 C9 8D 46 2C 8B 10 85 D2 74 ? F6 42 0C 01",
            "33 C0 8D 51 2C 56 8B 32 85 F6 74 ? F6 46 0C 01 74 ? 83 C0 01 83 C2 04 83 F8 03 7C ? 33 C0 5E C3 8B 44 81 2C 85 C0 74 ? 8B C8");
        auto caller = find_pattern("8B 88 ? ? ? ? 8D 49 ? E8 ? ? ? ? 8B 4E 04 89 44 24 0C",
            "8B 80 24 02 00 00 8D 48 44 E8 ? ? ? ? 8B 4E 04 89 44 24");
        auto activeTask = hook::pattern("68 19 04 00 00 83 C1 44 E8 ? ? ? ? 80");
        auto pitch = hook::pattern("85 DB 74 ? 80 BB 8D 32 00 00 00 74 ? F3 0F 10 86 10 03 00 00");
        GetSimplestActiveMovementTask = (decltype(GetSimplestActiveMovementTask))movement.get_first(0);
        if (!activeTask.empty())
            FindActiveTaskByType = (decltype(FindActiveTaskByType))injector::GetBranchDestination(activeTask.get_first(8)).as_int();
        else
        {
            activeTask = hook::pattern("55 56 57 33 FF 8B E9 33 C0 8D A4 24 00 00 00 00 39 7C 85 00");
            FindActiveTaskByType = (decltype(FindActiveTaskByType))activeTask.get_first(0);
        }
        intelligenceOffset = *caller.get_first<int32_t>(2);
        taskManagerOffset = *caller.get_first<uint8_t>(8);
        if (!pitch.empty())
            shPedPitch = safetyhook::create_mid(pitch.get_first(0), PedPitch);
        else
        {
            legacyLayout = true;
            pedVehicleOffset = 0xB40;
            pitch = hook::pattern("85 DB 74 ? 80 BB 8D 32 00 00 00 74 ? D9 86 10 03 00 00");
            shPedPitch = safetyhook::create_mid(pitch.get_first(0), [](SafetyHookContext& regs)
            {
                if (!HoldPedPitch(regs.esi, regs.edi, regs.ebx))
                    return;
                const float pitch = *reinterpret_cast<const float*>(regs.esi + 0x304);
                const float velocity = *reinterpret_cast<const float*>(regs.esi + 0x310);
                const float timeStep = *reinterpret_cast<const float*>(regs.esp + 0x20);
                *reinterpret_cast<float*>(regs.esp + 0x10) = pitch;
                *reinterpret_cast<float*>(regs.esp + 0x1C) = velocity * timeStep;
            });
        }
        available = static_cast<bool>(shPedPitch);
    }
}

class CenteredCam
{
public:
    static inline bool bLoadedCenteredVehicleCamIVasi = false;
    static inline bool bLoadedCenteredOnFootCamIVasi = false;

    CenteredCam()
    {
        // Register preferences on the startup thread, not in the asynchronous hook installer.
        FusionFix::onInitEvent() += CameraAutoCenter::InitializeSettings;
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");
            bool bDefaultCameraAngleInTLaD = iniReader.ReadInteger("MISC", "DefaultCameraAngleInTLaD", 1) != 0;
            bool bDisableCameraCenteringInCover = iniReader.ReadInteger("MISC", "DisableCameraCenteringInCover", 1) != 0;

            // Framing offsets
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
                nCameraUnpauseTimer1 = 4;
                nCameraUnpauseTimer2 = 4;
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
                nCameraUnpauseTimer1 = 4;
                nCameraUnpauseTimer2 = 4;
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

            // Automatic centering, delay and turn speed
            {
                CameraAutoCenter::Init();
                // Timers
                static uint32_t nTimeToPassBeforeCenteringCameraFollowPed = 0;
                static uint32_t nTimeToPassBeforeCenteringCameraFollowVehicle = 0;

                // Settings
                static auto nTimeToWaitBeforeCenteringCameraFollowPed_KB = FusionFixSettings.GetRef("PREF_KBCAMCENTERDELAY");
                static auto nTimeToWaitBeforeCenteringCameraFollowPed_Pad = FusionFixSettings.GetRef("PREF_PADCAMCENTERDELAY");

                static auto nTimeToWaitBeforeCenteringCameraFollowVehicle_KB = FusionFixSettings.GetRef("PREF_KBCAMCENTERDELAYVEH");
                static auto nTimeToWaitBeforeCenteringCameraFollowVehicle_Pad = FusionFixSettings.GetRef("PREF_PADCAMCENTERDELAYVEH");

                static auto nCameraTurnSpeedFollowVehicle_KB = FusionFixSettings.GetRef("PREF_KBCAMTURNSPEEDVEH");
                static auto nCameraTurnSpeedFollowVehicle_Pad = FusionFixSettings.GetRef("PREF_PADCAMTURNSPEEDVEH");

                static auto ShouldCenter = [&](uint32_t& DelayTime, int DelaySetting, bool IsUsingPad) -> bool
                {
                    static uint32_t LastTime = 0;
                    uint32_t CurrentTime = *CTimer::m_snTimeInMilliseconds;

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
                        const auto camera = CameraAutoCenter::legacyLayout ? regs.esi : regs.edi;
                        const auto control = CameraAutoCenter::legacyLayout ? *reinterpret_cast<const uintptr_t*>(regs.esp + 0x24) : regs.esi;
                        if (CameraAutoCenter::HoldVehicle(camera, control))
                            f = 0.0f;

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
                            if (CameraAutoCenter::HoldVehicle(regs.edi, regs.esi))
                            {
                                f = 0.0f;
                                // The same target also feeds the ground-contact pitch correction.
                                *reinterpret_cast<float*>(regs.esp + 0x20) = *reinterpret_cast<const float*>(regs.edi + 0x190);
                            }
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
                            if (CameraAutoCenter::HoldVehicle(regs.esi, *reinterpret_cast<const uintptr_t*>(regs.esp + 0x24)))
                            {
                                f = 0.0f;
                                *reinterpret_cast<float*>(regs.esp + 0x18) = *reinterpret_cast<const float*>(regs.esi + 0x190);
                            }

                            float& AutoCenterValue = *(float*)(regs.esp + 0x100 - 0x44);

                            if (ShouldCenter(nTimeToPassBeforeCenteringCameraFollowVehicle, DelaySetting, IsUsingPad))
                                AutoCenterValue = f;
                            else
                                AutoCenterValue = 0.0f;
                        });
                    }
                }
            }
        };
    }
} CenteredCam;
