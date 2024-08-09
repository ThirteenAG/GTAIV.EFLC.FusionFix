module;

#include <common.hxx>
#include <fstream>

#include <common.h>
#include <StringContainer.h>

export module gxtloader;

import common;
import comvars;

export cStringContainer gxtEntries(KEYTYPE_JENKINS, true, false);

void LoadCustomGXT(std::filesystem::path& path)
{
    __try
    {
        gxtEntries.ReadGXT(path, false);
    }
    __except ((GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
    }
}

SafetyHookInline shsub_8C5D70{};
char* __fastcall sub_8C5D70(void* _this, void* edx, int a2, char* a3, char a4)
{
    auto ret = shsub_8C5D70.fastcall<char*>(_this, edx, a2, a3, a4);

    gxtEntries.Clear();
    for (auto i = 0; i < 10; i++)
    {
        std::string t = ret;
        t = t.substr(t.find_last_of("/\\") + 1);
        t.insert(t.find_last_of('.'), i == 0 ? "FF" : std::to_string(i));
        auto filePath = GetModulePath(GetModuleHandleW(NULL)).parent_path() / "common" / "text" / t;
        LoadCustomGXT(filePath);
    }

    return ret;
}

class GXTLoader
{
public:
    GXTLoader()
    {
        FusionFix::onInitEvent() += []()
        {
            auto pattern = find_pattern("53 56 57 FF 74 24 10 8B F1 E8 ? ? ? ? 8B 5C 24 14", "8B 44 24 04 53 56 57 50 8B F1 E8 ? ? ? ? 8B 5C 24 14");
            shsub_8C5D70 = safetyhook::create_inline(pattern.get_first(), sub_8C5D70);
        };
    }
} GXTLoader;