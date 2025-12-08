module;

#include <common.hxx>
#include <concepts>

export module shaders;

import common;
import comvars;
import settings;
import natives;
import shadows;
import timecycext;
import seasonal;
import d3dx9_43;

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
    static void OnBeforeLighting()
    {
        auto pDevice = rage::grcDevice::GetD3DDevice();

        {
            static auto AzimuthColorIdx = rage::grmShaderInfo::registerShaderParam("gta_atmoscatt_clouds.fxc", "AzimuthColor");
            static auto AzimuthHeightIdx = rage::grmShaderInfo::registerShaderParam("gta_atmoscatt_clouds.fxc", "AzimuthHeight");
            auto& AzimuthColor = rage::grmShaderInfo::getShaderParamData(AzimuthColorIdx);
            auto& AzimuthHeight = rage::grmShaderInfo::getShaderParamData(AzimuthHeightIdx);

            static float arr[4];
            arr[0] = AzimuthColor[0];
            arr[1] = AzimuthColor[1];
            arr[2] = AzimuthColor[2];
            arr[3] = AzimuthHeight[0];

            pDevice->SetPixelShaderConstantF(212, &arr[0], 1);
            pDevice->SetVertexShaderConstantF(228, &arr[0], 1);
        }

        {
            static auto AzimuthColorEastIdx = rage::grmShaderInfo::registerShaderParam("gta_atmoscatt_clouds.fxc", "AzimuthColorEast");
            static auto AzimuthStrengthIdx = rage::grmShaderInfo::registerShaderParam("gta_atmoscatt_clouds.fxc", "AzimuthStrength");
            auto& AzimuthColorEast = rage::grmShaderInfo::getShaderParamData(AzimuthColorEastIdx);
            auto& AzimuthStrength = rage::grmShaderInfo::getShaderParamData(AzimuthStrengthIdx);

            static float arr[4];
            arr[0] = AzimuthColorEast[0];
            arr[1] = AzimuthColorEast[1];
            arr[2] = AzimuthColorEast[2];
            arr[3] = AzimuthStrength[0];

            pDevice->SetPixelShaderConstantF(213, &arr[0], 1);
            pDevice->SetVertexShaderConstantF(229, &arr[0], 1);
        }

        {
            static auto SkyColorIdx = rage::grmShaderInfo::registerShaderParam("gta_atmoscatt_clouds.fxc", "SkyColor");
            static auto HDRExposureIdx = rage::grmShaderInfo::registerShaderParam("gta_atmoscatt_clouds.fxc", "HDRExposure");
            auto& SkyColor = rage::grmShaderInfo::getShaderParamData(SkyColorIdx);
            auto& HDRExposure = rage::grmShaderInfo::getShaderParamData(HDRExposureIdx);

            static float arr[4];
            arr[0] = SkyColor[0];
            arr[1] = SkyColor[1];
            arr[2] = SkyColor[2];
            arr[3] = HDRExposure[0];

            pDevice->SetPixelShaderConstantF(214, &arr[0], 1);
            pDevice->SetVertexShaderConstantF(234, &arr[0], 1);
        }

        {
            static auto SunColorIdx = rage::grmShaderInfo::registerShaderParam("gta_atmoscatt_clouds.fxc", "SunColor");
            static auto HDRSunExposureIdx = rage::grmShaderInfo::registerShaderParam("gta_atmoscatt_clouds.fxc", "HDRSunExposure");
            auto& SunColor = rage::grmShaderInfo::getShaderParamData(SunColorIdx);
            auto& HDRSunExposure = rage::grmShaderInfo::getShaderParamData(HDRSunExposureIdx);

            static float arr[4];
            arr[0] = SunColor[0];
            arr[1] = SunColor[1];
            arr[2] = SunColor[2];
            arr[3] = HDRSunExposure[0];

            pDevice->SetPixelShaderConstantF(215, &arr[0], 1);
            pDevice->SetVertexShaderConstantF(231, &arr[0], 1);
        }

        {
            static auto SunDirectionIdx = rage::grmShaderInfo::registerShaderParam("gta_atmoscatt_clouds.fxc", "SunDirection");
            auto& SunDirection = rage::grmShaderInfo::getShaderParamData(SunDirectionIdx);

            static float arr[4];
            arr[0] = SunDirection[0];
            arr[1] = (-1.0f * SunDirection[2]);
            arr[2] = SunDirection[1];
            arr[3] = 0.0f;

            pDevice->SetPixelShaderConstantF(216, &arr[0], 1);
            pDevice->SetVertexShaderConstantF(232, &arr[0], 1);
        }
    }

    //static void OnBeforeGBuffer()
    //{
    //
    //}
