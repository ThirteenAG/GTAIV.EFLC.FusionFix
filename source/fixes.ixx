module;

#include <common.hxx>

export module fixes;

import common;
import comvars;
import settings;
import natives;
import shaders;
import fusiondxhook;

class Fixes
{
public:
    static inline int32_t nTimeToPassBeforeCenteringCameraOnFoot = 0;
    static inline bool bWaitBeforeCenteringCameraOnFootUsingPad = false;

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

    Fixes()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");

            int32_t nAimingZoomFix = iniReader.ReadInteger("MAIN", "AimingZoomFix", 1);
            bool bRecoilFix = iniReader.ReadInteger("MAIN", "RecoilFix", 1) != 0;

            //[MISC]
            bool bDefaultCameraAngleInTLAD = iniReader.ReadInteger("MISC", "DefaultCameraAngleInTLAD", 0) != 0;
            bool bPedDeathAnimFixFromTBOGT = iniReader.ReadInteger("MISC", "PedDeathAnimFixFromTBOGT", 1) != 0;

            //[SHADOWS]
            bool bHighResolutionShadows = iniReader.ReadInteger("SHADOWS", "HighResolutionShadows", 0) != 0;

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

            if (bDefaultCameraAngleInTLAD)
            {
                static uint32_t episode_id = 0;
                auto pattern = find_pattern<2>("83 3D ? ? ? ? ? 8B 01 0F 44 C2 89 01 B0 01 C2 08 00", "83 3D ? ? ? ? ? 75 06 C7 00 ? ? ? ? B0 01 C2 08 00");
                injector::WriteMemory(pattern.count(2).get(0).get<void>(2), &episode_id, true);
            }

            if (bPedDeathAnimFixFromTBOGT)
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
                static constexpr float xmm_0 = FLT_MAX / 2.0f;
                unsigned char bytes[4];
                auto n = (uint32_t)&xmm_0;
                bytes[0] = (n >> 24) & 0xFF;
                bytes[1] = (n >> 16) & 0xFF;
                bytes[2] = (n >> 8) & 0xFF;
                bytes[3] = (n >> 0) & 0xFF;

                auto pattern = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 58 47 ? F3 0F 11 47 ? 8B D1 89 54 24 10", "F3 0F 10 05 ? ? ? ? F3 0F 58 46 ? 89 8C 24");
                static raw_mem CoverCB(pattern.get_first(4), { bytes[3], bytes[2], bytes[1], bytes[0] });
                FusionFixSettings.SetCallback("PREF_COVERCENTERING", [](int32_t value) {
                    if (value)
                        CoverCB.Restore();
                    else
                        CoverCB.Write();
                });
                if (!FusionFixSettings("PREF_COVERCENTERING"))
                    CoverCB.Write();
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
                            nTimeToPassBeforeCenteringCameraOnFoot = *CTimer::m_snTimeInMilliseconds + ((pad ? nTimeToWaitBeforeCenteringCameraOnFootPad->get() : nTimeToWaitBeforeCenteringCameraOnFootKB->get()) * 1000);

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
                struct MouseHeightHook 
                {
                    void operator()(injector::reg_pack& regs) 
                    {
                        *(float*)(regs.esp + 0x40 - 0x24) = 30.0f * (1.0f / 768.0f);
                    }
                };
                if (!pattern.empty())
                    injector::MakeInline<MouseHeightHook>(pattern.get_first(0), pattern.get_first(6));

                pattern = hook::pattern("F3 0F 11 44 24 ? FF 50 24 66 0F 6E C0 0F 5B C0 6A 01");
                struct MouseWidthHook 
                {
                    void operator()(injector::reg_pack& regs) 
                    {
                        *(float*)(regs.esp + 0x10) = 30.0f * (1.0f / 1024.0f);
                    }
                };
                if (!pattern.empty())
                    injector::MakeInline<MouseWidthHook>(pattern.get_first(0), pattern.get_first(6));
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

            // Glass Shards Color Fix
            {
                static auto veh_glass_red = "veh_glass_red";
                static auto veh_glass_amber = "veh_glass_amber";

                auto pattern = hook::pattern("68 ? ? ? ? EB E2 6A 00 68");
                if (!pattern.empty())
                    injector::WriteMemory(pattern.get_first(1), &veh_glass_red[0], true);

                pattern = hook::pattern("68 ? ? ? ? E8 ? ? ? ? 83 C4 08 89 44 24 0C 6A 00 6A 00");
                if (!pattern.empty())
                    injector::WriteMemory(pattern.get_first(1), &veh_glass_amber[0], true);
            }

