module;

#include <common.hxx>

export module shadows;

import common;
import comvars;
import natives;
import settings;

import <bitset>;

injector::memory_pointer_raw CModelInfoStore__allocateBaseModel     = nullptr;
injector::memory_pointer_raw CModelInfoStore__allocateInstanceModel = nullptr;
injector::memory_pointer_raw CBaseModelInfo__setFlags               = nullptr;

int32_t bExtraDynamicShadows;
bool bHighResolutionNightShadows = false;

std::string curModelName;

void* __cdecl CModelInfoStore__allocateBaseModelHook(char* modelName)
{
    curModelName = modelName;
    std::transform(curModelName.begin(), curModelName.end(), curModelName.begin(), [](unsigned char c) { return std::tolower(c); });
    return injector::cstd<void* (char*)>::call(CModelInfoStore__allocateBaseModel, modelName);
}

void* __cdecl CModelInfoStore__allocateInstanceModelHook(char* modelName)
{
    curModelName = modelName;
    std::transform(curModelName.begin(), curModelName.end(), curModelName.begin(), [](unsigned char c) { return std::tolower(c); });

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
    {
        return injector::cstd<void* (char*)>::call(CModelInfoStore__allocateBaseModel, modelName);
    }

    return injector::cstd<void* (char*)>::call(CModelInfoStore__allocateInstanceModel, modelName);
}

void __cdecl CBaseModelInfo__setFlagsHook(void* pModel, int dwFlags, int a3)
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

    std::vector<std::string> modelNames = { };

    if (bExtraDynamicShadows >= 1)
    {
        std::vector<std::string> vegetationNames = {
            "bush", "weed", "grass", "azalea", "bholly", "fern", "tree"
        };

        modelNames.insert(modelNames.end(), vegetationNames.begin(), vegetationNames.end());
    }

    if (bExtraDynamicShadows >= 2)
    {
        std::vector<std::string> grateNames = {
            "rail05ax_ksun_01op", "rail09c_ksun_01op", "rail10a_ksun_01op", "bkn_ltrn_bkslin05b",
            "bkn_ltrn_bkslin04b", "bkn_ltrn_bkslin03b", "bkn_ltrn_bkslin02b", "mbridge_lt10_bkn",
            "mbridge_lt10_bknb", "rail08_ks2un_01b", "rail08a_ks2un_01b", "rail08c_ks2un_01b",
            "bkn_ltrn_qwslin02b", "bkn_ltrn_qwslin03b", "qw2_track0_d", "qw2_track1_d",
            "qw2_track2_d", "qw2_track4_d", "am_qm_fm4trkb", "am_qm_ho4trkb", "qw_track10_d",
            "qw_track9_d", "qw_track11_d", "qw_track16_d", "qw_track16b_d", "am_qm_eltrntrk4b",
            "am_qm_eltrntrk3b", "am_qm_eltrntrk2b", "am_qm_eltrntrk1b", "qw2_track7_d",
            "qw2_track6_d", "qw2_track3_d", "qw2_track5_d", "rail01_qe_01b", "rail01_qe_02b",
            "bxw_el_mesh", "bxw_el_lights2", "bxw_el_lights", "el_lights01", "el_lights02a",
            "el_lights02", "el_lights04", "bx_eltrain_5a", "bx_eltrain_6a", "bx_eltrain_7a",
            "bx_eltrain_10a",
            "bx_firescape", "fire_esc_1b", "fire_esc_2b", "fire_esc_6", "fire_esc_6b", "fire_esc_7b",
            "fire_esc_10", "fire_esc_10b",
            "fence", "cj_mh_cp_post1"
        };

        modelNames.insert(modelNames.end(), grateNames.begin(), grateNames.end());
    }

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

    return injector::cstd<void(void*, int, int)>::call(CBaseModelInfo__setFlags, pModel, dwFlags, a3);
}

int GetNightShadowQuality()
{
    static auto ShadowDensity = FusionFixSettings.GetRef("PREF_SHADOW_DENSITY");
    switch (ShadowDensity->get())
    {
        case 0: //MO_OFF
            return 0;

        case 1: //MO_MED
            return 256  * (bHighResolutionNightShadows ? 2 : 1);

        case 2: //MO_HIGH
            return 512  * (bHighResolutionNightShadows ? 2 : 1);

        case 3: //MO_VHIGH
            return 1024 * (bHighResolutionNightShadows ? 2 : 1);

        default:
            return 0;
    }
}

