module;

#include <common.hxx>

export module imgloader;

import common;
import settings;
import comvars;

class IMGLoader
{
public:
    IMGLoader()
    {
        FusionFix::onInitEvent() += []()
        {
            static bool (WINAPI* GetOverloadPathW)(wchar_t* out, size_t out_size) = nullptr;

            ModuleList dlls;
            dlls.Enumerate(ModuleList::SearchLocation::LocalOnly);
            for (auto& e : dlls.m_moduleList)
            {
                auto m = std::get<HMODULE>(e);
                if (IsModuleUAL(m)) {
                    GetOverloadPathW = (decltype(GetOverloadPathW))GetProcAddress(m, "GetOverloadPathW");
                    break;
                }
            }

            std::wstring s;
            s.resize(MAX_PATH, L'\0');
            if (!GetOverloadPathW || !GetOverloadPathW(s.data(), s.size()))
            {
                s = GetExeModulePath() / L"update";
            }
            else
            {
                // Redirect update device
                static std::string update = std::filesystem::path(s.data()).filename().string();
                static std::string supdate = "%s" + update;

                auto pattern = find_pattern("68 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? 84 C0 74 ? 68 ? ? ? ? EB", "68 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? 84 C0 74 ? 68 ? ? ? ? EB");
                injector::WriteMemory(pattern.get_first(1), update.data(), true);

                pattern = find_pattern("68 ? ? ? ? 50 E8 ? ? ? ? 83 C4 ? 8D 04 24 6A", "68 ? ? ? ? 50 E8 ? ? ? ? 83 C4 ? 6A ? 8D 4C 24 ? 51 B9");
                injector::WriteMemory(pattern.get_first(1), supdate.data(), true);
            }

            static auto updatePath = std::filesystem::path(s.data());

            //IMG Loader
            auto pattern = find_pattern("E8 ? ? ? ? 6A 00 E8 ? ? ? ? 83 C4 14 6A 00 B9 ? ? ? ? E8 ? ? ? ? 83 3D", "E8 ? ? ? ? 6A 00 E8 ? ? ? ? 83 C4 14 6A 00 B9");
            static auto CImgManager__addImgFile = (void(__cdecl*)(const char*, char, int)) injector::GetBranchDestination(pattern.get_first(0)).get();
            static auto sub_A95980 = (void(__cdecl*)(unsigned char)) injector::GetBranchDestination(pattern.get_first(7)).get();

            pattern = find_pattern("89 44 24 44 8B 44 24 4C 53 68 ? ? ? ? 50 E8 ? ? ? ? 8B D8 6A 01 53 E8 ? ? ? ? 83 C4 10", "89 44 24 44 8B 44 24 4C 57 68 ? ? ? ? 50 E8 ? ? ? ? 8B F8 6A 01 57 E8 ? ? ? ? 83 C4 10");
            struct ImgListHook
            {
                void operator()(injector::reg_pack& regs)
                {
                    std::vector<const char*> defaultImgList =
                    {
                        "update/update.img",
                        "common/data/cdimages/carrec.img",
                        "common/data/cdimages/navgen_script.img",
                        "common/data/cdimages/script.img",
                        "common/data/cdimages/script_network.img",
                        "pc/anim/anim.img",
                        "pc/anim/cuts.img",
                        "pc/anim/cutsprops.img",
                        "pc/data/cdimages/gtxd.img",
                        "pc/data/cdimages/navmeshes.img",
                        "pc/data/cdimages/navmeshes_animviewer.img",
                        "pc/data/cdimages/paths.img",
                        "pc/data/cdimages/scripttxds.img",
                        "pc/data/maps/east/bronx_e.img",
                        "pc/data/maps/east/bronx_e2.img",
                        "pc/data/maps/east/bronx_w.img",
                        "pc/data/maps/east/bronx_w2.img",
                        "pc/data/maps/east/brook_n.img",
                        "pc/data/maps/east/brook_n2.img",
                        "pc/data/maps/east/brook_s.img",
                        "pc/data/maps/east/brook_s2.img",
                        "pc/data/maps/east/brook_s3.img",
                        "pc/data/maps/east/east_xr.img",
                        "pc/data/maps/east/queens_e.img",
                        "pc/data/maps/east/queens_m.img",
                        "pc/data/maps/east/queens_w.img",
                        "pc/data/maps/east/queens_w2.img",
                        "pc/data/maps/generic/lodcull_e.img",
                        "pc/data/maps/generic/lodcull_j.img",
                        "pc/data/maps/generic/lodcull_m.img",
                        "pc/data/maps/generic/procobj.img",
                        "pc/data/maps/interiors/generic/bars_1.img",
                        "pc/data/maps/interiors/generic/bars_2.img",
                        "pc/data/maps/interiors/generic/bars_3.img",
                        "pc/data/maps/interiors/generic/blocks.img",
                        "pc/data/maps/interiors/generic/blocks_2.img",
                        "pc/data/maps/interiors/generic/blocks_3.img",
                        "pc/data/maps/interiors/generic/brownstones.img",
                        "pc/data/maps/interiors/generic/homes_1.img",
                        "pc/data/maps/interiors/generic/homes_2.img",
                        "pc/data/maps/interiors/generic/homes_3.img",
                        "pc/data/maps/interiors/generic/indust_1.img",
                        "pc/data/maps/interiors/generic/public_1.img",
                        "pc/data/maps/interiors/generic/public_2.img",
                        "pc/data/maps/interiors/generic/public_3.img",
                        "pc/data/maps/interiors/generic/retail_1.img",
                        "pc/data/maps/interiors/generic/retail_2.img",
                        "pc/data/maps/interiors/generic/retail_3.img",
                        "pc/data/maps/interiors/generic/retail_4.img",
                        "pc/data/maps/interiors/int_props/props_ab.img",
                        "pc/data/maps/interiors/int_props/props_ah.img",
                        "pc/data/maps/interiors/int_props/props_km.img",
                        "pc/data/maps/interiors/int_props/props_ld.img",
                        "pc/data/maps/interiors/int_props/props_mp.img",
                        "pc/data/maps/interiors/int_props/props_ss.img",
                        "pc/data/maps/interiors/mission/level_1.img",
                        "pc/data/maps/interiors/mission/level_2.img",
                        "pc/data/maps/interiors/mission/level_3.img",
                        "pc/data/maps/interiors/mission/level_4.img",
                        "pc/data/maps/interiors/mission/level_5.img",
                        "pc/data/maps/interiors/mission/level_6.img",
                        "pc/data/maps/interiors/test/interiors.img",
                        "pc/data/maps/jersey/nj_01.img",
                        "pc/data/maps/jersey/nj_02.img",
                        "pc/data/maps/jersey/nj_03.img",
                        "pc/data/maps/jersey/nj_04e.img",
                        "pc/data/maps/jersey/nj_04w.img",
                        "pc/data/maps/jersey/nj_05.img",
                        "pc/data/maps/jersey/nj_docks.img",
                        "pc/data/maps/jersey/nj_liberty.img",
                        "pc/data/maps/jersey/nj_xref.img",
                        "pc/data/maps/leveldes/levelmap.img",
                        "pc/data/maps/leveldes/level_xr.img",
                        "pc/data/maps/manhat/manhat01.img",
                        "pc/data/maps/manhat/manhat02.img",
                        "pc/data/maps/manhat/manhat03.img",
                        "pc/data/maps/manhat/manhat04.img",
                        "pc/data/maps/manhat/manhat05.img",
                        "pc/data/maps/manhat/manhat06.img",
                        "pc/data/maps/manhat/manhat07.img",
                        "pc/data/maps/manhat/manhat08.img",
                        "pc/data/maps/manhat/manhat09.img",
                        "pc/data/maps/manhat/manhat10.img",
                        "pc/data/maps/manhat/manhat11.img",
                        "pc/data/maps/manhat/manhat12.img",
                        "pc/data/maps/manhat/manhatsw.img",
                        "pc/data/maps/manhat/manhatxr.img",
                        "pc/data/maps/manhat/subwayxr.img",
                        "pc/data/maps/props/commercial/7_11.img",
                        "pc/data/maps/props/commercial/bar.img",
                        "pc/data/maps/props/commercial/beauty.img",
                        "pc/data/maps/props/commercial/clothes.img",
                        "pc/data/maps/props/commercial/fastfood.img",
                        "pc/data/maps/props/commercial/garage.img",
                        "pc/data/maps/props/commercial/office.img",
                        "pc/data/maps/props/doors/ext_door.img",
                        "pc/data/maps/props/doors/int_door.img",
                        "pc/data/maps/props/industrial/build.img",
                        "pc/data/maps/props/industrial/drums.img",
                        "pc/data/maps/props/industrial/industrial.img",
                        "pc/data/maps/props/industrial/railway.img",
                        "pc/data/maps/props/industrial/skips.img",
                        "pc/data/maps/props/lev_des/icons.img",
                        "pc/data/maps/props/lev_des/minigame.img",
                        "pc/data/maps/props/lev_des/mission_int.img",
                        "pc/data/maps/props/prop_test/cj_test.img",
                        "pc/data/maps/props/residential/bathroom.img",
                        "pc/data/maps/props/residential/bedroom.img",
                        "pc/data/maps/props/residential/details.img",
                        "pc/data/maps/props/residential/dining.img",
                        "pc/data/maps/props/residential/electrical.img",
                        "pc/data/maps/props/residential/kitchen.img",
                        "pc/data/maps/props/residential/lights.img",
                        "pc/data/maps/props/residential/soft.img",
                        "pc/data/maps/props/roadside/bins.img",
                        "pc/data/maps/props/roadside/bllbrd.img",
                        "pc/data/maps/props/roadside/crates.img",
                        "pc/data/maps/props/roadside/fences.img",
                        "pc/data/maps/props/roadside/lamppost.img",
                        "pc/data/maps/props/roadside/rubbish.img",
                        "pc/data/maps/props/roadside/sign.img",
                        "pc/data/maps/props/roadside/st_vend.img",
                        "pc/data/maps/props/roadside/traffic.img",
                        "pc/data/maps/props/roadside/works.img",
                        "pc/data/maps/props/street/amenitie.img",
                        "pc/data/maps/props/street/elecbox.img",
                        "pc/data/maps/props/street/misc.img",
                        "pc/data/maps/props/street/rooftop.img",
                        "pc/data/maps/props/vegetation/ext_veg.img",
                        "pc/data/maps/props/vegetation/int_veg.img",
                        "pc/data/maps/props/windows/gen_win.img",
                        "pc/data/maps/util/animviewer.img",
                        "pc/models/cdimages/componentpeds.img",
                        "pc/models/cdimages/pedprops.img",
                        "pc/models/cdimages/radar.img",
                        "pc/models/cdimages/vehicles.img",
                        "pc/models/cdimages/weapons.img",
                        "TBoGT/common/data/cdimages/carrec.img",
                        "TBoGT/common/data/cdimages/script.img",
                        "TBoGT/common/data/cdimages/script_network.img",
                        "TBoGT/pc/anim/anim.img",
                        "TBoGT/pc/anim/cuts.img",
                        "TBoGT/pc/anim/cutsprops.img",
                        "TBoGT/pc/data/cdimages/e2_scripttxds.img",
                        "TBoGT/pc/data/cdimages/navmeshes.img",
                        "TBoGT/pc/data/cdimages/paths.img",
                        "TBoGT/pc/data/maps/east/bronx_e.img",
                        "TBoGT/pc/data/maps/east/bronx_e2.img",
                        "TBoGT/pc/data/maps/east/bronx_w.img",
                        "TBoGT/pc/data/maps/east/bronx_w2.img",
                        "TBoGT/pc/data/maps/east/brook_n.img",
                        "TBoGT/pc/data/maps/east/brook_n2.img",
                        "TBoGT/pc/data/maps/east/brook_s.img",
                        "TBoGT/pc/data/maps/east/brook_s2.img",
                        "TBoGT/pc/data/maps/east/queens_e.img",
                        "TBoGT/pc/data/maps/east/queens_m.img",
                        "TBoGT/pc/data/maps/east/queens_w2.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_1.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_10.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_11.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_12.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_13.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_14.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_15.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_2.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_3.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_4.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_5.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_6.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_7.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_8.img",
                        "TBoGT/pc/data/maps/interiors/e2_int_9.img",
                        "TBoGT/pc/data/maps/interiors/int_1.img",
                        "TBoGT/pc/data/maps/interiors/int_2.img",
                        "TBoGT/pc/data/maps/interiors/int_3.img",
                        "TBoGT/pc/data/maps/interiors/int_4.img",
                        "TBoGT/pc/data/maps/interiors/int_5.img",
                        "TBoGT/pc/data/maps/interiors/int_6.img",
                        "TBoGT/pc/data/maps/interiors/int_7.img",
                        "TBoGT/pc/data/maps/interiors/int_8.img",
                        "TBoGT/pc/data/maps/interiors/int_9.img",
                        "TBoGT/pc/data/maps/interiors/rep_public_3.img",
                        "TBoGT/pc/data/maps/jersey/nj_01.img",
                        "TBoGT/pc/data/maps/jersey/nj_02.img",
                        "TBoGT/pc/data/maps/jersey/nj_03.img",
                        "TBoGT/pc/data/maps/jersey/nj_04w.img",
                        "TBoGT/pc/data/maps/jersey/nj_05.img",
                        "TBoGT/pc/data/maps/jersey/nj_docks.img",
                        "TBoGT/pc/data/maps/jersey/nj_liberty.img",
                        "TBoGT/pc/data/maps/manhat/manhat01.img",
                        "TBoGT/pc/data/maps/manhat/manhat02.img",
                        "TBoGT/pc/data/maps/manhat/manhat03.img",
                        "TBoGT/pc/data/maps/manhat/manhat04.img",
                        "TBoGT/pc/data/maps/manhat/manhat05.img",
                        "TBoGT/pc/data/maps/manhat/manhat06.img",
                        "TBoGT/pc/data/maps/manhat/manhat07.img",
                        "TBoGT/pc/data/maps/manhat/manhat08.img",
                        "TBoGT/pc/data/maps/manhat/manhat09.img",
                        "TBoGT/pc/data/maps/manhat/manhat10.img",
                        "TBoGT/pc/data/maps/manhat/manhat11.img",
                        "TBoGT/pc/data/maps/manhat/manhat12.img",
                        "TBoGT/pc/data/maps/props/e2_xref.img",
                        "TBoGT/pc/data/maps/props/commercial/office.img",
                        "TBoGT/pc/models/cdimages/componentpeds.img",
                        "TBoGT/pc/models/cdimages/pedprops.img",
                        "TBoGT/pc/models/cdimages/vehicles.img",
                        "TBoGT/pc/models/cdimages/weapons_e2.img",
                        "TLAD/common/data/cdimages/carrec.img",
                        "TLAD/common/data/cdimages/script.img",
                        "TLAD/common/data/cdimages/script_network.img",
                        "TLAD/pc/anim/anim.img",
                        "TLAD/pc/anim/cuts.img",
                        "TLAD/pc/anim/cutsprops.img",
                        "TLAD/pc/data/cdimages/navmeshes.img",
                        "TLAD/pc/data/cdimages/paths.img",
                        "TLAD/pc/data/cdimages/scripttxds.img",
                        "TLAD/pc/data/maps/east/bronx_e.img",
                        "TLAD/pc/data/maps/east/bronx_e2.img",
                        "TLAD/pc/data/maps/east/bronx_w.img",
                        "TLAD/pc/data/maps/east/bronx_w2.img",
                        "TLAD/pc/data/maps/east/brook_n2.img",
                        "TLAD/pc/data/maps/east/brook_s.img",
                        "TLAD/pc/data/maps/east/queens_e.img",
                        "TLAD/pc/data/maps/east/queens_w2.img",
                        "TLAD/pc/data/maps/interiors/int_1.img",
                        "TLAD/pc/data/maps/interiors/int_2.img",
                        "TLAD/pc/data/maps/interiors/int_3.img",
                        "TLAD/pc/data/maps/interiors/int_4.img",
                        "TLAD/pc/data/maps/interiors/int_5.img",
                        "TLAD/pc/data/maps/interiors/int_6.img",
                        "TLAD/pc/data/maps/interiors/int_7.img",
                        "TLAD/pc/data/maps/interiors/int_8.img",
                        "TLAD/pc/data/maps/interiors/int_9.img",
                        "TLAD/pc/data/maps/interiors/int_test.img",
                        "TLAD/pc/data/maps/jersey/nj_01.img",
                        "TLAD/pc/data/maps/jersey/nj_02.img",
                        "TLAD/pc/data/maps/jersey/nj_03.img",
                        "TLAD/pc/data/maps/jersey/nj_04w.img",
                        "TLAD/pc/data/maps/jersey/nj_05.img",
                        "TLAD/pc/data/maps/jersey/nj_docks.img",
                        "TLAD/pc/data/maps/manhat/manhat01.img",
                        "TLAD/pc/data/maps/manhat/manhat02.img",
                        "TLAD/pc/data/maps/manhat/manhat04.img",
                        "TLAD/pc/data/maps/manhat/manhat05.img",
                        "TLAD/pc/data/maps/manhat/manhat06.img",
                        "TLAD/pc/data/maps/manhat/manhat07.img",
                        "TLAD/pc/data/maps/manhat/manhat08.img",
                        "TLAD/pc/data/maps/manhat/manhat09.img",
                        "TLAD/pc/data/maps/manhat/manhat11.img",
                        "TLAD/pc/data/maps/manhat/manhat12.img",
                        "TLAD/pc/data/maps/props/e1_xref.img",
                        "TLAD/pc/models/cdimages/componentpeds.img",
                        "TLAD/pc/models/cdimages/pedprops.img",
                        "TLAD/pc/models/cdimages/vehicles.img",
                        "TLAD/pc/models/cdimages/weapons_e1.img",
                    };

                    *(uint32_t*)(regs.esp + 0x44) = regs.eax;
                    regs.eax = *(uint32_t*)(regs.esp + 0x4C);

                    if (*_dwCurrentEpisode) {
                        if (std::string_view((const char*)regs.eax).contains(":"))
                            return;
                    }

                    std::vector<std::filesystem::path> episodicPaths = {
                        std::filesystem::path("IV"),
                        std::filesystem::path("TLAD"),
                        std::filesystem::path("TBoGT"),
                        std::filesystem::path("VICECITY"),
                    };

                    auto gamePath = GetExeModulePath();
                    std::error_code ec;

                    if (std::filesystem::exists(updatePath, ec))
                    {
                        constexpr auto perms = std::filesystem::directory_options::skip_permission_denied | std::filesystem::directory_options::follow_directory_symlink;
                        for (const auto& file : std::filesystem::recursive_directory_iterator(updatePath, perms, ec))
                        {
                            auto filePath = std::filesystem::path(file.path());

                            if (!std::filesystem::is_directory(file, ec) && iequals(filePath.extension().native(), L".img"))
                            {
                                auto contains_subfolder = [](const std::filesystem::path& path, const std::filesystem::path& base) -> bool {
                                    for (auto& p : path)
                                    {
                                        if (p == *path.begin())
                                            continue;

                                        if (iequals(p.native(), base.native()))
                                            return true;
                                    }
                                    return false;
                                };

                                static auto lexicallyRelativeCaseIns = [](const std::filesystem::path& path, const std::filesystem::path& base) -> std::filesystem::path
                                {
                                    class input_iterator_range
                                    {
                                    public:
                                        input_iterator_range(const std::filesystem::path::const_iterator& first, const std::filesystem::path::const_iterator& last)
                                            : _first(first)
                                            , _last(last)
                                        {}
                                        std::filesystem::path::const_iterator begin() const { return _first; }
                                        std::filesystem::path::const_iterator end() const { return _last; }
                                    private:
                                        std::filesystem::path::const_iterator _first;
                                        std::filesystem::path::const_iterator _last;
                                    };

                                    if (!iequals(path.root_name().wstring(), base.root_name().wstring()) || path.is_absolute() != base.is_absolute() || (!path.has_root_directory() && base.has_root_directory())) {
                                        return std::filesystem::path();
                                    }
                                    std::filesystem::path::const_iterator a = path.begin(), b = base.begin();
                                    while (a != path.end() && b != base.end() && iequals(a->wstring(), b->wstring())) {
                                        ++a;
                                        ++b;
                                    }
                                    if (a == path.end() && b == base.end()) {
                                        return std::filesystem::path(".");
                                    }
                                    int count = 0;
                                    for (const auto& element : input_iterator_range(b, base.end())) {
                                        if (element != "." && element != "" && element != "..") {
                                            ++count;
                                        }
                                        else if (element == "..") {
                                            --count;
                                        }
                                    }
                                    if (count < 0) {
                                        return std::filesystem::path();
                                    }
                                    std::filesystem::path result;
                                    for (int i = 0; i < count; ++i) {
                                        result /= "..";
                                    }
                                    for (const auto& element : input_iterator_range(a, path.end())) {
                                        result /= element;
                                    }
                                    return result;
                                };

                                auto relativePath = lexicallyRelativeCaseIns(filePath, gamePath);
                                auto imgPath = relativePath.string();
                                std::replace(std::begin(imgPath), std::end(imgPath), '\\', '/');
                                auto pos = imgPath.find(L'/');

                                if (pos != imgPath.npos)
                                {
                                    imgPath = imgPath.substr(pos + 1);

                                    if (std::any_of(defaultImgList.begin(), defaultImgList.end(), [&](const char* s) { return iequals(s, imgPath.c_str()); }))
                                        continue;

                                    if (CText::hasViceCityStrings() && imgPath == "GTAIV.EFLC.FusionFix/GTAIV.EFLC.FusionFix.img")
                                        continue;

                                    imgPath = "update:/" + imgPath;

                                    if (std::any_of(std::begin(episodicPaths), std::end(episodicPaths), [&](auto& it) { return contains_subfolder(relativePath, it); }))
                                    {
                                        auto curEp = *_dwCurrentEpisode;
                                        if (CText::hasViceCityStrings())
                                            curEp = episodicPaths.size() - 1;

                                        if (curEp < int32_t(episodicPaths.size()) && contains_subfolder(relativePath, episodicPaths[curEp]))
                                            CImgManager__addImgFile(imgPath.data(), 1, -1);
                                    }
                                    else
                                        CImgManager__addImgFile(imgPath.data(), 1, -1);
                                }
                            }
                        }
                    }
                }
            }; injector::MakeInline<ImgListHook>(pattern.get_first(0), pattern.get_first(8));
        };
    }
} IMGLoader;