public:
    Shaders()
    {
        static rage::grcRenderTargetPC* pHDRTexQuarter = nullptr;
        static float fOverrideTreeAlpha = 0.0f;

        static float fSHADOWFILTERSHARPShadowSoftness = 1.5f;
        static float fSHADOWFILTERSHARPShadowBias = 5.0f;

        static float fSHADOWFILTERSOFTShadowSoftness = 3.0f;
        static float fSHADOWFILTERSOFTShadowBias = 8.0f;

        static float fSHADOWFILTERCHSSShadowSoftness = 1.5f;
        static float fSHADOWFILTERCHSSShadowBias = 5.0f;
        static float fSHADOWFILTERCHSSMaxSoftness = 10.0f;

        static float fSSDensity = 0.9f;
        static float fSSDecay = 0.95f;

        static float fCascadeBlendSize = 0.1f;

        static float fMaxPQValue = 100.0f;

        static int nForceShadowFilter = 0;

        static bool bSmoothShorelines = true;

        static bool bSmoothLightVolumes = true;

        static bool bNoBloomColorShift = true;

        static auto bNoWindSway = false;

        FusionFix::onInitEvent() += []()
        {
            CIniReader iniReader("");
            fOverrideTreeAlpha = std::clamp(iniReader.ReadFloat("MISC", "OverrideTreeAlpha", 0.0f), 0.0f, 255.0f);
            fSHADOWFILTERSHARPShadowSoftness = iniReader.ReadFloat("SHADOWFILTERSHARP", "ShadowSoftness", 1.5f);
            fSHADOWFILTERSHARPShadowBias = iniReader.ReadFloat("SHADOWFILTERSHARP", "ShadowBias", 5.0f);
            fSHADOWFILTERSOFTShadowSoftness = iniReader.ReadFloat("SHADOWFILTERSOFT", "ShadowSoftness", 3.0f);
            fSHADOWFILTERSOFTShadowBias = iniReader.ReadFloat("SHADOWFILTERSOFT", "ShadowBias", 8.0f);

            fSHADOWFILTERCHSSShadowSoftness = iniReader.ReadFloat("SHADOWFILTERCHSS", "ShadowSoftness", 1.5f);
            fSHADOWFILTERCHSSShadowBias = iniReader.ReadFloat("SHADOWFILTERCHSS", "ShadowBias", 5.0f);
            fSHADOWFILTERCHSSMaxSoftness = iniReader.ReadFloat("SHADOWFILTERCHSS", "MaxSoftness", 10.0f);

            fSSDensity = std::clamp(iniReader.ReadFloat("SUNSHAFTS", "SunShaftsDensity", 0.9f), 0.0f, 1.0f);
            fSSDecay = std::clamp(iniReader.ReadFloat("SUNSHAFTS", "SunShaftsDecay", 0.95f), 0.0f, 1.0f);

            fCascadeBlendSize = std::clamp(iniReader.ReadFloat("SHADOWS", "CascadeBlendSize", 0.1f), 0.0f, 1.0f);
            nForceShadowFilter = std::clamp(iniReader.ReadInteger("SHADOWS", "ForceShadowFilter", 0), 0, 2);
            bool bConsoleCarReflectionsAndDirt = iniReader.ReadInteger("MISC", "ConsoleCarReflectionsAndDirt", 1) != 0;
            bSmoothShorelines = iniReader.ReadInteger("MISC", "SmoothShorelines", 1) != 0;
            bSmoothLightVolumes = iniReader.ReadInteger("MISC", "SmoothLightVolumes", 1) != 0;

            bNoBloomColorShift = iniReader.ReadInteger("MISC", "NoBloomColorShift", 1) != 0;
            fMaxPQValue = std::max(iniReader.ReadFloat("MISC", "MaxPQValue", 100.0f), 0.0000001f);

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

            // Redirect common\shaders\win32_30\rage_perlinnoise.fxc
            {
                std::ifstream is(GetModulePath(GetModuleHandleW(NULL)).parent_path() / "common" / "shaders" / "win32_30" / "rage_perlinnoise.fxc", std::ios::binary);
                if (is)
                {
                    static std::string rage_perlinnoise((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());

                    auto pattern = hook::pattern("A1 ? ? ? ? A3 ? ? ? ? C7 05 ? ? ? ? ? ? ? ? C3");
                    for (size_t i = 0; i < pattern.size(); ++i)
                    {
                        auto off_110ECB0 = *pattern.get(i).get<void***>(16);
                        if (!IsBadReadPtr(off_110ECB0, sizeof(uint32_t)))
                        {
                            if (!IsBadReadPtr(off_110ECB0[0], strlen("win32_30/rage_perlinnoise.fxc")))
                            {
                                auto str = std::string_view((const char*)off_110ECB0[0]);
                                if (str == "win32_30/rage_perlinnoise.fxc")
                                {
                                    injector::WriteMemory(&off_110ECB0[1], rage_perlinnoise.data(), true);
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            // Actually read the rain lighting settings in the visualsettings.dat
            {
                auto pattern = hook::pattern("8D 44 24 2C 50 FF 33 F3 0F 11 44 24 ? 56 E8");
                if (!pattern.empty())
                {
                    injector::WriteMemory<uint8_t>(pattern.get_first(3), 0x34, true);
                    pattern = hook::pattern("8D 44 24 2C 50 FF 73 04 F3 0F 11 44 24 ? 56 E8");
                    injector::WriteMemory<uint8_t>(pattern.get_first(3), 0x30, true);
                    pattern = hook::pattern("8D 44 24 2C 50 FF 73 08 F3 0F 11 44 24 ? 56 E8");
                    injector::WriteMemory<uint8_t>(pattern.get_first(3), 0x38, true);
                    pattern = hook::pattern("8D 44 24 2C 50 FF 73 0C F3 0F 11 44 24 ? 56 E8");
                    injector::WriteMemory<uint8_t>(pattern.get_first(3), 0x3C, true);
                }
                else
                {
                    pattern = hook::pattern("F3 0F 10 05 ? ? ? ? 53 56 57 8B 7C 24 10 6A 05");
                    injector::MakeNOP(pattern.get_first(0), 8, true);
                    pattern = hook::pattern("F3 0F 11 44 24 ? F3 0F 10 05 ? ? ? ? 68 ? ? ? ? F3 0F 11 44 24 ? F3 0F 10 05 ? ? ? ? 50");
                    injector::MakeNOP(pattern.get_first(0), 14, true);
                    pattern = hook::pattern("F3 0F 11 44 24 ? F3 0F 10 05 ? ? ? ? 50 F3 0F 11 44 24 ? F3 0F 10 05 ? ? ? ? 8D 5F 14");
                    injector::MakeNOP(pattern.get_first(0), 14, true);
                    pattern = hook::pattern("F3 0F 11 44 24 ? F3 0F 10 05 ? ? ? ? 8D 5F 14 53 F3 0F 11 44 24");
                    injector::MakeNOP(pattern.get_first(0), 14, true);
                    pattern = hook::pattern("F3 0F 11 44 24 ? E8 ? ? ? ? 8B 0D ? ? ? ? 8B 56 10");
                    injector::MakeNOP(pattern.get_first(0), 6, true);
                }
            }

            // Override reflection multiplier that resulted in weaker reflections on PC, a fix/workaround used to be in place on the shader side, move it in here instead.
            {
                static float dw103F984 = 1.0f; // 0.6f -> 1.0f
                auto pattern = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 59 C1 51 F3 0F 11 04 24 E8", "F3 0F 10 0D ? ? ? ? 51 F3 0F 59 C8 F3 0F 11 0C 24 E8");
                injector::WriteMemory(pattern.get_first(4), &dw103F984, true);
            }

            // Restore console car reflections and dirt level settings. Any car on console could have dirt when they would spawn while on PC some cars _always_ spawn fully cleaned.
            if (bConsoleCarReflectionsAndDirt)
            {
                auto pattern = find_pattern("75 0C C7 87 ? ? ? ? ? ? ? ? EB 20 66 0F 6E C2 F3 0F E6 C0 C1 EA 1F F2 0F 58 04 D5", "75 0D 0F 57 C0 F3 0F 11 86 ? ? ? ? EB 18 85 D2 89 54 24 10 DB 44 24 10 7D 06 D8 05");
                injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);
                pattern = find_pattern("75 37 C7 47 ? ? ? ? ? C7 87 ? ? ? ? ? ? ? ? C7 87", "75 3A F3 0F 10 05 ? ? ? ? F3 0F 11 46 ? F3 0F 11 86");
                injector::MakeNOP(pattern.get_first(0), 2, true);
            }

            // Adjust mirror plane offset, fixes stuff like the graffiti decals not appearing on the mirror in the Middle Park public toilet.
            {
                static float dwMirrorOffset = 0.0125f; // 0.035 (PC) -> 0.0125 (Xbox)
                auto pattern = find_pattern("F3 0F 10 15 ? ? ? ? 0F 28 CC F3 0F 59 CB", "F3 0F 10 0D ? ? ? ? 8B 0D ? ? ? ? 0F 28 D0 F3 0F 59 D3");
                injector::WriteMemory(pattern.get_first(4), &dwMirrorOffset, true);
            }

            // Contrast slider value is actually one tick lower internally on the Xbox 360 version (n ticks visually, actually n-1 in game code). Implement this behavior to get proper console gamma w/ FusionShaders
            {
                auto pattern = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 59 C6 F3 0F 11 4C 24", "F3 0F 10 05 ? ? ? ? F3 0F 59 C6 F3 0F 11 44 24 ? F3 0F 10 05");
                static auto PostFXContrastHook = safetyhook::create_mid(pattern.get_first(8), [](SafetyHookContext& regs)
                {
                    static auto consolegamma = FusionFixSettings.GetRef("PREF_CONSOLE_GAMMA");
                    regs.xmm0.f32[0] += regs.xmm0.f32[0] >= 1.3f ? 0.0f : (consolegamma->get() ? 0.06f : 0.0f);
                });
            }

            // Force the water surface render target resolution to always be 256x256. This matches the water tiling on the console versions.
            {
                static uint32_t dwWaterQuality = 1; // MO_MED?

                auto pattern = find_pattern("8B 0D ? ? ? ? 53 BB ? ? ? ? D3 E3 85 D2 0F 85", "8B 0D ? ? ? ? BF ? ? ? ? D3 E7 85 C0 0F 85");
                if (!pattern.empty())
                {
                    injector::WriteMemory(pattern.get_first(2), &dwWaterQuality, true); // MO_LOW?

                    pattern = find_pattern("8B 0D ? ? ? ? F3 0F 10 0D ? ? ? ? B8 ? ? ? ? D3 E0 8B 0D", "8B 0D ? ? ? ? F3 0F 10 05 ? ? ? ? 6A 02 6A 01 BA");
                    injector::WriteMemory(pattern.get_first(2), &dwWaterQuality, true); // MO_HIGH?

                    pattern = find_pattern("8B 0D ? ? ? ? BE ? ? ? ? D3 E6 83 3D", "8B 0D ? ? ? ? F3 0F 11 0D ? ? ? ? F3 0F 10 0D");
                    injector::WriteMemory(pattern.get_first(2), &dwWaterQuality, true); // MO_VHIGH?
                }
            }

            // z-fighting fix helpers
            {
                auto pattern = find_pattern("75 ? 8B CE E8 ? ? ? ? 5E 8B E5 5D C3", "? 75 ? 56 E8 ? ? ? ? 8B E5");
                static auto grcViewPortUpdateTransformHook = safetyhook::create_mid(pattern.get_first(4), [](SafetyHookContext& regs)
                {
                    auto pDevice = rage::grcDevice::GetD3DDevice();

                    if (pDevice)
                    {
                        auto viewport = rage::GetCurrentViewport();
                        if (viewport)
                        {
                            static float arr[4];
                            static float cachedNearClip = 0.0f;
                            static float cachedFarClip = 0.0f;
                            static float cachedLog2Value = 0.0f;

                            arr[0] = 1.0f / viewport->mNearClip;

                            // Only recalculate log2 if clip planes have changed
                            if (viewport->mNearClip != cachedNearClip || viewport->mFarClip != cachedFarClip)
                            {
                                cachedNearClip = viewport->mNearClip;
                                cachedFarClip = viewport->mFarClip;
                                cachedLog2Value = 1.0f / log2(viewport->mFarClip / viewport->mNearClip);
                            }

                            arr[1] = cachedLog2Value;
                            arr[2] = viewport->mFarClip / viewport->mNearClip;
                            arr[3] = viewport->mNearClip;

                            pDevice->SetVertexShaderConstantF(227, &arr[0], 1);
                            pDevice->SetPixelShaderConstantF(209, &arr[0], 1);
                        }
                    }
                });
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
                static auto dw103E49C = *find_pattern("8B 0D ? ? ? ? 8B 01 FF 50 ? B9 ? ? ? ? E9", "8B 0D ? ? ? ? 8B 11 8B 42 ? FF D0 B9").get_first<void**>(2);
                auto bLoadscreenActive = (CMenuManager::bLoadscreenShown && *CMenuManager::bLoadscreenShown) || bLoadingShown;

                if (*dw103E49C && !bLoadscreenActive)
                {
                    //static Cam cam = 0;
                    //Natives::GetRootCam(&cam);
                    //if (cam)
                    //{
                    //    static float farclip;
                    //    static float nearclip;
                    //
                    //    Natives::GetCamFarClip(cam, &farclip);
                    //    Natives::GetCamNearClip(cam, &nearclip);
                    //
                    //    static float arr[4];
                    //    arr[0] = nearclip;
                    //    arr[1] = farclip;
                    //    arr[2] = 0.0f;
                    //    arr[3] = 0.0f;
                    //    pDevice->SetVertexShaderConstantF(227, &arr[0], 1);
                    //}

                    // DynamicShadowForTrees Wind Sway
                    {
                        static float arr2[4];
                        arr2[0] = (Natives::IsInteriorScene() || bNoWindSway) ? 0.0f : *dw11A2948;
                        arr2[1] = SeasonalManager::GetCurrent() == SeasonalType::Snow ? 0.005f : std::clamp(*CTimer::fTimeScale2 * 0.015f, 0.0015f, 0.015f);
                        arr2[2] = 0.0f;
                        arr2[3] = 0.0f;
                        pDevice->SetVertexShaderConstantF(233, &arr2[0], 1);
                    }

                    // Shadow Ini Settings
                    {
                        static float arr7[4];
                        static float arr9[4];
                        static auto shadowFilter = FusionFixSettings.GetRef("PREF_SHADOWFILTER");

                        if (shadowFilter->get() == FusionFixSettings.ShadowFilterText.eSoft)
                        {
                            arr7[0] = fSHADOWFILTERSOFTShadowSoftness;
                            arr7[1] = fSHADOWFILTERSOFTShadowBias;
                        }
                        else if (shadowFilter->get() == FusionFixSettings.ShadowFilterText.eSharp)
                        {
                            arr7[0] = fSHADOWFILTERSHARPShadowSoftness;
                            arr7[1] = fSHADOWFILTERSHARPShadowBias;
                        }
                        else //eCHSS
                        {
                            arr7[0] = fSHADOWFILTERCHSSShadowSoftness;
                            arr7[1] = fSHADOWFILTERCHSSShadowBias;
                        }

                        static auto sq = FusionFixSettings.GetRef("PREF_SHADOW_QUALITY");
                        if (sq->get() >= 4) // Very High
                            arr7[2] = shadowFilter->get() == FusionFixSettings.ShadowFilterText.eCHSS ? 1.0f : 0.0f;
                        else
                            arr7[2] = 0.0f;

                        arr7[3] = fCascadeBlendSize;

                        pDevice->SetPixelShaderConstantF(218, &arr7[0], 1);

                        arr9[0] = bHighResolutionShadows ? fSHADOWFILTERCHSSMaxSoftness * 2.0f : fSHADOWFILTERCHSSMaxSoftness;
                        arr9[1] = bHighResolutionShadows ? CTimeCycleExt::GetCHSSLightSize() * 2.0f : CTimeCycleExt::GetCHSSLightSize();

                        static auto tm = FusionFixSettings.GetRef("PREF_TONEMAPPING");
                        arr9[2] = static_cast<float>(tm->get());

                        arr9[3] = bNoBloomColorShift && tm->get() ? 1.0f : 0.0f;



                        pDevice->SetPixelShaderConstantF(217, &arr9[0], 1);
                    }

                    // Water reflection half-pixel offset
                    {
                        static float arr[4];
                        static auto wq = FusionFixSettings.GetRef("PREF_WATER_QUALITY");
                        switch (wq->get())
                        {
                            case 0:
                                arr[0] = (0.5f / 160.0f);
                                arr[1] = (0.5f / 64.0f);
                                break;
                            case 1:
                                arr[0] = (0.5f / 320.0f);
                                arr[1] = (0.5f / 128.0f);
                                break;
                            case 2:
                                arr[0] = (0.5f / 640);
                                arr[1] = (0.5f / 256.0f);
                                break;
                            case 3:
                            default:
                                arr[0] = (0.5f / 1280.0f);
                                arr[1] = (0.5f / 512.0f);
                                break;
                        }

                        static auto unclamplighting = FusionFixSettings.GetRef("PREF_UNCLAMPLIGHTING");

                        arr[2] = unclamplighting->get() ? 1.0f : 4.0f / 3.0f;
                        arr[3] = unclamplighting->get() ? 0.0f : -1.0f / 3.0f;

                        pDevice->SetPixelShaderConstantF(220, &arr[0], 1);
                    }

                    // Current Settings
                    {
                        static float arr5[4];
                        static auto treealpha = FusionFixSettings.GetRef("PREF_TREEALPHA");

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

                        arr5[1] = SeasonalManager::GetCurrent() == SeasonalType::Snow ? 1.0f : 0.0f;
                        arr5[2] = fMenuBlur;

                        static float alphamul = 4.0f;
                        if (fOverrideTreeAlpha == 0.0f)
                        {
                            switch (treealpha->get())
                            {
                            case FusionFixSettings.TreeFxText.ePC: alphamul = 0.625f; break;
                            case FusionFixSettings.TreeFxText.ePCPlus: alphamul = 1.0f; break;
                            case FusionFixSettings.TreeFxText.eConsole: alphamul = 4.0f; break;
                            }
                        }
                        else
                            alphamul = fOverrideTreeAlpha;

                        arr5[3] = alphamul;
                        pDevice->SetPixelShaderConstantF(221, &arr5[0], 1);
                    }

                    // FXAA, Gamma
                    {
                        static auto gamma = FusionFixSettings.GetRef("PREF_CONSOLE_GAMMA");
                        static auto mblur = FusionFixSettings.GetRef("PREF_MOTIONBLUR");
                        static auto ae = FusionFixSettings.GetRef("PREF_AUTOEXPOSURE");
                        static float arr3[4];
                        arr3[0] = (ae->get() ? 1.0f / 9.0f : 1.0f / 16.0f);
                        arr3[1] = (bSmoothShorelines ? 1.0f : 0.0f);
                        arr3[2] = static_cast<float>(gamma->get());
                        static float mblurscale = 1.0f;
                        switch(mblur->get())
                        {
                            case 0: mblurscale = 0.0f; break;
                            case 1: mblurscale = 0.125f; break;
                            case 2: mblurscale = 0.25f; break;
                            case 3: mblurscale = 0.5f; break;
                            case 4: mblurscale = 1.0f; break;
                            default: mblurscale = 1.0f; break;
                        }
                        arr3[3] = mblurscale / (30.0f * Natives::Timestep());
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

                    {
                        static auto fog = FusionFixSettings.GetRef("PREF_VOLUMETRICFOG");

                        static float arr10[4];
                        arr10[0] = std::max(CTimeCycleExt::GetVolFogDensity(), 0.0f);
                        arr10[1] = std::max(CTimeCycleExt::GetVolFogHeightFalloff(), 0.0000001f);
                        arr10[2] = std::max(CTimeCycleExt::GetVolFogPower(), 0.0f);
                        arr10[3] = std::clamp(CTimeCycleExt::GetVolFogAltitudeTweak(), 0.0f, 1.0f);
                        pDevice->SetPixelShaderConstantF(211, &arr10[0], 1);
                        pDevice->SetVertexShaderConstantF(235, &arr10[0], 1);

                        static float arr11[4];
                        arr11[0] = 1.0f / fMaxPQValue;
                        arr11[1] = static_cast<float>(fog->get());
                        arr11[2] = bSmoothLightVolumes ? 1.0f : 0.0f;
                        arr11[3] = 0.0f;

                        if (bIsQUB3D)
                            arr11[1] = 0.0f;

                        static float arr12[4];
                        arr12[0] = CTimeCycleExt::GetDirLightColorR();
                        arr12[1] = CTimeCycleExt::GetDirLightColorG();
                        arr12[2] = CTimeCycleExt::GetDirLightColorB();
                        arr12[3] = CTimeCycleExt::GetDirLightMultiplier();

                        pDevice->SetPixelShaderConstantF(210, &arr11[0], 1);
                        pDevice->SetVertexShaderConstantF(236, &arr11[0], 1);
                        pDevice->SetVertexShaderConstantF(237, &arr12[0], 1);

                        static float arr13[4];
                        arr13[0] = std::max(CTimeCycleExt::GetSSIntensity(), 0.0f);
                        arr13[1] = fSSDensity;
                        arr13[2] = fSSDecay;
                        arr13[3] = 0.0f;

                        pDevice->SetPixelShaderConstantF(208, &arr13[0], 1);
                    }
                }
            });
        };

        FusionFix::onInitEventAsync() += []()
        {
            // Skip SetRenderState D3DRS_ADAPTIVETESS_* entirely (silences warnings in DXVK logs)
            auto pattern = find_pattern("74 24 F3 0F 10 44 24 ? 51 8D 44 24 44", "74 1C D9 44 24 14 51 8D 4C 24 44");
            injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);

            pattern = find_pattern("74 0A 6A 00 E8 ? ? ? ? 83 C4 04 8B 7C 24 1C 83 EF 80", "74 0A 6A 00 E8 ? ? ? ? 83 C4 04 8B 5C 24 20 81 C6");
            injector::WriteMemory<uint8_t>(pattern.get_first(0), 0xEB, true);

            //rain drop refraction fix
            pattern = find_pattern("E8 ? ? ? ? 8B 5E 74 8B 46 0C", "E8 ? ? ? ? 8B 4D 50 85 C9 5F 5E");
            static auto grcEffect__SetTexture = static_cast<void(__thiscall*)(uintptr_t, uintptr_t, uint32_t, rage::grcTexturePC*)>(injector::GetBranchDestination(pattern.get_first(0)).get());

            pattern = find_pattern("55 8B EC 83 E4 F8 83 EC 18 56 57 8B F9 83 7F 04 00", "55 8B EC 83 E4 F8 83 EC 10 56 8B F0");
            static auto stackSize = *pattern.get_first<uint8_t>(8);
            static auto SetCommonParticleVarsHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            {
                if (!pHDRTexQuarter)
                {
                    pHDRTexQuarter = rage::grcTextureFactoryPC::GetRTByName("Quarter Screen 0");
                }

                uintptr_t thisPtr;
                if (stackSize == 0x10)
                {
                    thisPtr = regs.eax;
                }
                else
                {
                    thisPtr = regs.ecx;
                }

                uint32_t frameMapVar = *(uint32_t*)(thisPtr + 0xA0);
                uintptr_t shaderFx = *(uintptr_t*)(thisPtr + 0x4);
                uintptr_t instanceData = shaderFx + 0x14;
                uintptr_t effect = *(uintptr_t*)(instanceData + 0x4);

                if (frameMapVar)
                {
                    grcEffect__SetTexture(effect, instanceData, frameMapVar, reinterpret_cast<rage::grcTexturePC*>(pHDRTexQuarter));
                }
            });

            FusionFix::onGameInitEvent() += []()
            {
                if (CText::hasViceCityStrings())
                {
                    bNoWindSway = true;
                }
            };

            FusionFix::onBeforeReset() += []()
            {
                pHDRTexQuarter = nullptr;
            };

            if (GetD3DX9_43DLL())
            {
                CRenderPhaseDeferredLighting_LightsToScreen::OnBuildRenderList() += []()
                {
                    auto mBeforeLightingCB = new T_CB_Generic_NoArgs(OnBeforeLighting);
                    if (mBeforeLightingCB)
                        mBeforeLightingCB->Append();
                };

                //CRenderPhaseDeferredLighting_SceneToGBuffer::OnBuildRenderList() += []()
                //{
                //    auto mBeforeGBuffer = new T_CB_Generic_NoArgs(OnBeforeGBuffer);
                //    if (mBeforeGBuffer)
                //        mBeforeGBuffer->Append();
                //};
            }
        };
    };
} Shaders;