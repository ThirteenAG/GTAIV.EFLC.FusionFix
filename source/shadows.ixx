module;

#include <common.hxx>

export module shadows;

import <bitset>;
import common;
import comvars;
import settings;

int32_t bExtraDynamicShadows;
std::string curModelName;
injector::memory_pointer_raw CModelInfoStore__allocateBaseModel = nullptr;
void* __cdecl CModelInfoStore__allocateBaseModelHook(char* modelName)
{
    curModelName = modelName;
    std::transform(curModelName.begin(), curModelName.end(), curModelName.begin(), [](unsigned char c) { return std::tolower(c); });
    return injector::cstd<void* (char*)>::call(CModelInfoStore__allocateBaseModel, modelName);
}
injector::memory_pointer_raw CModelInfoStore__allocateInstanceModel = nullptr;
void* __cdecl CModelInfoStore__allocateInstanceModelHook(char* modelName)
{
    curModelName = modelName;
    std::transform(curModelName.begin(), curModelName.end(), curModelName.begin(), [](unsigned char c) { return std::tolower(c); });

    if (bDynamicShadowForTrees) {
        static std::vector<std::string> treeNames = {
            "ag_bigandbushy", "ag_bigandbushygrn", "ag_tree00", "ag_tree02", "ag_tree06", "azalea_md_ingame",
            "azalea_md_ingame_05", "azalea_md_ingame_06", "azalea_md_ingame_2", "azalea_md_ingame_3",
            "azalea_md_ingame_4", "bholly_md_ingame", "bholly_md_ingame_2", "bholly_md_s_ingame",
            "bholly_md_s_ingame_2", "beech_md_ingame_2", "c_apple_md_ingame", "c_apple_md_ingame01",
            "c_apple_md_ingame_2", "c_fern_md_ingame", "c_fern_md_ingame_2", "c_fern_md_ingame_3",
            "elm_md_ingame", "elm_md_ingame_2", "h_c_md_f_ingame", "h_c_md_f_ingame_2", "l_p_sap_ingame_2",
            "liveoak_md_ingame", "liveoak_md_ingame_2", "londonp_md_ingame", "londonp_md_ingame_2",
            "mglory_c_md_ingame", "mglory_c_md_ingame_2", "pinoak_md_ingame", "pinoak_md_ingame_2",
            "scotchpine", "tree_beech1", "tree_beech2", "w_birch_md_ingame", "w_birch_md_ingame2",
            "w_birch_md_ingame_2", "w_r_cedar_md_ingame", "w_r_cedar_md_ing_2", "scotchpine2", "scotchpine4",
            "tree_redcedar", "tree_redcedar2"
        };
        if (std::any_of(std::begin(treeNames), std::end(treeNames), [](auto& i) { return i == curModelName; }))
            return injector::cstd<void* (char*)>::call(CModelInfoStore__allocateBaseModel, modelName);
    }

    return injector::cstd<void* (char*)>::call(CModelInfoStore__allocateInstanceModel, modelName);
}

std::vector<std::string> modelNames = { "track", "fence", "rail", "pillar", "post", "trn", "trk" };
injector::memory_pointer_raw CBaseModelInfo__setFlags = nullptr;
void __cdecl CBaseModelInfo__setFlagsHook(void* pModel, int dwFlags, int a3)
{
    if (bExtraDynamicShadows)
    {
        enum
        {
            flag0, flag1, alpha, flag3, flag4, trees, flag6, instance, flag8,
            enable_bone_anim, enable_uv_animation, model_hidden_shadow_casting,
            flag12, no_shadow, flag14, flag15, flag16, dynamic, flag18, flag19,
            flag20, no_backface_cull, static_shadow_1, static_shadow_2,
            flag24, flag25, enable_specialattribute, flag27, flag28,
            flag29, flag30, flag31
        };

        auto bitFlags = std::bitset<32>(dwFlags);
        if (bitFlags.test(no_shadow))
        {
            if (bExtraDynamicShadows >= 3 || std::any_of(std::begin(modelNames), std::end(modelNames), [](auto& i) { return curModelName.contains(i); }))
            {
                bitFlags.reset(no_shadow);
                bitFlags.reset(static_shadow_1);
                bitFlags.reset(static_shadow_2);
                dwFlags = static_cast<int>(bitFlags.to_ulong());
            }
        }
    }

    return injector::cstd<void(void*, int, int)>::call(CBaseModelInfo__setFlags, pModel, dwFlags, a3);
}