class Shadows
{
    static inline SafetyHookInline shsub_925DB0{};
    static int __cdecl sub_925DB0(int a1, int a2, int flags)
    {
        if (!bExtraNightShadows)
        {
            if (!Natives::IsInteriorScene())
                return -1;
        }

        return shsub_925DB0.ccall<int>(a1, a2, flags);
    }

    static inline SafetyHookInline shsub_D77A00{};
    static void __fastcall sub_D77A00(void* _this, void* edx)
    {
        if (!bExtraNightShadows)
        {
            if (!Natives::IsInteriorScene())
                return;
        }

        return shsub_D77A00.fastcall(_this, edx);
    }

public:
    Shadows()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");

            // [SHADOWS]
            bExtraDynamicShadows = iniReader.ReadInteger("SHADOWS", "ExtraDynamicShadows", 2);
            bDynamicShadowsForTrees = iniReader.ReadInteger("SHADOWS", "DynamicShadowsForTrees", 1) != 0;
            bHighResolutionShadows = iniReader.ReadInteger("SHADOWS", "HighResolutionShadows", 0) != 0;
            bHighResolutionNightShadows = iniReader.ReadInteger("SHADOWS", "HighResolutionNightShadows", 0) != 0;

            // Dynamic shadows for trees
            if (bDynamicShadowsForTrees)
            {
                auto pattern = find_pattern("E8 ? ? ? ? EB 11 8D 44 24 54", "E8 ? ? ? ? EB 0E 09 4C 24 08");
                CModelInfoStore__allocateBaseModel = injector::GetBranchDestination(pattern.get_first(0));
                injector::MakeCALL(pattern.get_first(0), CModelInfoStore__allocateBaseModelHook, true);

                pattern = find_pattern("E8 ? ? ? ? 8B F0 83 C4 04 8D 44 24 6C", "E8 ? ? ? ? 8B F0 83 C4 04 8D 54 24 6C");
                CModelInfoStore__allocateInstanceModel = injector::GetBranchDestination(pattern.get_first(0));
                injector::MakeCALL(pattern.get_first(0), CModelInfoStore__allocateInstanceModelHook, true);
            }

            // Extra dynamic shadows
            if (bExtraDynamicShadows)
            {
                auto pattern = hook::pattern("D9 6C 24 0E 56");
                CBaseModelInfo__setFlags = injector::GetBranchDestination(pattern.get_first(5));
                injector::MakeCALL(pattern.get_first(5), CBaseModelInfo__setFlagsHook, true);
            }

