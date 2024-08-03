module;

#include <common.hxx>
#include <concepts>

export module shaders;

import common;
import comvars;
import settings;
import natives;
import shadows;

template<typename T, typename ... U>
concept IsAnyOf = (std::same_as<T, U> || ...);

template<typename T>
using remove_cvref_t = std::remove_cvref_t<std::remove_pointer_t<std::decay_t<T>>>;

export template <typename T> requires IsAnyOf<remove_cvref_t<T>, IDirect3DPixelShader9, IDirect3DVertexShader9>
int GetFusionShaderID(T pShader)
{
    if (pShader)
    {
        UINT len = 0;
        pShader->GetFunction(nullptr, &len);
        std::vector<uint8_t> pbFunc(len + len % 4);
        pShader->GetFunction(pbFunc.data(), &len);
        static auto sFusionShader = to_bytes("FusionShader");
        auto s = pattern_str(sFusionShader);
        while (s.back() == ' ' || s.back() == '0') s.pop_back();
        auto pattern = hook::range_pattern((uintptr_t)pbFunc.data(), (uintptr_t)pbFunc.data() + pbFunc.size(), s);
        if (!pattern.empty()) {
            auto id = *pattern.get_first<int>(sFusionShader.size() - 1);
            if constexpr (IsAnyOf<remove_cvref_t<T>, IDirect3DVertexShader9>)
                if(id >= 0)
                    id -= 2000;
            return id;
        }
    }
    return -1;
}

