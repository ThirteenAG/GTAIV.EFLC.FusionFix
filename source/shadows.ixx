module;

#include <common.hxx>

export module shadows;

import common;
import comvars;
import settings;

import <bitset>;

injector::memory_pointer_raw CModelInfoStore__allocateBaseModel = nullptr;
injector::memory_pointer_raw CModelInfoStore__allocateInstanceModel = nullptr;
injector::memory_pointer_raw CBaseModelInfo__setFlags = nullptr;

int32_t bExtraDynamicShadows;

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

std::vector<std::string> modelNames;
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

    auto bitFlags = std::bitset<32>(dwFlags);

    if (bitFlags.test(no_shadow))
    {
        if (std::any_of(std::begin(modelNames), std::end(modelNames), [](auto& i) { return curModelName.contains(i); }))
        {
            bitFlags.reset(no_shadow);
            bitFlags.reset(static_shadow_1);
            bitFlags.reset(static_shadow_2);

            dwFlags = static_cast<int>(bitFlags.to_ulong());
        }
    }

    return injector::cstd<void(void*, int, int)>::call(CBaseModelInfo__setFlags, pModel, dwFlags, a3);
}

class Shadows
{
public:
    Shadows()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");

            // [SHADOWS]
            bExtraDynamicShadows    = iniReader.ReadInteger("SHADOWS", "ExtraDynamicShadows",    2);
            bDynamicShadowsForTrees = iniReader.ReadInteger("SHADOWS", "DynamicShadowsForTrees", 1) != 0;
            bHighResolutionShadows  = iniReader.ReadInteger("SHADOWS", "HighResolutionShadows",  0) != 0;

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
                if (bExtraDynamicShadows >= 1)
                {
                    std::vector<std::string> vegetationNames = {
                        "ag_bigandbushy", "ag_bigandbushygrn", "azalea_md_ingame", "azalea_md_ingame_06", "azalea_md_ingame_2", "bholly_md_ingame", "bholly_md_ingame_2",
                        "bholly_md_s_ingame_2", "c_fern_md_ingame_2"
                    };

                    modelNames.insert(modelNames.end(), vegetationNames.begin(), vegetationNames.end());
                }

