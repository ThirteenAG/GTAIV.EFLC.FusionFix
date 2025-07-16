module;

#include <Shlobj.h>

#include <common.hxx>

export module userdata;

import common;
import settings;
import <filesystem>;

std::filesystem::path customUserProfilePath;

HRESULT WINAPI SHGetFolderPathA_Hook(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPSTR pszPath)
{
    switch (csidl)
    {
        case CSIDL_APPDATA:
        case CSIDL_COMMON_APPDATA:
        case CSIDL_COMMON_DOCUMENTS:
        case CSIDL_LOCAL_APPDATA:
        case CSIDL_PERSONAL:
            return strncpy_s(pszPath, MAX_PATH, customUserProfilePath.string().data(), MAX_PATH);
        default:
            break;
    }
    return SHGetFolderPathA(hwnd, csidl, hToken, dwFlags, pszPath);
}

HRESULT WINAPI SHGetFolderPathW_Hook(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPWSTR pszPath)
{
    switch (csidl)
    {
    case CSIDL_APPDATA:
    case CSIDL_COMMON_APPDATA:
    case CSIDL_COMMON_DOCUMENTS:
    case CSIDL_LOCAL_APPDATA:
    case CSIDL_PERSONAL:
        return wcsncpy_s(pszPath, MAX_PATH, customUserProfilePath.wstring().data(), MAX_PATH);
    default:
        break;
    }
    return SHGetFolderPathW(hwnd, csidl, hToken, dwFlags, pszPath);
}

SafetyHookInline sh_sub_8C9830{};
const char* sub_8C9830()
{
    if (CText::hasViceCityStrings())
        return "SGTAVC";
    return "SGTA4";
}

class UserData
{
public:
    UserData()
    {
        auto pattern = find_pattern("E8 ? ? ? ? 50 68 ? ? ? ? FF 74 24", "E8 ? ? ? ? 8B 4C 24 ? 50 68");
        if (!pattern.empty())
            sh_sub_8C9830 = safetyhook::create_inline(injector::GetBranchDestination(pattern.get_first(0)).as_int(), sub_8C9830);

        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");
            customUserProfilePath = std::filesystem::path(iniReader.ReadString("USERPROFILE", "CustomUserProfilePath", ""));

            if (customUserProfilePath.empty())
                return;

            if (!customUserProfilePath.is_absolute())
            {
                auto exe_path = GetExeModulePath();
                customUserProfilePath = exe_path / customUserProfilePath;
            }

            auto CreateDirectoryRecursive = [](auto dirName) -> bool
            {
                std::error_code err;
                if (!std::filesystem::create_directories(dirName, err))
                {
                    if (std::filesystem::exists(dirName))
                        return true;
                    return false;
                }
                return true;
            };

            if (CreateDirectoryRecursive(customUserProfilePath))
            {
                IATHook::Replace(GetModuleHandleA(NULL), "SHELL32.DLL",
                    std::forward_as_tuple("SHGetFolderPathA", SHGetFolderPathA_Hook),
                    std::forward_as_tuple("SHGetFolderPathW", SHGetFolderPathW_Hook)
                );
            }
        };
    }
} UserData;