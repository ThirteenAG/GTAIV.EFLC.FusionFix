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
            CIniReader iniReader("");
            static auto bLamppostShadows = iniReader.ReadInteger("NIGHTSHADOWS", "LamppostShadows", 0) != 0;

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

                    if (*_dwCurrentEpisode) {
                        if (std::string_view((const char*)regs.eax).contains(":"))
                            return;
                    }

                    auto gamePath = GetExeModulePath();
                    auto updatePath = gamePath / L"update";

                    if (std::filesystem::exists(updatePath))
                    {
                        constexpr auto perms = std::filesystem::directory_options::skip_permission_denied | std::filesystem::directory_options::follow_directory_symlink;
                        for (const auto& file : std::filesystem::recursive_directory_iterator(updatePath, perms))
                        {
                            auto filePath = std::filesystem::path(file.path());

                            if (!std::filesystem::is_directory(file) && iequals(filePath.extension().native(), L".img"))
                            {
                                if (bLamppostShadows)
                                {
                                    if (iequals(filePath.stem().native(), L"FusionLights"))
                                        continue;
                                }
                                else
                                {
                                    if (iequals(filePath.stem().native(), L"FusionLightsShadowcast"))
                                        continue;
                                }

                                static std::vector<std::filesystem::path> episodicPaths = {
                                    std::filesystem::path("IV"),
                                    std::filesystem::path("TLAD"),
                                    std::filesystem::path("TBoGT"),
                                };

                                auto contains_subfolder = [](const std::filesystem::path& path, const std::filesystem::path& base) -> bool {
                                    for (auto& p : path)
                                    {
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
                                auto imgPath = relativePath.native();
                                std::replace(std::begin(imgPath), std::end(imgPath), L'\\', L'/');
                                auto pos = imgPath.find(L'/');

                                if (pos != imgPath.npos)
                                {
                                    imgPath.replace(pos, 1, L":/");

                                    if (iequals(imgPath, L"update:/update.img"))
                                        continue;

                                    if (std::any_of(std::begin(episodicPaths), std::end(episodicPaths), [&](auto& it) { return contains_subfolder(relativePath, it); }))
                                    {
                                        if (*_dwCurrentEpisode < int32_t(episodicPaths.size()) && contains_subfolder(relativePath, episodicPaths[*_dwCurrentEpisode]))
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