                if (bExtraDynamicShadows >= 2)
                {
                    std::vector<std::string> miscNames = {
                        "am_qm_eltrntrk1b", "am_qm_eltrntrk2b", "am_qm_eltrntrk3b", "am_qm_eltrntrk4b", "am_qm_fm4trkb", "am_qm_ho4trkb", "bkn_ltrn_bkslin02b", "bkn_ltrn_bkslin03b",
                        "bkn_ltrn_bkslin04b", "bkn_ltrn_bkslin05b", "bkn_ltrn_qwslin02b", "bkn_ltrn_qwslin03b", "bx_eltrain_5a", "bx_eltrain_6a", "bx_eltrain_7a", "bx_eltrain_10a",
                        "bxw_el_lights", "bxw_el_lights2", "bxw_el_mesh", "el_lights01", "el_lights02", "el_lights02a", "el_lights04", "mbridge_lt10_bkn", "mbridge_lt10_bknb",
                        "qw2_track0_d", "qw2_track1_d", "qw2_track2_d", "qw2_track3_d", "qw2_track4_d", "qw2_track5_d", "qw2_track6_d", "qw2_track7_d", "qw_track9_d", "qw_track10_d",
                        "qw_track11_d", "qw_track16_d", "qw_track16b_d", "rail01_qe_01b", "rail01_qe_02b", "rail05ax_ksun_01op", "rail08_ks2un_01b", "rail08a_ks2un_01b", "rail08c_ks2un_01b",
                        "rail09c_ksun_01op", "rail10a_ksun_01op", "railtrack_02_mh5", "railtrack_03_mh5", "railtrack_04_mh5", "railtrack_05_mh5", "railtrack_06_mh5", "railtrack_07_mh5",
                        "et_oldrailbits02", "railtnnl2_dtl1_mh5", "railtrck2_dtl1_mh5", "railtrck2_dtl2_mh5", "njdock_grasstrack", "njdock_grasstrack01", "bx_firescape", "fire_esc_1",
                        "fire_esc_1b", "fire_esc_2", "fire_esc_2_steps", "fire_esc_2b", "fire_esc_4", "fire_esc_4b", "fire_esc_5", "fire_esc_5b", "fire_esc_6", "fire_esc_6b", "fire_esc_7",
                        "fire_esc_7b", "fire_esc_8", "fire_esc_8b", "fire_esc_9", "fire_esc_9b", "fire_esc_10", "fire_esc_10b", "fire_esc_11", "fire_esc_11b", "fire_esc_11c", "fire_esc_12b",
                        "ironsteps_mh12", "bxw_wii_ladders", "fence", "gatepillara_mh4", "gatepillarb_mh4", "gatepillarc_mh4", "gatepillard_mh4", "gravegates_mh10", "gravegates_mh11",
                        "hp_gate_brriers", "uni_gatea_mh4", "uni_gateb_mh4", "pedbridge2d2_mh8", "pedbridge2d_mh8", "pedbridged_mh8", "rodabridge1d_mh4", "warbridge1d_mh8", "warbridge1d2_mh8",
                        "warbridge2d_mh8", "warbridge3d_mh8", "coast_d_03_mh8", "coast_d_06_mh8", "coast_d_08_mh8", "coast_d_09_mh8", "coast_d_10_mh8", "coast_d_11_mh8", "coast_d_21_mh8",
                        "coast_d_26_mh8", "coast_d_27_mh8", "coast_d_28_mh8", "coast_d_34_mh8", "coast_d_35_mh8", "coast_d_36_mh8", "coast_d_37_mh8", "hlmcoast11d_mh12", "hlmcoastrd2_mh12",
                        "bxw_coastland8", "bxe2_cstrocks_ng", "05_01_dtl1_mh5", "05_02_dtl1_mh5", "05_03_dtl1_mh5", "05_04_dtl1_mh5", "08_bld05dtl_mh5", "24resibuild1_dtl_mh5", "resid_12-2_mh4",
                        "resid_12-5_mh4", "resi_builds8d_34_mh4", "resi_yfrontd_mh4", "bkn_bit04", "dockbrookn_01", "dockbrookn_wall", "dumbo_bstep_bkn", "dumbolisted_bkn03", "dumbofront_bkn",
                        "dumborock01_bkn", "dumborock02_bkn", "lod_tg_f_n01", "mh_roadbuit_bkn", "tg_f_n", "cj_mh_cp_post1", "concblock01_mh12", "concblock02_mh12", "concblock03_mh12",
                        "seawall_det2_mh12", "seawall_det4_mh12", "seawall_det4c_mh12", "seawall_det5_mh12", "wii_bboard", "block03det_mh10", "5thavten01det_dc9", "hk_cnrten01det_mh3",
                        "hk_crmten5x7det_mh3", "hk_crntenbrn1det_mh3", "hk_redten3x4det_mh3", "hk_redten4x4det_mh3", "soho_bank01det_mh2", "litaly_ten22det_mh2", "litaly_ten23det_mh2",
                        "litaly_tendet21_mh2", "litaly_tendet24_mh2", "litaly_tendet25_mh2", "lty2_ten01d02_mh2", "lty2_ten02d1_mh2", "lty2_ten02d2_mh2", "soho_ten12det_mh2", "soho_ten21det_mh2",
                        "tentop4_bxw", "tentop5b_bxw", "esb_roofdet01_dc9", "cityhalldet0", "coop_projektwall3", "nj04e_bbwall01", "nj04e_brdgwall03", "wallbed_1b_mh8", "wallbed_2b_mh8",
                        "wallbed_3b_mh8", "27_land01_lmap", "cc2_3_crprk_cst", "cityhall_gnd01_ns2", "cityhall_gnd02_ns2", "dm_prj_grnd04_mh7", "dm_un_hedge05_mh7", "gm-19_03", "mun_gnd01_mh2",
                        "sp_weeds_mh10", "stmys_path2", "qw_astprkgrs5", "qw_astprkgrs7", "jers_tun01_mh3", "qw2_queenstun", "nj5_priswaste03", "nj5_priswaste04", "nj5_priswaste05", "nj5priscage00",
                        "pris_baskbl2", "prispath_0", "bayrd_2_bxe", "baywindowsd_tf8", "weedbay05a", "et_piera", "et_pierb", "et_pierc", "et_pierd", "et_piere", "et_pierf", "et_pierg", "et_pierh",
                        "njdockgrd01", "njdockgrd02", "njdockgrd04", "am_ap_runwayg", "am_ap_runwayh", "am_h02_freeway", "nj03hiway00", "nj03hiway03", "ag_roads_brks02", "ag_roads_brks03",
                        "ag_roads_brks07", "tg_f_road1", "qw_road2", "qw_road3", "qw_road4", "bxe_road_14", "bxe_road_27", "bxe_road_39", "bxe_road_43", "bxe_road_57a", "bxe_road_63",
                        "bxw_road_47", "bx_ind6_jetty03_road", "bxbayroad_1", "croad", "10_road2_mh1", "10_road3_mh1", "10_road_mh1", "21_road02_mh1", "newjunt_mh10", "newroads_11_mh3",
                        "newroads_12_mh3", "newroads_13_mh3", "newroads_16_mh3", "newroads_23_mh3", "newroads_26_mh3", "newroads_32_mh3", "newroads_41_mh3", "newroads_41b_mh3", "newroads_44_mh3",
                        "road1_02_mh1", "road11_01_mh1", "road14_01_mh1", "road2_01_mh1", "road22_01_mh1", "road_02_mh2", "road_03_mh2", "road_06b_mh2", "road_08_mh10", "road_10_mh2",
                        "road_11_mh2", "road_12_mh04", "road_17_mh5", "road_20_mh04", "road_25_mh2", "road_26_mh10", "road_26b_mh10", "road_28_mh04", "road_28_mh2", "road_28_mtf8",
                        "road_29_mh04", "road_30_mh2", "road_31_mh04", "road_31_mh2", "road_31_mtf8", "road_32_mh04", "road_33_mh04", "road_33b_mh04", "road_34_mh2", "road_36_mh2",
                        "road_37_mh2", "road_37_mh5", "road_40_mh5", "road_49_mh5", "road_55_mh5", "road_56_mtf8", "road_59_mh7", "cppave09_mh8", "smordmanh03_mxr15", "ts_roads04_dc9",
                        "ts_roads27_dc9", "newj4e_road07", "newj4e_road15", "nj01_08road", "nj01_24road", "nj01_31road", "nj02road_02", "nj03hiway08", "nj03hiway09", "nj03road05", "nj03road06",
                        "nj03road12", "nj03road16", "nj03road32", "nj03road33", "nj03road51", "embankment1d_mh8", "embankment2d_mh8", "embankment4d_mh8", "embankment5d_mh8", "gas_terr_mh2",
                        "soho02_terrain_mh2", "04_terr01_mh2", "mh2_08_ter01_mh2", "mh2_08_ter05_mh2", "mh2_13_terr01_mh2", "mh2_07_terrain_mh2", "mh2_10_terrain_mh2", "blk34_terrb_mh8",
                        "nj01_13_gutter", "nj01_19_gutter1", "nj01_19_gutter2", "nj01_20_gutter1", "nj01_20_gutter2", "nj01_25_gutter1", "nj01_25_gutter2", "nj01_26_gutter", "nj01_27_gutter",
                        "nj01_32_gutter", "nj01_39_gutter", "nj01_40_gutter", "dm_roofpipe02_mh7", "mh3_07_pipes0", "tg_fway_pipes", "bxepink_grnd_bay2", "hp1_grnd", "nj5_bld4grnd", "sb2_grnd",
                        "am_ap_carpk_gnd1", "am_ap_carpk_gnd2", "et_groundsouth", "ltrain_ground", "mp_ground01_mh3", "ng_ground_top", "nj04e_ground1", "nj04e_ground2", "nj04e_ground5", "nj04e_ground6",
                        "nj04e_ground7", "nj04e_ground8", "og_bs3blk07ground", "og_bs3blk08ground", "tg_sh4_nground", "et_docketrance1", "wi1_dockbase", "wi1_docks", "base_wresd_mh4", "et_cranebase06",
                        "et_cranebase07", "esb_basedet01_dc9", "esb_basedet02_dc9", "esb_basedet03_dc9", "esb_basedet04_dc9", "tg_cibase_01", "tg_cibase_02", "tg_cibase_03", "tg_cp3_projbase", "wi_wt_base7",
                        "wi_wt_base8", "12_land01_mh5", "am_ap_carpkisland1", "am_ap_carpkisland2", "coop_projekt_land02", "coop_projekt_land03", "dmblandio", "ind1_upass_landb", "land_cc4", "nj04paulieland01",
                        "nj04paulieland02", "nj05_land03b02", "pp_stairs01rail_mh12", "facetedtowerd_tf8", "lite_towerd_mh2", "lite_towerd_mh8", "northtowerd_mh8", "plaintowerd_tf8", "tower08d_mh8",
                        "tower1d_mh4", "trumptowerd_mh4", "ts_atowerdet_dc9", "apt_corn_12_5d_ixx", "apt_corn1_10d_mh8", "aptmentsd_a2_mh4", "crnapt_uniacomd_mh4", "aprts_03d_mh4", "aprts_03d_mh4_lmap",
                        "aprts_04d_mh4", "aprt_10_24d_mh4", "rdwrks_reelixr", "ts_roads03wrks_dc9", "ts_roads21wrks_dc9", "blk23_unique1d_mh4", "blk30_unique1d_mh4", "blk30_unique2d_mh4", "cablesnposts_mh12",
                        "ks_cabled01", "ws_cc_cables02_mh7", "block03det_mh10", "hk_block02det_mh3", "hotel1d_block11_mh8", "hotel2d_block11_mh4", "hotel3d_block11_mh4", "hotel3d_block11_mh8",
                        "nj01roadblock01", "nj01roadblock03", "nj01roadblock05", "nj01roadblock39", "bilboard75corn_dc9", "nj01_bilboardframe", "ts_bilboard", "eeris_logo_bkn", "detail_0","detail_1",
                        "detail_resid61i_mh12", "lite_detail_mh8", "lite_detailb_mh8", "rope06_mh12", "rope07_mh12", "rope08_mh12", "rope09_mh12", "ap_carpark_stp_01", "ap_carpark_stp_02", "ap_carpark_stp_03",
                        "mh3_07_barbers_det", "ts_wigwamdet_01_dc9", "nj03_rubfnce_01", "ccbarrier1_mh7", "wi1_lightpoles", "et_radiomast", "nj_plumdingr", "24_chimneys1_mh04", "nathistd_mh4", "plazad2_tf8",
                        "shp_corner1d_12_mh05", "timewarnerd_mh4", "white_coma_mh4", "jumparea3_mh12", "win_1x2_dlxr", "nj04pauliebunt01", "fordham_alpha2", "sb5_alpha", "dm_un_flag01_mh7", "dm_un_flag02_mh7",
                        "aprt_7_33d_mx07", "aprts_01d_mh4", "aprts_02d_mh4", "aprts_02d2_mh4", "aprt_10_42d_mh4", "aprt_10_44d_mh4", "aprt_10_44d_mh8", "aprt_7_33d_mx07", "aprts_01d_mh4", "aprts_02d_mh4",
                        "aprts_02d2_mh4", "blk23_unique2d_mh4", "shp_corner1d_12_mh4", "shp_corner2d_12_mh4", "shp_cornerd_12a_mh4", "resi_builds9d_mh4", "tg_ciproj_balconyg", "cipblk2_ks2_00", "cipblk2_ks2_01",
                        "cipblk_ks2_02", "04_03_dtl1_mh5", "08_bld01dtl_mh5", "08_bld02dtl_mh5", "08_bld03dtl_mh5", "08_bld04dtl_mh5", "08_bld05dtl2_mh5", "10_01_dtl1_mh5", "10_02_dtl1_mh5", "10_03_dtl1_mh5",
                        "10_04_dtl1_mh5", "10_05_dtl1_mh5", "12_warehs01dtl_mh5", "13_blds1_dtl_mh5", "13_blds4_dtl_mh5", "13_blds6_dtl_mh5", "14_01_dtl1_mh5", "14_backdtls01_mh5", "15_blds1dtl1_mh5",
                        "15_blds1dtl2_mh5", "15_blds1dtl3_mh5", "15_blds4dtl1_mh5", "15_blds4dtl2_mh5", "15_blds5dtl_mh5", "16_01_dtl1_mh5", "16_01_dtl2_mh5", "16_01_dtl3_mh5", "16_02_dtl1_mh5", "16_02_dtl2_mh5",
                        "16_03_dtl1_mh5", "16_03_dtl2_mh5", "19_01_dtl1_mh5", "19_02_dtl1_mh5", "19_05_dtl1_mh5", "20_01_dtl1_mh5", "20_01_dtl2_mh5", "20_02_dtl1_mh5", "22_bld01dtl_mh5", "22_bld02dtl_mh5",
                        "22_bld03dtl_mh5", "22_bld05dtl_mh5", "24_bld01dtl1_mh5", "24_bld01dtl2_mh5", "24_bld01dtl3_mh5", "26_02_dtl2_mh5", "26_03_dtl1_mh5", "26_04_dtl1_mh5", "26_05_dtl1_mh5", "b12_bld01dtl_mh5",
                        "b12_meatfdtl_mh5", "b12_warhs02dtl_mh5", "brownstdtl01_mh5", "policehqdtl_mh10", "taino_dtl01_mh5", "taino_dtl02_mh5"
                    };

                    modelNames.insert(modelNames.end(), miscNames.begin(), miscNames.end());
                }

                if (bExtraDynamicShadows >= 3)
                {
                    std::vector<std::string> roadNames = {
                        "road", "coast", "det"
                    };

                    modelNames.insert(modelNames.end(), roadNames.begin(), roadNames.end());
                }

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

                for (size_t i = 0; i < 4; i++) // MO_LOW / MO_MED / MO_HIGH / MO_VHIGH
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

                for (size_t i = 0; i < 4; i++) // MO_LOW / MO_MED / MO_HIGH / MO_VHIGH
                {
                    injector::scoped_unprotect(&pShadowMatrix[i], sizeof(ShadowMatrix));

                    pShadowMatrix[i].ShadowMatrix0 = 0.0f;
                    pShadowMatrix[i].ShadowMatrix1 = 0.0f;
                    pShadowMatrix[i].ShadowMatrix2 = 0.0f;
                    pShadowMatrix[i].ShadowMatrix3 = 0.0f;
                }
            }
        };
    }
} Shadows;