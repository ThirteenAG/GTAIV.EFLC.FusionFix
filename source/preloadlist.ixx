module;

#include <common.hxx>
#include <unordered_set>

export module preloadlist;

import common;
import comvars;

std::vector<std::string> gFxcNames{};
std::vector<std::string> gFxcLoadedNames{};

const std::unordered_set<std::string> gEmbeddedShaders = {
    "rage_im",
    "rage_bink",
    "shadowSmartBlit",
    "shadowZ",
    "rage_im",
    "mirror",
    "water",
    "rmptfx_default",
    "rmptfx_litsprite",
    "rage_postfx",
    "cascadeConvert",
    "cascadeGen",
    "deferred_lighting",
    "gpuptfx_update",
    "gpuptfx_simplerender",
    "rage_perlinnoise"
};

auto ResolveVirtualPath(const char* in) -> std::filesystem::path
{
    if (!in || !*in)
        return {};

    std::string s(in);

    std::replace(s.begin(), s.end(), '\\', '/');

    s.erase(std::remove_if(s.begin(), s.end(), [](char c)
    {
        return !std::isalnum(c) && c != '/' && c != '.' && c != '_' && c != '-' && c != ' ';
    }), s.end());

    while (!s.empty() && (s.front() == '/' || s.front() == '.'))
        s.erase(s.begin());

    if (!s.empty() && s.back() == '/')
        s.pop_back();

    std::wstring path;
    path.resize(MAX_PATH, L'\0');
    if (!UAL::GetOverloadPathW || !UAL::GetOverloadPathW(path.data(), path.size()))
    {
        path = GetExeModulePath() / L"update";
    }

    return std::filesystem::path(path.data()) / std::filesystem::path(s);
}

auto CollectFxcNames(const std::filesystem::path& dir) -> std::vector<std::string>
{
    std::vector<std::string> out;
    std::error_code ec;

    if (dir.empty() || !std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec))
        return out;

    std::filesystem::recursive_directory_iterator it(dir, std::filesystem::directory_options::skip_permission_denied | std::filesystem::directory_options::follow_directory_symlink, ec);
    std::filesystem::recursive_directory_iterator end;

    for (; !ec && it != end; it.increment(ec))
    {
        std::error_code fileEc;
        if (!it->is_regular_file(fileEc))
            continue;

        auto ext = it->path().extension().string();
        if (iequals(ext, ".fxc"))
            out.push_back(it->path().stem().string());
    }

    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
};

namespace rage
{
    void* ASSET = nullptr;

    namespace fiAssetManager
    {
        int (__stdcall* FindPath)(void* assetManager, void*) = nullptr;
    }

    namespace grmShaderFxTemplate
    {
        SafetyHookInline shLoadIntoSlot = {};
        void __fastcall LoadIntoSlot(const char* a1, int a2)
        {
            if (a1)
                gFxcLoadedNames.push_back(a1);

            return shLoadIntoSlot.unsafe_fastcall(a1, a2);
        }

        SafetyHookInline shLoadIntoSlot_2 = {};
        void __cdecl LoadIntoSlot_2(const char* a1, int a2)
        {
            if (a1)
                gFxcLoadedNames.push_back(a1);

            return shLoadIntoSlot_2.unsafe_ccall(a1, a2);
        }
    }

    namespace grmShaderFactoryStandard
    {
        SafetyHookInline shPreloadShaders = {};
        void __fastcall PreloadShaders(void* a1, void* edx, char* a2, bool a3)
        {
            static bool Scanned = false;

            if (!Scanned)
            {
                auto RealPath = ResolveVirtualPath(a2);
                gFxcNames = CollectFxcNames(RealPath);
                Scanned = true;
            }

            shPreloadShaders.unsafe_fastcall(a1, edx, a2, a3);
        }
    };
}