class Shaders
{
public:
    Shaders()
    {
        static IDirect3DTexture9* pHDRTexQuarter = nullptr;
        static float fTreeAlphaMultiplier = 1.0f;
        static float fCoronaReflectionIntensity = 1.0f;

        static float fShadowSoftness = 3.0f;
        static float fShadowBias = 8.0f;
        static float fShadowBlendRange = 0.3f;

        static int nForceShadowFilter = 0;

        FusionFix::onInitEvent() += []()
        {
            CIniReader iniReader("");
            bFixAutoExposure = iniReader.ReadInteger("MISC", "FixAutoExposure", 1) != 0;
            fTreeAlphaMultiplier = std::clamp(iniReader.ReadFloat("MISC", "TreeAlphaMultiplier", 1.0f), 1.0f, 255.0f);
            fCoronaReflectionIntensity = iniReader.ReadFloat("MISC", "CoronaReflectionIntensity", 1.0f);
            fShadowSoftness = iniReader.ReadFloat("SHADOWS", "ShadowSoftness", 3.0f);
            fShadowBias = iniReader.ReadFloat("SHADOWS", "ShadowBias", 8.0f);
            fShadowBlendRange = std::clamp(iniReader.ReadFloat("SHADOWS", "ShadowBlendRange", 0.3f), 0.0f, 1.0f);
            nForceShadowFilter = std::clamp(iniReader.ReadInteger("SHADOWS", "ForceShadowFilter", 0), 0, 2);

            // Redirect path to one unified folder
            auto pattern = hook::pattern("8B 04 8D ? ? ? ? A3 ? ? ? ? 8B 44 24 04");
            if (!pattern.empty())
            {
                static auto off_1045520 = *pattern.get_first<const char**>(3);
                struct ShaderPathHook
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        regs.ecx = 0;
                        *(const char**)&regs.eax = *off_1045520;
                    }
                }; injector::MakeInline<ShaderPathHook>(pattern.get_first(0), pattern.get_first(7));
            }
            else
            {
                pattern = hook::pattern("8B 14 85 ? ? ? ? A3 ? ? ? ? 8B 44 24 04");
                static auto off_1045520 = *pattern.get_first<const char**>(3);
                struct ShaderPathHook
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        regs.eax = 0;
                        *(const char**)&regs.edx = *off_1045520;
                    }
                }; injector::MakeInline<ShaderPathHook>(pattern.get_first(0), pattern.get_first(7));
            }
        };

        FusionFix::onGameInitEvent() += []()
        {
            auto pattern = hook::pattern("80 7C 24 ? ? 74 3F 80 BE ? ? ? ? ? 74 36");
            static auto BeginSceneHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            {
                auto pDevice = rage::grcDevice::GetD3DDevice();

                // Setup variables for shaders
                static auto dw11A2948 = *find_pattern("C7 05 ? ? ? ? ? ? ? ? 0F 85 ? ? ? ? 6A 00", "D8 05 ? ? ? ? D9 1D ? ? ? ? 83 05").get_first<float*>(2);
                static auto dw103E49C = *hook::get_pattern<void**>("A3 ? ? ? ? C7 80", 1);
                auto bLoadscreenActive = (CMenuManager::bLoadscreenShown && *CMenuManager::bLoadscreenShown) || bLoadingShown;

                if (*dw103E49C && !bLoadscreenActive)
                {
                    static Cam cam = 0;
                    Natives::GetRootCam(&cam);
                    if (cam)
                    {
                        static float farclip;
                        static float nearclip;

                        Natives::GetCamFarClip(cam, &farclip);
                        Natives::GetCamNearClip(cam, &nearclip);

                        static float arr[4];
                        arr[0] = nearclip;
                        arr[1] = farclip;
                        arr[2] = 0.0f;
                        arr[3] = 0.0f;
                        pDevice->SetVertexShaderConstantF(227, &arr[0], 1);
                    }

                    // DynamicShadowForTrees Wind Sway
                    {
                        static float arr2[4];
                        arr2[0] = Natives::IsInteriorScene() ? 0.0f : *dw11A2948;
                        arr2[1] = bEnableSnow ? 0.005f : 0.015f;
                        arr2[2] = fCoronaReflectionIntensity;
                        arr2[3] = 0.0f;
                        pDevice->SetVertexShaderConstantF(233, &arr2[0], 1);
                    }

                    // Shadow Ini Settings
                    {
                        static float arr7[4];

                        arr7[0] = fShadowSoftness;
                        arr7[1] = fShadowBias;
                        arr7[2] = fShadowBlendRange;
                        arr7[3] = bEnableSnow ? 1.0f : 0.0f;

                        pDevice->SetPixelShaderConstantF(218, &arr7[0], 1);
                    }

                    // Shadow Quality
                    {
                        static float arr6[4];

                        switch (FusionFixSettings.Get("PREF_SHADOW_QUALITY"))
                        {
                        case 0:
                            arr6[0] = 0.0f;
                            arr6[1] = 0.0f;
                            arr6[2] = 0.0f;
                            arr6[3] = 0.0f;
                            break;
                        case 1:
                            arr6[0] = 0.0f;
                            arr6[1] = 0.0f;
                            arr6[2] = 0.0f;
                            arr6[3] = 1.0f;
                            break;
                        case 2:
                            arr6[0] = 0.0f;
                            arr6[1] = 0.0f;
                            arr6[2] = 1.0f;
                            arr6[3] = 0.0f;
                            break;
                        case 3:
                            arr6[0] = 0.0f;
                            arr6[1] = 1.0f;
                            arr6[2] = 0.0f;
                            arr6[3] = 0.0f;
                            break;
                        case 4:
                        default:
                            arr6[0] = 1.0f;
                            arr6[1] = 0.0f;
                            arr6[2] = 0.0f;
                            arr6[3] = 0.0f;
                            break;
                        }

                        pDevice->SetPixelShaderConstantF(220, &arr6[0], 1);
                    }

                    // Current Settings
                    {
                        static float arr5[4];

                        if (nForceShadowFilter == 0)
                        {
                            static auto definition = FusionFixSettings.GetRef("PREF_DEFINITION");
                            arr5[0] = static_cast<float>(definition->get());
                        }
                        else if (nForceShadowFilter == 1)
                        {
                            arr5[0] = 0.0f;
                        }
                        else if (nForceShadowFilter == 2)
                        {
                            arr5[0] = 1.0f;
                        }

                        arr5[1] = 0.0f;
                        arr5[2] = 1.0f / (30.0f * Natives::Timestep());
                        arr5[3] = fTreeAlphaMultiplier;
                        pDevice->SetPixelShaderConstantF(221, &arr5[0], 1);
                    }

                    // FXAA, Gamma
                    {
                        static auto gamma = FusionFixSettings.GetRef("PREF_CONSOLE_GAMMA");
                        static auto mblur = FusionFixSettings.GetRef("PREF_MOTIONBLUR");
                        static float arr3[4];
                        arr3[0] = (bFixAutoExposure ? 1.0f : 0.0f);
                        arr3[1] = 0.0f;
                        arr3[2] = static_cast<float>(gamma->get());
                        arr3[3] = static_cast<float>(mblur->get());
                        pDevice->SetPixelShaderConstantF(222, &arr3[0], 1);
                    }

                    // Cascaded Shadow Map Res, Time of Day, Tree Translucency
                    {
                        static Cam cam = 0;
                        Natives::GetRootCam(&cam);
                        if (cam)
                        {
                            float fov = 0.0f;
                            uint32_t hour = -1;
                            uint32_t minute = -1;
                            Natives::GetTimeOfDay(&hour, &minute);
                            Natives::GetCamFov(cam, &fov);
                            static auto tree_lighting = FusionFixSettings.GetRef("PREF_TREE_LIGHTING");
                            static auto shadowFilter = FusionFixSettings.GetRef("PREF_SHADOW_FILTER");
                            static auto definition = FusionFixSettings.GetRef("PREF_DEFINITION");
                            static float arr4[4];
                            arr4[0] = static_cast<float>(tree_lighting->get() - FusionFixSettings.TreeFxText.ePC);
                            arr4[1] = (fov > 45.0f) ? pow((fov / 45.0f), 1.25f) : 1.0f;
                            arr4[2] = static_cast<float>(definition->get());
                            arr4[3] = (((hour == 6 && minute >= 45) || (hour > 6)) && ((hour == 19 && minute < 15) || (hour < 19))) ? 0.0f : 1.0f;
                            pDevice->SetPixelShaderConstantF(223, &arr4[0], 1);
                        }
                    }
                }
            });
        };

        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");
            static auto bFixRainDrops = iniReader.ReadInteger("MISC", "FixRainDrops", 1) != 0;

            //SetRenderState D3DRS_ADAPTIVETESS_X
            auto pattern = hook::pattern("74 ? 68 4E 56 44 42 68");
            injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);

            pattern = find_pattern<2>("89 3C B5 ? ? ? ? 8B 82 ? ? ? ? 57 8B 08 56 50 FF 91 ? ? ? ? 5F 5E C2 0C 00", "89 14 8D ? ? ? ? 8B 80 ? ? ? ? 8B 30 52 8B 96 ? ? ? ? 51 50 FF D2 5E C2 0C 00");
            static auto reg = *pattern.get(1).get<uint8_t>(1);
            static auto SetTextureHook = safetyhook::create_mid(pattern.get(1).get<void>(0), [](SafetyHookContext& regs)
            {
                if (bFixRainDrops && ((reg == 0x3C && regs.esi == 1 && regs.edi == 0) || (reg != 0x3C && regs.ecx == 1 && regs.edx == 0)))
                {
                    if (!pHDRTexQuarter)
                    {
                        auto qs0 = rage::grcTextureFactoryPC::GetRTByName("Quarter Screen 0");
                        if (qs0)
                            pHDRTexQuarter = qs0->mD3DTexture;
                    }
                
                    if (pHDRTexQuarter)
                    {
                        if (reg == 0x3C)
                            regs.edi = (uintptr_t)pHDRTexQuarter;
                        else
                            regs.edx = (uintptr_t)pHDRTexQuarter;
                    }
                }
            });

            FusionFix::onBeforeReset() += []()
            {
                pHDRTexQuarter = nullptr;
            };
        };
    };
} Shaders;