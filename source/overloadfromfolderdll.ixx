module;

#include <common.hxx>

export module overloadfromfolderdll;

import common;

HMODULE hm = NULL;
HMODULE ual = NULL;

typedef HANDLE(WINAPI* tCreateFileA)(LPCSTR lpFilename, DWORD dwAccess, DWORD dwSharing, LPSECURITY_ATTRIBUTES saAttributes, DWORD dwCreation, DWORD dwAttributes, HANDLE hTemplate);
typedef HANDLE(WINAPI* tCreateFileW)(LPCWSTR lpFilename, DWORD dwAccess, DWORD dwSharing, LPSECURITY_ATTRIBUTES saAttributes, DWORD dwCreation, DWORD dwAttributes, HANDLE hTemplate);
typedef DWORD(WINAPI* tGetFileAttributesA)(LPCSTR lpFileName);
typedef DWORD(WINAPI* tGetFileAttributesW)(LPCWSTR lpFileName);
typedef BOOL(WINAPI* tGetFileAttributesExA)(LPCSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation);
typedef BOOL(WINAPI* tGetFileAttributesExW)(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation);
tCreateFileA ptrCreateFileA;
tCreateFileW ptrCreateFileW;
tGetFileAttributesA ptrGetFileAttributesA;
tGetFileAttributesW ptrGetFileAttributesW;
tGetFileAttributesExA ptrGetFileAttributesExA;
tGetFileAttributesExW ptrGetFileAttributesExW;

void HookKernel32IATForOverride(HMODULE mod)
{
    IATHook::Replace(mod, "KERNEL32.DLL",
        std::forward_as_tuple("CreateFileA", ptrCreateFileA),
        std::forward_as_tuple("CreateFileW", ptrCreateFileW),
        std::forward_as_tuple("GetFileAttributesA", ptrGetFileAttributesA),
        std::forward_as_tuple("GetFileAttributesW", ptrGetFileAttributesW),
        std::forward_as_tuple("GetFileAttributesExA", ptrGetFileAttributesExA),
        std::forward_as_tuple("GetFileAttributesExW", ptrGetFileAttributesExW)
    );
}

void OverrideCreateFileInDLLs(HMODULE mod)
{
    ModuleList dlls;
    dlls.Enumerate(ModuleList::SearchLocation::LocalOnly);
    for (auto& e : dlls.m_moduleList)
    {
        auto m = std::get<HMODULE>(e);
        if (m == mod && m != ual && m != hm && m != GetModuleHandle(NULL))
            HookKernel32IATForOverride(mod);
    }
}

class OverloadFromFolderDLL
{
public:
    OverloadFromFolderDLL()
    {
        FusionFix::onInitEvent() += []()
        {
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&OverrideCreateFileInDLLs, &hm);

            static std::once_flag flag;
            std::call_once(flag, []()
            {
                ModuleList dlls;
                dlls.Enumerate(ModuleList::SearchLocation::LocalOnly);
                for (auto& e : dlls.m_moduleList)
                {
                    auto m = std::get<HMODULE>(e);
                    auto pCreateFileA = (tCreateFileA)GetProcAddress(m, "CustomCreateFileA");
                    auto pCreateFileW = (tCreateFileW)GetProcAddress(m, "CustomCreateFileW");
                    auto pGetFileAttributesA = (tGetFileAttributesA)GetProcAddress(m, "CustomGetFileAttributesA");
                    auto pGetFileAttributesW = (tGetFileAttributesW)GetProcAddress(m, "CustomGetFileAttributesW");
                    auto pGetFileAttributesExA = (tGetFileAttributesExA)GetProcAddress(m, "CustomGetFileAttributesExA");
                    auto pGetFileAttributesExW = (tGetFileAttributesExW)GetProcAddress(m, "CustomGetFileAttributesExW");
                    if (pCreateFileA && pCreateFileW)
                    {
                        ual = m;
                        ptrCreateFileA = pCreateFileA;
                        ptrCreateFileW = pCreateFileW;
                        if (pGetFileAttributesA)
                            ptrGetFileAttributesA = pGetFileAttributesA;
                        if (pGetFileAttributesW)
                            ptrGetFileAttributesW = pGetFileAttributesW;
                        if (pGetFileAttributesExA)
                            ptrGetFileAttributesExA = pGetFileAttributesExA;
                        if (pGetFileAttributesExW)
                            ptrGetFileAttributesExW = pGetFileAttributesExW;
                        break;
                    }
                }

                if (ptrCreateFileA && ptrCreateFileW)
                {
                    for (auto& e : dlls.m_moduleList)
                    {
                        auto m = std::get<HMODULE>(e);
                        if (m != ual && m != hm && m != GetModuleHandle(NULL))
                            HookKernel32IATForOverride(m);
                    }
                    CallbackHandler::RegisterCallback(OverrideCreateFileInDLLs);
                }
            });
        };
    }
} OverloadFromFolderDLL;