int GetNightShadowQuality()
{
    switch (FusionFixSettings.Get("PREF_SHADOW_DENSITY"))
    {
    case 0: //MO_OFF
        return 0;
        break;
    case 1: //MO_MED
        return 256;
        break;
    case 2: //MO_HIGH
        return 512;
        break;
    case 3: //MO_VHIGH
        return 1024;
        break;
    default:
        return 0;
        break;
    }
}

class Shadows
{
public:
    Shadows()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");

            //[SHADOWS]
            bExtraDynamicShadows = iniReader.ReadInteger("SHADOWS", "ExtraDynamicShadows", 1);
            bDynamicShadowForTrees = iniReader.ReadInteger("SHADOWS", "DynamicShadowForTrees", 1) != 0;
            bool bOverrideCascadeRanges = iniReader.ReadInteger("SHADOWS", "OverrideCascadeRanges", 1) != 0;
            bool bHighResolutionShadows = iniReader.ReadInteger("SHADOWS", "HighResolutionShadows", 0) != 0;

            if (bExtraDynamicShadows || bDynamicShadowForTrees)
            {
                auto pattern = find_pattern("E8 ? ? ? ? EB 11 8D 44 24 54", "E8 ? ? ? ? EB 0E 09 4C 24 08");
                CModelInfoStore__allocateBaseModel = injector::GetBranchDestination(pattern.get_first(0));
                injector::MakeCALL(pattern.get_first(0), CModelInfoStore__allocateBaseModelHook, true);
                pattern = find_pattern("E8 ? ? ? ? 8B F0 83 C4 04 8D 44 24 6C", "E8 ? ? ? ? 8B F0 83 C4 04 8D 54 24 6C");
                CModelInfoStore__allocateInstanceModel = injector::GetBranchDestination(pattern.get_first(0));
                injector::MakeCALL(pattern.get_first(0), CModelInfoStore__allocateInstanceModelHook, true);
                pattern = hook::pattern("D9 6C 24 0E 56");
                CBaseModelInfo__setFlags = injector::GetBranchDestination(pattern.get_first(5));
                injector::MakeCALL(pattern.get_first(5), CBaseModelInfo__setFlagsHook, true);

                std::vector<std::string> vegetationNames = {
                    "bush", "weed", "grass", "azalea", "bholly", "fern", "tree"
                };

                if (bExtraDynamicShadows == 2)
                    modelNames.insert(modelNames.end(), vegetationNames.begin(), vegetationNames.end());
            }

            if (bOverrideCascadeRanges)
            {
                struct CascadeRange
                {
                    float CascadeRange0;
                    float CascadeRange1;
                    float CascadeRange2;
                    float CascadeRange3;
                    float CascadeRange4;
                };

                auto pattern = find_pattern("8B 87 ? ? ? ? 89 04 B5 ? ? ? ? 83 FE 04 0F 8D ? ? ? ? 8D 04 8E 8B 04 85 ? ? ? ? 89 04 B5 ? ? ? ? E9 ? ? ? ? 0F 2F DC",
                                            "D9 82 ? ? ? ? D9 1C BD ? ? ? ? 0F 8D ? ? ? ? 8D 04 87 D9 04 85 ? ? ? ? D9 1C BD ? ? ? ? E9 ? ? ? ? 0F 2F C1");
                auto pCascadeRange1 = *pattern.get_first<CascadeRange*>(2);

                pattern = find_pattern("8B 87 ? ? ? ? 89 04 B5 ? ? ? ? 83 FE 04 0F 8D ? ? ? ? 8D 04 8E 8B 04 85 ? ? ? ? 89 04 B5 ? ? ? ? E9 ? ? ? ? 85 D2",
                                       "D9 82 ? ? ? ? D9 1C BD ? ? ? ? 0F 8D ? ? ? ? 8D 04 87 D9 04 85 ? ? ? ? D9 1C BD ? ? ? ? E9 ? ? ? ? 85 C9");
                auto pCascadeRange2 = *pattern.get_first<CascadeRange*>(2);

                for (size_t i = 0; i < 4; i++) // low medium high veryhigh
                {
                    injector::scoped_unprotect(&pCascadeRange1[i], sizeof(CascadeRange));
                    injector::scoped_unprotect(&pCascadeRange2[i], sizeof(CascadeRange));

                    pCascadeRange1[i].CascadeRange1 = 10.0f;
                    pCascadeRange1[i].CascadeRange2 = 30.0f;
                    pCascadeRange1[i].CascadeRange3 = 85.0f;
                    pCascadeRange1[i].CascadeRange4 = 256.0f;

                    pCascadeRange2[i].CascadeRange1 = 10.0f;
                    pCascadeRange2[i].CascadeRange2 = 30.0f;
                    pCascadeRange2[i].CascadeRange3 = 85.0f;
                    pCascadeRange2[i].CascadeRange4 = 256.0f;
                }
            }

