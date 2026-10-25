module;

#include <common.hxx>

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
            std::vector<BYTE> versionInfo(size);
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

                wchar_t* gitHash = nullptr;
                UINT gitHashLen = 0;
                if (VerQueryValueW(versionInfo.data(), L"\\StringFileInfo\\040004b0\\GitHash", (LPVOID*)&gitHash, &gitHashLen) && gitHashLen > 1)
                {
                    versionStr += L"-" + std::wstring(gitHash);
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
            SettingsReader reader;

            static auto bExtraInfo = reader.ReadInteger("MISC", "ExtraInfo", 1) != 0;

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
                    ualVer = L"ASI Loader: v" + GetModuleVersion(m) + L" (" + GetModulePath(m).filename().wstring() + L")";
                    break;
                }
            }

            // The game's info line below the menu: the versions (Game) and the IMG files and streaming memory (Graphics).
            // Descriptions of Fusion Fix's rows are the menu's info text at the top right (settings.ixx).
            auto pattern = find_pattern("F3 0F 10 44 24 ? 6A FF 6A FF 50 83 EC 08 F3 0F 11 44 24 ? F3 0F 10 44 24 ? F3 0F 11 04 24 E8 ? ? ? ? 83 C4 14", "D9 44 24 0C 6A FF 6A FF 52 83 EC 08 D9 5C 24 04 D9 44 24 1C D9 1C 24 E8 ? ? ? ? 83 C4 14");
            if (!pattern.empty())
            {
                static auto reg = *pattern.get_first<uint8_t>(10);
                static auto PauseHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                {
                    static std::wstring extra = L"";
                    static constexpr std::wstring_view indent = L"                        ";

                    bool info = bExtraInfo && CGameConfigReader::ms_imgFiles && pMenuTab && (*pMenuTab == 49 || *pMenuTab == 0); // Graphics || Game
                    // A setting applied after a restart was changed (the graphics API, Skip Intro, a plugin's): the
                    // game's restart warning, on every tab
                    bool restart = CSettings::RestartPending();
                    if (!info && !restart)
                        return;

                    extra = std::wstring_view((wchar_t*)(reg == 0x50 ? regs.eax : regs.edx));
                    if (info && *pMenuTab == 0)
                    {
                        extra += L"~n~" + std::wstring(indent);

                        if (!exeVer.empty())
                            extra += L"~p~" + exeVer;

                        if (!asiVer.empty())
                            extra += L" / " + asiVer;

                        if (!ualVer.empty())
                            extra += L" / " + ualVer;
                    }

                    if (info && *pMenuTab == 49)
                    {
                        auto imgNum = 0;
                        auto imgArrSize = 0;
                        for (auto& it : *CGameConfigReader::ms_imgFiles)
                        {
                            if (it.m_hFile != -1)
                                imgNum++;
                            imgArrSize++;
                        }

                        {
                            extra += L"~n~" + std::wstring(indent);
                            auto FF_WARN0 = CText::getText("FF_WARN0");
                            extra += (FF_WARN0[0] ? FF_WARN0 : L"~p~IMG Files:") + std::wstring(L" ") + std::to_wstring(imgNum) + L" / " + std::to_wstring(imgArrSize);

                            // Available physical memory decreases as used memory increases, used plus available is the total
                            constexpr uint32_t MB = 1024 * 1024;
                            uint32_t nUsedPhysical = (CStreamingEngine::ms_info->m_PhysicalUsed + MB / 2) / MB;
                            uint32_t nMaxPhysical = (CStreamingEngine::ms_info->m_ResourcePhysicalAvailable + MB / 2) / MB;
                            uint32_t nUsedVirtual = (CStreamingEngine::ms_info->m_VirtualUsed + MB / 2) / MB;
                            uint32_t nMaxVirtual = (CStreamingEngine::ms_info->m_ResourceVirtualMax + MB / 2) / MB;
                            extra += std::format(L"; Streaming memory: {} / {} MB; Virtual heap size: {} / {} MB", nUsedPhysical, nUsedPhysical + nMaxPhysical, nUsedVirtual, nMaxVirtual);
                        }

                        auto FF_WARN1 = CText::getText("FF_WARN1");
                        if (imgNum >= imgArrSize)
                            extra += FF_WARN1[0] ? FF_WARN1 : L"; ~r~WARNING: 255 IMG limit exceeded, will cause streaming issues.";
                    }

                    if (restart)
                    {
                        auto FF_RESTART = CText::getText("FF_RESTART");
                        extra += L"~n~" + std::wstring(indent) + L"~r~" +
                            (FF_RESTART && FF_RESTART[0] ? std::wstring(FF_RESTART) : std::wstring(L"GTA IV must be restarted in order to apply settings."));
                    }

                    if (reg == 0x50)
                        regs.eax = (uintptr_t)extra.c_str();
                    else
                        regs.edx = (uintptr_t)extra.c_str();
                });
            }
        };
    }
} ExtraInfo;