            // Fix Cascaded Shadow Map Resolution
            {
                // Clamp night shadow resolution to 512x512 @ Very High (was 1024x1024)
                auto pattern = hook::pattern("83 3D ? ? ? ? ? 7E 12 B8 ? ? ? ? D3 E0 B9 ? ? ? ? 3B C1 0F 4F C1 C3");
                if (!pattern.empty())
                    injector::WriteMemory(pattern.get_first(17), 0x200, true);
                else
                {
                    pattern = hook::pattern("3D ? ? ? ? 7E 05 B8 ? ? ? ? C3");
                    injector::WriteMemory(pattern.count(2).get(0).get<void*>(1), 0x200, true);
                    injector::WriteMemory(pattern.count(2).get(0).get<void*>(8), 0x200, true);
                }

                // Remove code that doubled shadow cascade resolution.
                pattern = find_pattern("03 F6 E8 ? ? ? ? 8B 0D ? ? ? ? 8D 54 24 0C", "03 F6 E8 ? ? ? ? 8B 0D ? ? ? ? 8D 44 24 0C");
                injector::MakeNOP(pattern.get_first(0), 2, true);

                // Force water surface rendertarget resolution to always be 256x256. This matches the water tiling on the console version.
                static uint32_t dwWaterQuality = 1;
                pattern = find_pattern("8B 0D ? ? ? ? 53 BB ? ? ? ? D3 E3 85 D2 0F 85", "8B 0D ? ? ? ? BF ? ? ? ? D3 E7 85 C0 0F 85");
                if (!pattern.empty())
                {
                    injector::WriteMemory(pattern.get_first(2), &dwWaterQuality, true);
                    pattern = find_pattern("8B 0D ? ? ? ? F3 0F 10 0D ? ? ? ? B8 ? ? ? ? D3 E0 8B 0D", "8B 0D ? ? ? ? F3 0F 10 05 ? ? ? ? 6A 02 6A 01 BA");
                    injector::WriteMemory(pattern.get_first(2), &dwWaterQuality, true);
                    pattern = find_pattern("8B 0D ? ? ? ? BE ? ? ? ? D3 E6 83 3D", "8B 0D ? ? ? ? F3 0F 11 0D ? ? ? ? F3 0F 10 0D");
                    injector::WriteMemory(pattern.get_first(2), &dwWaterQuality, true);
                }

                // Switch texture formats
                // CASCADE_ATLAS
                pattern = find_pattern("C7 05 ? ? ? ? ? ? ? ? C7 05 ? ? ? ? ? ? ? ? 8B 08 50 FF 51 08 5E 59 C3 8B 44 24 04 6A 72", "C7 05 ? ? ? ? ? ? ? ? C7 05 ? ? ? ? ? ? ? ? 8B 08");
                injector::WriteMemory(pattern.get_first(6), rage::getEngineTextureFormat(D3DFMT_R32F), true);

                // _DEFERRED_GBUFFER_0_ / _DEFERRED_GBUFFER_1_ / _DEFERRED_GBUFFER_2_
                pattern = find_pattern("BA ? ? ? ? 84 C0 0F 45 CA 8B 15", "40 05 00 00 00 8B 0D ? ? ? ? 8B 11 8B 52 38 8D 74 24 14 56 50 A1");
                injector::WriteMemory(pattern.get_first(1), rage::getEngineTextureFormat(D3DFMT_A8R8G8B8), true);
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
                static uintptr_t loc_6E39F3 = (uintptr_t)find_pattern("8B 45 0C 8B 4C 24 18 33 F6 33 D2 89 74 24 1C 66 3B 54 C1", "8B 4D 0C 66 83 7C CE").get_first(0);
                struct EmissiveDepthWriteHook
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        // Fix for visual bugs in QUB3D that will occur with this fix. Only enable z-write for emissives shaders when the camera/player height is 3000 or higher.
                        // This height check was present in patch 1.0.4.0 and was removed in patch 1.0.6.0+, in addition Z-write for emissive shaders was enabled permanently.
                        if ((regs.edi == 5 || regs.edi == 4) && *(float*)(*dwEFB1B8 + 296) < 3000.0f)
                        {
                        }
                        else
                        {
                            *(uintptr_t*)(regs.esp - 4) = loc_6E39F3;
                        }
                    }
                }; injector::MakeInline<EmissiveDepthWriteHook>(pattern.get_first(0), pattern.get_first(10));

                pattern = find_pattern("6A 01 8B C8 E8 ? ? ? ? EB 02 33 C0 50 E8 ? ? ? ? 83 C4 04 8B 45 0C 8B 4C 24 18", "6A 01 8B C8 E8 ? ? ? ? EB 02 33 C0 8B C8 E8 ? ? ? ? 8B 4D 0C");
                injector::WriteMemory<uint8_t>(pattern.get_first(1), 0, true);
            }

            // Water Foam Height Weirdness
            {
                auto pattern = hook::pattern("F3 0F 58 0D ? ? ? ? 83 EC 08 F3 0F 59 05");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(0), 8, true);
            }
        };
    }
} Fixes;