            // Fix Cascaded Shadow Map Resolution
            {
                // Force water surface rendertarget resolution to always be 256x256. This matches the water tiling on the console version.
                static uint32_t dwWaterQuality = 1;
                auto pattern = find_pattern("8B 0D ? ? ? ? 53 BB ? ? ? ? D3 E3 85 D2 0F 85", "8B 0D ? ? ? ? BF ? ? ? ? D3 E7 85 C0 0F 85");
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
                static constexpr auto NewCascadeAtlasFormat = D3DFMT_R32F;

                pattern = find_pattern("C7 05 ? ? ? ? ? ? ? ? C7 05 ? ? ? ? ? ? ? ? 8B 08 50 FF 51 08 5E 59 C3 8B 44 24 04 6A 72", "C7 05 ? ? ? ? ? ? ? ? C7 05 ? ? ? ? ? ? ? ? 8B 08");
                injector::WriteMemory(pattern.get_first(6), rage::getEngineTextureFormat(NewCascadeAtlasFormat), true);

                // _DEFERRED_GBUFFER_0_ / _DEFERRED_GBUFFER_1_ / _DEFERRED_GBUFFER_2_
                pattern = find_pattern("BA ? ? ? ? 84 C0 0F 45 CA 8B 15", "40 05 00 00 00 8B 0D ? ? ? ? 8B 11 8B 52 38 8D 74 24 14 56 50 A1");
                injector::WriteMemory(pattern.get_first(1), rage::getEngineTextureFormat(D3DFMT_A8R8G8B8), true);

                if (bHighResolutionShadows)
                {
                    auto pattern = hook::pattern("8D 7D 40 8B 01 57 FF 75 10 FF 75 24 FF 75 0C FF 75 20 FF 75 18");
                    if (!pattern.empty())
                    {
                        static auto FixCascadedShadowMapResolution = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            auto& Width = *(uint32_t*)(regs.ebp + 0x14);
                            auto& Height = *(uint32_t*)(regs.ebp + 0x18);
                            auto& Levels = *(uint32_t*)(regs.ebp + 0x20);
                            auto& Format = *(uint32_t*)(regs.ebp + 0x24);

                            if (D3DFORMAT(Format) == NewCascadeAtlasFormat && Height >= 256 && Width == Height * 4 && Levels == 1)
                            {
                                Width *= 2;
                                Height *= 2;
                            }
                        });
                    }
                    else
                    {
                        pattern = hook::pattern("8D 4F 40 51 89 4C 24 24 8B 4F 10 51 8B 4F 24 51 8B 4F 0C 51 8B 4F 20 E9");
                        static auto FixCascadedShadowMapResolution = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            auto& Width = *(uint32_t*)(regs.edi + 0x14);
                            auto& Height = *(uint32_t*)(regs.edi + 0x18);
                            auto& Levels = *(uint32_t*)(regs.edi + 0x20);
                            auto& Format = *(uint32_t*)(regs.edi + 0x24);

                            if (D3DFORMAT(Format) == NewCascadeAtlasFormat && Height >= 256 && Width == Height * 4 && Levels == 1)
                            {
                                Width *= 2;
                                Height *= 2;
                            }
                        });
                    }
                }
                else
                {
                    // Remove code that doubled shadow cascade resolution.
                    pattern = find_pattern("03 F6 E8 ? ? ? ? 8B 0D ? ? ? ? 8D 54 24 0C", "03 F6 E8 ? ? ? ? 8B 0D ? ? ? ? 8D 44 24 0C");
                    injector::MakeNOP(pattern.get_first(0), 2, true);
                }

                // Fix night shadow resolution
                pattern = find_pattern("8B 0D ? ? ? ? 85 C9 7E 1B", "8B 0D ? ? ? ? 33 C0 85 C9 7E 1B");
                static auto shsub_925E70 = safetyhook::create_inline(pattern.get_first(0), GetNightShadowQuality);
            }
        };
    }
} Shadows;