            // Fix cascaded shadow mapping
            {
                // Switch texture formats
                // CASCADE_ATLAS
                static constexpr auto NewCascadeAtlasFormat = D3DFMT_R32F;

                auto pattern = find_pattern("C7 05 ? ? ? ? ? ? ? ? C7 05 ? ? ? ? ? ? ? ? 8B 08 50 FF 51 08 5E 59 C3 8B 44 24 04 6A 72", "C7 05 ? ? ? ? ? ? ? ? C7 05 ? ? ? ? ? ? ? ? 8B 08");
                injector::WriteMemory(pattern.get_first(6), rage::getEngineTextureFormat(NewCascadeAtlasFormat), true);

                // _DEFERRED_GBUFFER_0_ / _DEFERRED_GBUFFER_1_ / _DEFERRED_GBUFFER_2_
                pattern = find_pattern("BA ? ? ? ? 84 C0 0F 45 CA 8B 15", "40 05 00 00 00 8B 0D ? ? ? ? 8B 11 8B 52 38 8D 74 24 14 56 50 A1");
                injector::WriteMemory(pattern.get_first(1), rage::getEngineTextureFormat(D3DFMT_A8R8G8B8), true);

                if (bHighResolutionShadows)
                {
                    pattern = hook::pattern("8D 7D 40 8B 01 57 FF 75 10 FF 75 24 FF 75 0C FF 75 20 FF 75 18");
                    if (!pattern.empty())
                    {
                        static auto FixCascadedShadowMapResolution = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            auto& Width  = *(uint32_t*)(regs.ebp + 0x14);
                            auto& Height = *(uint32_t*)(regs.ebp + 0x18);
                            auto& Levels = *(uint32_t*)(regs.ebp + 0x20);
                            auto& Format = *(uint32_t*)(regs.ebp + 0x24);

                            if (D3DFORMAT(Format) == NewCascadeAtlasFormat && Height >= 256 && Width == Height * 4 && Levels == 1)
                            {
                                Width  *= 2;
                                Height *= 2;
                            }
                        });
                    }
                    else
                    {
                        pattern = hook::pattern("8D 4F 40 51 89 4C 24 24 8B 4F 10 51 8B 4F 24 51 8B 4F 0C 51 8B 4F 20 E9");
                        static auto FixCascadedShadowMapResolution = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            auto& Width  = *(uint32_t*)(regs.edi + 0x14);
                            auto& Height = *(uint32_t*)(regs.edi + 0x18);
                            auto& Levels = *(uint32_t*)(regs.edi + 0x20);
                            auto& Format = *(uint32_t*)(regs.edi + 0x24);

                            if (D3DFORMAT(Format) == NewCascadeAtlasFormat && Height >= 256 && Width == Height * 4 && Levels == 1)
                            {
                                Width  *= 2;
                                Height *= 2;
                            }
                        });
                    }
                }
                else
                {
                    // Remove code that doubled shadow cascade resolution
                    pattern = find_pattern("03 F6 E8 ? ? ? ? 8B 0D ? ? ? ? 8D 54 24 0C", "03 F6 E8 ? ? ? ? 8B 0D ? ? ? ? 8D 44 24 0C");
                    injector::MakeNOP(pattern.get_first(0), 2, true);
                }

                // Override cascade ranges
                struct CascadeRange
                {
                    float CascadeRange0;
                    float CascadeRange1;
                    float CascadeRange2;
                    float CascadeRange3;
                    float CascadeRange4;
                };

                pattern = find_pattern("8B 87 ? ? ? ? 89 04 B5 ? ? ? ? 83 FE 04 0F 8D ? ? ? ? 8D 04 8E 8B 04 85 ? ? ? ? 89 04 B5 ? ? ? ? E9 ? ? ? ? 0F 2F DC",
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

                // Override shadow matrix
                struct ShadowMatrix
                {
                    float ShadowMatrix0;
                    float ShadowMatrix1;
                    float ShadowMatrix2;
                    float ShadowMatrix3;
                };

                pattern = find_pattern("F3 0F 10 14 85 ? ? ? ? F3 0F 10 0C 85 ? ? ? ? F3 0F 5C CA F3 0F 59 C8 F3 0F 58 CA F3 0F 11 0C B5",
                                       "F3 0F 10 94 00 ? ? ? ? F3 0F 10 9C 00 ? ? ? ? 03 C0 F3 0F 5C DA F3 0F 5E D9 F3 0F 59 D8 F3 0F 58 DA F3 0F 11 1C BD");
                auto pShadowMatrix = *pattern.get_first<ShadowMatrix*>(5);

                for (size_t i = 0; i < 4; i++) // low medium high veryhigh
                {
                    injector::scoped_unprotect(&pShadowMatrix[i], sizeof(ShadowMatrix));

                    pShadowMatrix[i].ShadowMatrix0 = 0.0f;
                    pShadowMatrix[i].ShadowMatrix1 = 0.0f;
                    pShadowMatrix[i].ShadowMatrix2 = 0.0f;
                    pShadowMatrix[i].ShadowMatrix3 = 0.0f;
                }

                // Make night shadow options adjust night shadow resolution
                pattern = find_pattern("8B 0D ? ? ? ? 85 C9 7E 1B", "8B 0D ? ? ? ? 33 C0 85 C9 7E 1B");
                static auto shsub_925E70 = safetyhook::create_inline(pattern.get_first(0), GetNightShadowQuality);

                // Lamppost shadows workaround
                {
                    auto pattern = hook::pattern("80 3D ? ? ? ? ? 75 04 83 C8 FF");
                    shsub_925DB0 = safetyhook::create_inline(pattern.get_first(), sub_925DB0);

                    pattern = find_pattern("83 EC 3C 80 3D ? ? ? ? ? 56 8B F1", "83 EC 3C 53 33 DB");
                    shsub_D77A00 = safetyhook::create_inline(pattern.get_first(0), sub_D77A00);

                    pattern = find_pattern("8B 55 20 F6 C1 06");
                    if (!pattern.empty())
                    {
                        static auto FlagsHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                        {
                            if (!bExtraNightShadows)
                            {
                                if (Natives::IsInteriorScene())
                                {
                                    if ((*(uint32_t*)(regs.edi + 0x4C) & 0x8000000) != 0) // new flag to detect affected lampposts
                                    {
                                        regs.ecx &= ~3;
                                        regs.ecx &= ~4;
                                        *(uint32_t*)(regs.esp + 0x18) = regs.ecx;
                                    }
                                }
                            }
                        });
                    }
                }
            }
        };
    }
} Shadows;