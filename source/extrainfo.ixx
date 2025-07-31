module;

#include <common.hxx>
#include <psapi.h>
#include <format>

export module extrainfo;

import common;
import comvars;
import settings;

std::wstring GetModuleVersion(HMODULE hModule)
{
    std::wstring versionStr;
    WCHAR filePath[MAX_PATH];

    if (GetModuleFileNameW(hModule, filePath, MAX_PATH))
    {
        DWORD dummy;
        DWORD size = GetFileVersionInfoSizeW(filePath, &dummy);
        if (size > 0)
        {
            std::vector<BYTE> versionInfo(size); // RAII-managed buffer
            if (GetFileVersionInfoW(filePath, 0, size, versionInfo.data()))
            {
                VS_FIXEDFILEINFO* fileInfo;
                UINT len;
                if (VerQueryValueW(versionInfo.data(), L"\\", (LPVOID*)&fileInfo, &len))
                {
                    DWORD major = (fileInfo->dwFileVersionMS >> 16) & 0xFFFF;
                    DWORD minor = (fileInfo->dwFileVersionMS) & 0xFFFF;
                    DWORD build = (fileInfo->dwFileVersionLS >> 16) & 0xFFFF;
                    DWORD revision = (fileInfo->dwFileVersionLS) & 0xFFFF;
                    versionStr = std::format(L"{}.{}.{}.{}", major, minor, build, revision);
                }
            }
        }
    }
    return versionStr;
}

class ExtraInfo
{
public:
    ExtraInfo()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");

            auto bExtraInfo = iniReader.ReadInteger("MISC", "ExtraInfo", 1) != 0;

            if (bExtraInfo)
            {
                HMODULE hm = NULL;
                GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&GetModuleVersion, &hm);
                static auto asiVer = L"Fusion Fix: v" + GetModuleVersion(hm);
                static auto exeVer = GetExeModuleName().wstring() + L": v" + GetModuleVersion(NULL);
                static std::wstring ualVer = L"";

                ModuleList dlls;
                dlls.Enumerate(ModuleList::SearchLocation::LocalOnly);
                for (auto& e : dlls.m_moduleList)
                {
                    auto m = std::get<HMODULE>(e);
                    if (IsModuleUAL(m))
                    {
                        ualVer = L"ASI Loader: v" + GetModuleVersion(m);
                        break;
                    }
                }

                auto pattern = hook::pattern("F3 0F 10 44 24 ? 6A FF 6A FF 50 83 EC 08 F3 0F 11 44 24 ? F3 0F 10 44 24 ? F3 0F 11 04 24 E8 ? ? ? ? 83 C4 14");
                if (!pattern.empty())
                {
                    static auto PauseHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        static std::wstring extra = L"";
                        
                        if (CGameConfigReader::ms_imgFiles && pMenuTab && (*pMenuTab == 49 || *pMenuTab == 0))
                        {
                            auto s = std::wstring_view((wchar_t*)regs.eax);
                            auto imgNum = 0;
                            auto imgArrSize = 0;
                            for (auto& it : *CGameConfigReader::ms_imgFiles)
                            {
                                if (it.m_hFile != -1)
                                    imgNum++;
                                imgArrSize++;
                            }
                            extra = s;
                            extra += L"~n~";
                            extra += L"                        ";

                            if (*pMenuTab == 49)
                            {
                                auto FF_WARN0 = CText::getText("FF_WARN0");
                                extra += (FF_WARN0[0] ? FF_WARN0 : L"~p~IMG Files:") + std::wstring(L" ") + std::to_wstring(imgNum) + L" / " + std::to_wstring(imgArrSize);

                                ::PROCESS_MEMORY_COUNTERS pmc;
                                if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc)))
                                {
                                    extra += L"; RAM: " + std::to_wstring(pmc.WorkingSetSize / 1000 / 1000) + L" MB";
                                }
                            }

                            if (*pMenuTab == 0)
                            {
                                if (!exeVer.empty())
                                    extra += L"~p~" + exeVer;

                                if (!asiVer.empty())
                                    extra += L" / " + asiVer;

                                if (!ualVer.empty())
                                    extra += L" / " + ualVer;
                            }

                            if (*pMenuTab == 49)
                            {
                                auto FF_WARN1 = CText::getText("FF_WARN1");
                                if (imgNum >= imgArrSize) extra += FF_WARN1[0] ? FF_WARN1 : L"; ~r~WARNING: 255 IMG limit exceeded, will cause streaming issues.";
                            }

                            /*
                            if (bExtraNightShadows || bHeadlightShadows)
                            {
                                extra += L"~n~";
                                extra += L"                        ";
                                auto FF_WARN2 = std::wstring(CText::getText("FF_WARN2"));

                                if (FF_WARN2[0])
                                    extra += FF_WARN2;
                                else
                                    extra += L"~r~WARNING: Extra Night Shadows is an original PC option by Rockstar. Extremely broken, not recommended.";
                            }
                            */


                            if (FusionFixSettings.Get("PREF_SHADOWFILTER") == FusionFixSettings.ShadowFilterText.eCHSS)
                            {
                                if (FusionFixSettings.Get("PREF_SHADOW_QUALITY") < 4) // Very High
                                {
                                    extra += L"~n~";
                                    extra += L"                        ";
                                    auto FF_WARN6 = CText::getText("FF_WARN6");
                                    extra += FF_WARN6[0] ? FF_WARN6 : L"~r~WARNING: CHSS only takes effect with Shadow Quality set to Very High.";
                                }
                            }

                            regs.eax = (uintptr_t)extra.c_str();
                        }
                    });
                }
            }
        };
    }
} ExtraInfo;