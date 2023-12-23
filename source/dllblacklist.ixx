module;

#include <common.hxx>
#include "MinHook.h"

export module dllblacklist;

import common;

std::vector<std::wstring> dlllist = {
    L"NahimicOSD",
};

typedef void(WINAPI* LdrLoadDllFunc) (IN PWCHAR PathToFile OPTIONAL, IN PULONG Flags OPTIONAL, IN PUNICODE_STRING ModuleFileName, OUT HMODULE* ModuleHandle);
static LdrLoadDllFunc realLdrLoadDll = NULL;
static void WINAPI LdrLoadDllHook(IN PWCHAR PathToFile OPTIONAL, IN PULONG Flags OPTIONAL, IN PUNICODE_STRING ModuleFileName, OUT HMODULE* ModuleHandle)
{
    if (ModuleFileName->Buffer)
    {
        std::wstring fileName(ModuleFileName->Buffer, ModuleFileName->Length / sizeof(WCHAR));
        if (std::any_of(std::begin(dlllist), std::end(dlllist), [&fileName](auto& i) { return iequals(fileName, i); }))
        {
            SetLastError(ERROR_DLL_INIT_FAILED);
            return;
        }
    }

    if (realLdrLoadDll)
        realLdrLoadDll(PathToFile, Flags, ModuleFileName, ModuleHandle);
}

class DLLBlacklist
{
public:
    DLLBlacklist()
    {
        FusionFix::onInitEvent() += []()
        {
            for (auto& s : dlllist)
            {
                auto hModule = GetModuleHandleW(s.c_str());
                if (hModule)
                    FreeLibrary(hModule);
            }

            LdrLoadDllFunc baseLdrLoadDll = (LdrLoadDllFunc)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrLoadDll");
            if (baseLdrLoadDll == NULL)
                return;
            
            MH_Initialize();
            MH_CreateHook(baseLdrLoadDll, LdrLoadDllHook, (LPVOID*)&realLdrLoadDll);
            MH_EnableHook(baseLdrLoadDll);
        };
    }
} DLLBlacklist;