class PreloadList
{
public:
    PreloadList()
    {
        FusionFix::onInitEvent() += []()
        {
            auto pattern = find_pattern("B9 ? ? ? ? E8 ? ? ? ? 8B F8 85 FF 0F 84 ? ? ? ? 83 EC", "B9 ? ? ? ? E8 ? ? ? ? 8B F8 3B FB");
            rage::ASSET = *pattern.get_first<void*>(1);

            pattern = find_pattern("E8 ? ? ? ? 8B D8 68 ? ? ? ? 8D 84 24", "E8 ? ? ? ? 8B E8 68 ? ? ? ? 8D 84 24 ? ? ? ? 50 8D 4C 24", "E8 ? ? ? ? 8B E8 68 ? ? ? ? 8D 44 24");
            rage::fiAssetManager::FindPath = (decltype(rage::fiAssetManager::FindPath))injector::GetBranchDestination(pattern.get_first(0)).as_int();

            pattern = hook::pattern("E8 ? ? ? ? 68 ? ? ? ? 8D 84 24 ? ? ? ? 50 8D 4C 24");
            if (!pattern.empty())
            {
                rage::grmShaderFxTemplate::shLoadIntoSlot = safetyhook::create_inline(injector::GetBranchDestination(pattern.get_first(0)).as_int(), rage::grmShaderFxTemplate::LoadIntoSlot);
            }
            else
            {
                pattern = find_pattern("E8 ? ? ? ? 83 C4 ? 68 ? ? ? ? 8D 84 24 ? ? ? ? 50 8D 4C 24 ? E8", "E8 ? ? ? ? 83 C4 ? 68 ? ? ? ? 8D 44 24 ? 50 8D 8C 24");
                rage::grmShaderFxTemplate::shLoadIntoSlot_2 = safetyhook::create_inline(injector::GetBranchDestination(pattern.get_first(0)).as_int(), rage::grmShaderFxTemplate::LoadIntoSlot_2);
            }

            pattern = find_pattern("81 EC ? ? ? ? B9 ? ? ? ? 53 55", "81 EC ? ? ? ? 53 55 56 8B B4 24 ? ? ? ? 57 56", "81 EC ? ? ? ? 53 56 8B B4 24 ? ? ? ? 57 56 B9");
            rage::grmShaderFactoryStandard::shPreloadShaders = safetyhook::create_inline(pattern.get_first(0), rage::grmShaderFactoryStandard::PreloadShaders);

            pattern = find_pattern("83 7F ? ? 75 ? 83 7F ? ? 74 ? 8B CF E8 ? ? ? ? 8B 0F FF 77 ? 8B 01 FF 50 ? C7 47 ? ? ? ? ? C7 07 ? ? ? ? EB", "39 5F ? 75 ? 39 5F ? 74 ? 8B CF E8 ? ? ? ? 8B 0F 8B 11", "39 5F ? 5D 75");
            static auto rage__grmShaderFactoryStandard__PreloadShaders_Hook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
            {
                static bool Scanned = false;

                if (!Scanned)
                {
                    if (!gFxcNames.empty())
                    {
                        for (const auto& Loaded : gFxcLoadedNames)
                        {
                            std::erase_if(gFxcNames, [&](const std::string& Name)
                            {
                                return iequals(Name, Loaded);
                            });
                        }

                        std::erase_if(gFxcNames, [&](const std::string& Name)
                        {
                            return gEmbeddedShaders.contains(Name);
                        });
                    }

                    for (const auto& Name : gFxcNames)
                    {
                        if (rage::grmShaderFxTemplate::shLoadIntoSlot)
                            rage::grmShaderFxTemplate::shLoadIntoSlot.unsafe_fastcall(Name.c_str(), regs.ebx);

                        if (rage::grmShaderFxTemplate::shLoadIntoSlot_2)
                            rage::grmShaderFxTemplate::shLoadIntoSlot_2.unsafe_ccall(Name.c_str(), regs.ebp);
                    }

                    gFxcNames.clear();
                    gFxcLoadedNames.clear();

                    Scanned = true;
                }
            });
        };
    }
} PreloadList;