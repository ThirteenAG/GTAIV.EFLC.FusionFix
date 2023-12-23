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
            //IMG Loader
            auto pattern = find_pattern("E8 ? ? ? ? 6A 00 E8 ? ? ? ? 83 C4 14 6A 00 B9 ? ? ? ? E8 ? ? ? ? 83 3D", "E8 ? ? ? ? 6A 00 E8 ? ? ? ? 83 C4 14 6A 00 B9");
            static auto CImgManager__addImgFile = (void(__cdecl*)(const char*, char, int)) injector::GetBranchDestination(pattern.get_first(0)).get();
            static auto sub_A95980 = (void(__cdecl*)(unsigned char)) injector::GetBranchDestination(pattern.get_first(7)).get();

            pattern = find_pattern("89 44 24 44 8B 44 24 4C 53 68 ? ? ? ? 50 E8 ? ? ? ? 8B D8 6A 01 53 E8 ? ? ? ? 83 C4 10", "89 44 24 44 8B 44 24 4C 57 68 ? ? ? ? 50 E8 ? ? ? ? 8B F8 6A 01 57 E8 ? ? ? ? 83 C4 10");
            struct ImgListHook
            {
                void operator()(injector::reg_pack& regs)
                {
                    *(uint32_t*)(regs.esp + 0x44) = regs.eax;
                    regs.eax = *(uint32_t*)(regs.esp + 0x4C);

                    auto exePath = std::filesystem::path(GetExeModulePath<std::wstring>());
                    auto updatePath = exePath.remove_filename() / L"update";

                    if (std::filesystem::exists(updatePath))
                    {
                        for (const auto& file : std::filesystem::recursive_directory_iterator(updatePath, std::filesystem::directory_options::skip_permission_denied))
                        {
                            static auto gamePath = std::filesystem::path(GetExeModulePath<std::wstring>()).remove_filename();
                            auto filePath = std::filesystem::path(file.path());
                            auto relativePath = std::filesystem::relative(filePath, gamePath);

                            if (!std::filesystem::is_directory(file) && iequals(filePath.extension().wstring(), L".img"))
                            {
                                static std::vector<std::filesystem::path> episodicPaths = {
                                    std::filesystem::path("/IV/").make_preferred(),
                                    std::filesystem::path("/TLAD/").make_preferred(),
                                    std::filesystem::path("/TBoGT/").make_preferred(),
                                };

                                auto is_subpath = [](const std::filesystem::path& path, const std::filesystem::path& base) -> bool {
                                    std::wstring str1(path.wstring()); std::wstring str2(base.wstring());
                                    std::transform(str1.begin(), str1.end(), str1.begin(), ::tolower);
                                    std::transform(str2.begin(), str2.end(), str2.begin(), ::tolower);
                                    return str1.contains(str2);
                                };

                                auto imgPath = std::filesystem::relative(filePath, exePath).native();
                                std::replace(std::begin(imgPath), std::end(imgPath), L'\\', L'/');
                                auto pos = imgPath.find(std::filesystem::path("/").native());

                                if (pos != imgPath.npos)
                                {
                                    imgPath.replace(pos, 1, std::filesystem::path(":/").native());

                                    if (iequals(imgPath, L"update:/update.img"))
                                        continue;

                                    if (std::any_of(std::begin(episodicPaths), std::end(episodicPaths), [&](auto& it) { return is_subpath(relativePath, it); }))
                                    {
                                        if (*_dwCurrentEpisode < int32_t(episodicPaths.size()) && is_subpath(relativePath, episodicPaths[*_dwCurrentEpisode]))
                                            CImgManager__addImgFile(std::filesystem::path(imgPath).string().c_str(), 1, -1);
                                    }
                                    else
                                        CImgManager__addImgFile(std::filesystem::path(imgPath).string().c_str(), 1, -1);
                                }
                            }
                        }
                    }
                }
            }; injector::MakeInline<ImgListHook>(pattern.get_first(0), pattern.get_first(8));
        };
    }
} IMGLoader;