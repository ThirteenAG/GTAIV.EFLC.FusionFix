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

rage::grcRenderTargetPC** pCascadeAtlas = nullptr;

// The atlas holds the 4 cascades side by side. Every shadow tap is clamped half a texel inside its own tile, so the
// bilinear depth compare never reads the neighbouring cascade: c203 = upper bound, c204 = lower bound per cascade.
export void SetCascadeTileBounds(IDirect3DDevice9* pDevice)
{
    if (!pCascadeAtlas || !*pCascadeAtlas)
        return;

    static uint16_t width = 0;
    static float bounds[2][4];
    if ((*pCascadeAtlas)->mWidth != width)
    {
        width = (*pCascadeAtlas)->mWidth;
        float halfTexel = 0.5f / width;
        bounds[0][0] = 0.25f - halfTexel;
        bounds[0][1] = 0.5f - halfTexel;
        bounds[0][2] = 0.75f - halfTexel;
        bounds[0][3] = 1.0f;
        bounds[1][0] = 0.0f;
        bounds[1][1] = 0.25f + halfTexel;
        bounds[1][2] = 0.5f + halfTexel;
        bounds[1][3] = 0.75f + halfTexel;
    }

    pDevice->SetPixelShaderConstantF(203, &bounds[0][0], 2);
}

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
            SettingsReader reader;

            // [SHADOWS]
            bExtraDynamicShadows    = reader.ReadInteger("SHADOWS", "ExtraDynamicShadows",    2);
            bDynamicShadowsForTrees = reader.ReadInteger("SHADOWS", "DynamicShadowsForTrees", 1) != 0;
            bHighResolutionShadows  = reader.ReadInteger("SHADOWS", "HighResolutionShadows",  0) != 0;

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
                        "b12_meatfdtl_mh5", "b12_warhs02dtl_mh5", "brownstdtl01_mh5", "policehqdtl_mh10", "taino_dtl01_mh5", "taino_dtl02_mh5", "metbanns_mh06", "mus_skylightmh06", "nj03ac13veg", "nj03ac2041",
                        "nj03ac2312", "nj03ac23alph3", "nj03ac24plnts", "nj03ac2912alph", "nj03ac2916", "nj03ac3208", "nj03ac3442grs", "nj03ac36alph", "nj04e_cstglue", "nj04e_glue01", "nj04e_glue02",
                        "nj04e_glue05", "nj04e_glue06", "nj04e_glue07", "nj04e_glue09", "nj04e_glue10", "nj04e_glue11", "nj04e_glue12", "nj04e_glue69", "nj04e_indbuild02", "nj04e_indbuild33", "nj04e_indbuild35",
                        "nj04e_indbuild36", "nj04e_indbuild38", "nj04eleaf_pile03", "nj04paulieweed01", "xnj2_cd30_01alley", "14_alley_mh1", "bxwsb8_alley", "nj_alley1", "swn04_blokalley", "ag_bushytree11", 
                        "ag_bushytree4", "ag_bushytree5", "ag_bushytree6", "ag_bushytree7", "bknbush_02", "et_bushlib02", "tr_dstore_det01_dc9", "ts_bawsaqdet_02_dc9", "ts_bilb8x14_dc9", "ts_bilbixrtile01_dc9",
                        "ts_bilbstand1_dc9", "ts_bilcorn01_dc9", "ts_floodlitixr_dc9", "ts_floodlitixr2_dc9", "ts_floodlitixr3_dc9", "ts_roads03x_dc9", "ts_roads07_dc9", "ts_roads08_dc9", "ts_roads09x_dc9",
                        "ts_roads12_dc9", "ts_roads14_dc9", "ts_roads15_dc9", "ts_roads18_dc9", "ts_roads19_dc9", "ts_roads21_dc9", "ts_roads22_dc9", "ts_roads23_dc9", "ts_roads29_dc9", "ts_roads31_dc9",
                        "ts_roads32_dc9", "ts_roads58_dc9", "ts_roads60_dc9", "ts_rtheat1_det_dc9", "intbild1_d_mh12", "intbild2_d_mh12", "rez_bild8_33d_ix06", "bxw_coastfortwds", "bxw_constalpha",
                        "sb4b_alpha", "sb6_glues", "sb7_alpha", "sb8_wires", "sbx4_weeds", "percyshpsd_mh8", "bc_clam_alpha", "bxwmanny_alpha", "hp_2_plants_alpha03", "hp1_alpha", "nj01_casino_alpha",
                        "plant_weeds_bxe_e02", "plant_weeds_bxe_e03", "bkn_wi3_sigs2", "dmb_brokerd_bkn", "dmb_midg02_bkn", "hunts_p03_market", "03_waredet", "bm_aerial", "road_13_mh7",
                        "bxe_road_40", "bxe_road_54", "bxe_road_66", "bxe_road_81", "beacon1_mh8", "beacon2_mh8", "bears_lbayalfa", "hk_slumber01det_mh3", "nj05_bridge_ivy", "nj05_ivy_test",
                        "nj05_plntgngat01", "pave_treehole4a_mxr", "brown_accomd_mh4", "cc1_gardn_01a", "b4_gcdet02_mh7", "blk02_05det_mh10", "blk07_03det_mh10", "blk10_01det_mh10", "blk10_06det_mh10",
                        "road_qe_21", "agld04_03", "municipal_det17_mh2", "tg_cimidrf", "qm_fh_hedge", "et_firestation01", "gastation1d_mh8", "cop_det01_mh2", "coptrain_det", "coop_police_shops2",
                        "qw_nwcost", "qw2_lic7", "bigfish_d_mh10", "gm_16_09", "gm_16_24", "gm-08_07", "gm-19_09wire02", "nj5_gm12_ivy", "qw_drainpip", "newj_indbuild43", "newsky_det_mh7",
                        "nj5_plat1_hid", "nj5derlct04b", "nj5sport", "dm_hoteldet02_mh7", "dm_hoteldet12_mh7", "hospitald1_mh8", "ld03_glue", "dm_un_cordn_mh7", "ld04_11", "dm_un_det02_mh7",
                        "blck02_det010_mh7", "blck02_det01_mh7", "blck02_det02_mh7", "blck11_det02_mh7", "blck12_det05_mh7", "chrydet03_mh7", "dm_hotel_det11", "sp_tudor_03det_mh7", "sp_tudor_07det_mh7",
                        "mh7_1det02_mh7", "dm_wooddet02_mh7", "dm_wooddet03_mh7", "hk_rdcrntn01det_mh3"
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

                // 1.2.0.32, 1.2.0.43, 1.2.0.59 or 1.0.7.0, 1.0.8.0, 1.1.2.0, 1.1.3.0: the code that differs between them
                bool bCE = !hook::pattern("83 EC ? 53 8B 5C 24 ? 56 8B 74 24 ? 57 8B 7C 24 ? 39 35").empty();

                // The cascade targets were twice the tile size, to be downsampled into the atlas; the cascades render
                // straight into the atlas now (below)
                pattern = hook::pattern(bCE ? "03 F6 E8 ? ? ? ? 8B 0D ? ? ? ? 8D 54 24 0C" : "03 F6 E8 ? ? ? ? 8B 0D ? ? ? ? 8D 44 24 0C");
                injector::MakeNOP(pattern.get_first(0), 2, true);

                if (bHighResolutionShadows)
                {
                    // Tile size argument of CWarpShadow::CreateRenderTargets
                    pattern = hook::pattern(bCE ? "83 EC ? 53 8B 5C 24 ? 56 8B 74 24 ? 57 8B 7C 24 ? 39 35" : "83 EC 30 53 8B 5C 24 ? 56 8B 74 24 ? 39 35 ? ? ? ? 57 8B 7C 24 ? 75");
                    static auto DoubleCascadeTileSize = safetyhook::create_mid(pattern.get_first(13), [](SafetyHookContext& regs)
                    {
                        regs.esi *= 2;
                    });
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

                // Render each cascade straight into its atlas tile with a sampleable depth buffer: the shaders compare
                // depth in hardware (bilinear PCF), the raw depth colour target is only kept for CHSS's blocker search
                {
                    pattern = find_pattern("89 35 ? ? ? ? 6A ? 8D 4C 24");
                    static int& shadowRes = **pattern.get_first<int*>(2); // tile size, the atlas is 4 tiles wide

                    // Copy of the cascade target into the atlas, not needed anymore
                    pattern = hook::pattern(bCE ? "83 EC ? 56 8B 35 ? ? ? ? 57 89 74 24" : "83 EC 34 53 55 56 8B 35 ? ? ? ? 57 E8");
                    injector::MakeRET(pattern.get_first());

                    // Shadow texel size from the tile, not from the old cascade target
                    pattern = hook::pattern(bCE ? "FF D0 F3 0F 10 0D ? ? ? ? 8B 7C 24" : "8B 42 ? 0F 28 C1 F3 0F 5E C2 F3 0F 2A 15 ? ? ? ? F3 0F 11 44 24 ? 0F 28 C1 F3 0F 5E C2 F3 0F 11 44 24 ? FF D0");
                    auto getShadowWidth = pattern.get_first(bCE ? 0 : 0x25);
                    injector::MakeNOP(getShadowWidth, 2);
                    static auto getShadowWidthHook = safetyhook::create_mid(getShadowWidth, [](SafetyHookContext& regs)
                    {
                        regs.eax = shadowRes;
                    });

                    pattern = hook::pattern(bCE ? "68 ? ? ? ? FF 50 ? A3 ? ? ? ? 5F" : "68 ? ? ? ? FF D2 A3 ? ? ? ? 5F 5E 5B");
                    static auto changeAtlasFormatHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                    {
                        // CreateRenderTarget arguments: 3, width, height, 32, desc
                        auto atlasTexDesc = *(rage::grcRenderTargetDesc**)(regs.esp + 0x10);
                        atlasTexDesc->mFormat = rage::GRCFMT_D24S8;
                    });

                    pattern = find_pattern("A3 ? ? ? ? 5F 5E 5B 83 C4");
                    pCascadeAtlas = *pattern.get_first<rage::grcRenderTargetPC**>(1);

                    // The atlas is the depth target of the cascade pass
                    pattern = hook::pattern(bCE ? "FF 35 ? ? ? ? 8B C8 FF 35 ? ? ? ? 6A ? E8 ? ? ? ? EB ? 33 C0 50 E8 ? ? ? ? 68" : "8B 15 ? ? ? ? 8B 0D ? ? ? ? 53 52 51 53 8B C8 E8 ? ? ? ? EB ? 33 C0 8B C8 E8 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 8B 97 38 09 00 00");
                    injector::WriteMemory(pattern.get_first(2), pCascadeAtlas, true);

                    // CASCADE_TARGET_R32, the colour target, is as large as the atlas
                    pattern = hook::pattern(bCE ? "56 A3 ? ? ? ? C6 44 24" : "56 A3 ? ? ? ? 88 5C 24 ? C7 44 24");
                    static auto atlasWidthHook1 = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                    {
                        regs.esi *= 4;
                    });
                    static auto atlasWidthHook2 = safetyhook::create_mid(pattern.get_first(6), [](SafetyHookContext& regs)
                    {
                        regs.esi /= 4;
                    });

                    pattern = hook::pattern(bCE ? "83 EC ? 8B 44 24 ? 8B 54 24 ? 56" : "83 EC 0C 8B 44 24 1C 53 8B 5C 24 14 85 DB 55");
                    static auto rage__grcViewport__SetWindow = pattern.get_first<void __thiscall(rage::grcViewport* _this, int x, int y, int width, int height, float minZ, float maxZ)>();

                    // The display list commands that set the viewport
                    static void* (*CBaseDC__operator_new)(int size, int unk);
                    static void* (__thiscall* CSetCurrentViewportDC__ctor)(void* _this, rage::grcViewport* viewport);
                    static void (*CBaseDC__Execute)(void* dc);
                    static void (__fastcall* CBaseDC__ExecuteLegacy)(void* dc);
                    static DWORD (__thiscall* rage__grcViewport__SetCameraMatrix)(rage::grcViewport* _this, void* cameraMtx);
                    static DWORD (__thiscall* rage__grcViewport__SetProjectionMatrix)(rage::grcViewport* _this, void* projMtx);
                    static int viewportStackOffset;

                    if (bCE)
                    {
                        CBaseDC__operator_new = hook::pattern("53 56 57 8B 7C 24 ? FF 74 24").get_first<void*(int, int)>();
                        CSetCurrentViewportDC__ctor = hook::pattern("56 8B F1 57 8B 46 ? C7 06 ? ? ? ? 33 05").get_first<void* __thiscall(void*, rage::grcViewport*)>();
                        CBaseDC__Execute = hook::pattern("56 57 8B 7C 24 ? 8B CF 8B 07 FF 50").get_first<void(void*)>();
                        rage__grcViewport__SetCameraMatrix = hook::pattern("55 8B EC 83 E4 ? 83 EC ? 56 8B 75 ? 57 8B 06 8B F9 56").get_first<DWORD __thiscall(rage::grcViewport*, void*)>();
                        rage__grcViewport__SetProjectionMatrix = hook::pattern("56 FF 74 24 ? 8B F1 8D 8E ? ? ? ? C6 86").get_first<DWORD __thiscall(rage::grcViewport*, void*)>();
                        viewportStackOffset = 8; // the hook is after two pushes
                    }
                    else
                    {
                        // CRenderer::ScanWorldShadowMap: viewport camera and projection, then a CSetCurrentViewportDC
                        pattern = hook::pattern("8D 44 24 ? 50 8D 8C 24 ? ? ? ? E8 ? ? ? ? 8B CE C1 E1 06 8D 94 0C ? ? ? ? 52 8D 8C 24 ? ? ? ? E8 ? ? ? ? 53 68 10 04 00 00 E8 ? ? ? ? 83 C4 08 3B C3 74 ? 8D 8C 24 ? ? ? ? 51 8B C8 E8 ? ? ? ? EB ? 33 C0 8B C8 E8");
                        rage__grcViewport__SetCameraMatrix = injector::GetBranchDestination(pattern.get_first(0x0C)).get<DWORD __thiscall(rage::grcViewport*, void*)>();
                        rage__grcViewport__SetProjectionMatrix = injector::GetBranchDestination(pattern.get_first(0x25)).get<DWORD __thiscall(rage::grcViewport*, void*)>();
                        CBaseDC__operator_new = injector::GetBranchDestination(pattern.get_first(0x30)).get<void*(int, int)>();
                        CSetCurrentViewportDC__ctor = injector::GetBranchDestination(pattern.get_first(0x46)).get<void* __thiscall(void*, rage::grcViewport*)>();
                        CBaseDC__ExecuteLegacy = injector::GetBranchDestination(pattern.get_first(0x51)).get<void __fastcall(void*)>();
                        viewportStackOffset = 0;
                    }

                    struct CCascadedShadow_obj1
                    {
                        rage::Vector4 mParam0123;
                        rage::Vector4 mParam4567;
                        rage::Vector4 mParam89101113;
                        rage::Vector4 mParam14151617;
                        rage::Vector4 mFacetCentre;
                        rage::Vector4 mParam18192021;
                        rage::Matrix44 mCameraMatrix;
                        rage::Matrix44 mProjectionMatrices[4];
                        char field_1A0[0xB0];
                        rage::Vector4 mDirection;
                        rage::Vector4 mViewPosition;
                    };

                    // CRenderer::ScanWorldShadowMap, after the cascade's render targets are set: viewport on its tile
                    pattern = hook::pattern(bCE ? "E8 ? ? ? ? 6A ? 6A ? FF B7 ? ? ? ? E8 ? ? ? ? 6A ? 6A ? C7 44 24" : "68 ? ? ? ? E8 ? ? ? ? 8B 97 38 09 00 00");
                    static auto viewportHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                    {
                        // Locals of the function: its viewport and the cascades' matrices
                        auto* vp = (rage::grcViewport*)(regs.esp + viewportStackOffset + 0x2E0);
                        auto* cascadedShadowObj = (CCascadedShadow_obj1*)(regs.esp + viewportStackOffset + 0x70);
                        int cascadeIndex = (int)regs.esi;

                        vp->mWidth = shadowRes * 4;
                        vp->mHeight = shadowRes;
                        vp->mIsPerspective = false;

                        rage__grcViewport__SetWindow(vp, cascadeIndex * shadowRes, 0, shadowRes, shadowRes, 0.0f, 1.0f);
                        rage__grcViewport__SetCameraMatrix(vp, &cascadedShadowObj->mCameraMatrix);
                        rage__grcViewport__SetProjectionMatrix(vp, &cascadedShadowObj->mProjectionMatrices[cascadeIndex]);

                        void* dc = CBaseDC__operator_new(0x410, 0); // sizeof(CSetCurrentViewportDC)
                        dc = CSetCurrentViewportDC__ctor(dc, vp);
                        if (CBaseDC__Execute)
                            CBaseDC__Execute(dc);
                        else
                            CBaseDC__ExecuteLegacy(dc);
                    });

                    // Sampleable depth format for the atlas
                    if (bCE)
                    {
                        static auto hwFilteringHook = safetyhook::create_mid(hook::pattern("C6 44 24 ? ? 80 7E ? ? BF").get_first(), [](SafetyHookContext& regs)
                        {
                            rage::grcRenderTargetPC* rt = (rage::grcRenderTargetPC*)regs.esi;
                            if (std::string_view(rt->mName) == "CASCADE_ATLAS")
                                regs.ebx = D3DFMT_D24X8;
                        });
                    }
                    else
                    {
                        // grcRenderTargetPC::Create, at CreateTexture
                        static auto hwFilteringHook = safetyhook::create_mid(hook::pattern("8B 52 5C 6A 00 8D 5E 34 53 6A 00 57").get_first(3), [](SafetyHookContext& regs)
                        {
                            rage::grcRenderTargetPC* rt = (rage::grcRenderTargetPC*)regs.esi;
                            if (std::string_view(rt->mName) == "CASCADE_ATLAS")
                                regs.edi = D3DFMT_D24X8;
                        });
                    }

                    // No colour writes in the cascade pass, unless CHSS needs the raw depth (below)
                    pattern = hook::pattern(bCE ? "C7 44 24 ? ? ? ? ? 8B 01 6A ? 68 ? ? ? ? FF 50 ? 8B 0D ? ? ? ? 8D 54 24" : "C7 44 24 ? 04 00 00 00 8B 01 8B 40 38 6A ? 68");
                    injector::WriteMemory<int>(pattern.get_first(4), rage::GRCFMT_NULL, true);

                    pattern = hook::pattern(bCE ? "C7 05 ? ? ? ? ? ? ? ? 8B 0D ? ? ? ? 85 C9 74 ? 0F B7 41 ? 66 85 C0 74 ? 48 66 89 41 ? 8A 41 ? 3C ? 74 ? 3C ? 74 ? 32 C0 EB ? B0 ? 66 83 79 ? ? 75 ? 84 C0 74 ? 8B 01 6A ? FF 10 C7 05 ? ? ? ? ? ? ? ? C3 CC CC CC CC CC CC CC C6 05" : "A3 ? ? ? ? 8D 44 24 ? 50 A1 ? ? ? ? 6A 20");
                    static auto& cascadeTargetR32 = **pattern.get_first<rage::grcRenderTargetPC**>(bCE ? 2 : 1);

                    pattern = hook::pattern(bCE ? "89 3D ? ? ? ? E8 ? ? ? ? 8B F0 83 C4 ? 85 F6 74 ? 8B 17 8B CF 8B 52" : "89 3D ? ? ? ? E8 ? ? ? ? 8B F0 83 C4 08 3B F3 74");
                    static auto hookRT = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                    {
                        static auto shadowFilter = FusionFixSettings.GetRef("PREF_SHADOWFILTER");
                        rage::grcTextureFormat targetFormat = shadowFilter->get() == FusionFixSettings.ShadowFilterText.eCHSS ? rage::GRCFMT_R32F : rage::GRCFMT_NULL;
                        if (cascadeTargetR32 && cascadeTargetR32->mFormat != targetFormat)
                        {
                            rage::grcRenderTargetDesc desc{};
                            desc.mMultisampleCount = 0;
                            desc.field_0 = 1;
                            desc.field_12 = 1;
                            desc.mDepthRT = nullptr;
                            desc.field_8 = 1;
                            desc.field_10 = 1;
                            desc.field_11 = 1;
                            desc.field_24 = false;
                            desc.mFormat = targetFormat;
                            desc.mLevels = 1;
                            cascadeTargetR32->Destroy();
                            cascadeTargetR32 = rage::grcTextureFactory::GetInstance()->CreateRenderTarget("CASCADE_TARGET_R32", 3, shadowRes * 4, shadowRes, 32, &desc);
                        }
                    });

                    // ShadowZTextureDirRaw: the raw depth for CHSS's blocker search
                    static int ShadowZTextureIndex;

                    pattern = hook::pattern(bCE ? "C6 05 ? ? ? ? ? 5E 85 C0" : "85 C0 C6 05 ? ? ? ? 00 5E 74");
                    static auto getShadowZTextureIndexHook = safetyhook::create_mid(pattern.get_first(bCE ? 0 : 2), [](SafetyHookContext& regs)
                    {
                        ShadowZTextureIndex = rage::grmShaderInfo::getGlobalParameterIndexByName("ShadowZTextureDirRaw");
                    });

                    pattern = hook::pattern(bCE ? "E8 ? ? ? ? 83 C4 ? 5E 8B E5 5D C3 8D 4C 24" : "E8 ? ? ? ? 83 C4 08 5E 8B E5 5D C3 0F 57 C0");
                    static auto CShaderLib__SetGlobalVarInContext = injector::GetBranchDestination(pattern.get_first()).get<void(int, void*)>();
                    static auto setShadowZTextureHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                    {
                        CShaderLib__SetGlobalVarInContext(ShadowZTextureIndex, &cascadeTargetR32);
                    });

                    pattern = hook::pattern(bCE ? "E8 ? ? ? ? 83 C4 ? 8D 44 24 ? 50 FF 35" : "E8 ? ? ? ? 8B 15 ? ? ? ? 83 C4 40 8D 4C 24");
                    static auto nullShadowZTexture = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                    {
                        void* null = nullptr;
                        CShaderLib__SetGlobalVarInContext(ShadowZTextureIndex, &null);
                    });
                }
            }
        };
    }
} Shadows;