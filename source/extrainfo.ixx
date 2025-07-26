module;

#include <common.hxx>
#include <psapi.h>

export module extrainfo;

import common;
import comvars;
import settings;

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
                auto pattern = hook::pattern("F3 0F 10 44 24 ? 6A FF 6A FF 50 83 EC 08 F3 0F 11 44 24 ? F3 0F 10 44 24 ? F3 0F 11 04 24 E8 ? ? ? ? 83 C4 14");
                if (!pattern.empty())
                {
                    static auto PauseHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
                    {
                        static std::wstring extra = L"";
                        
                        if (CGameConfigReader::ms_imgFiles && pMenuTab && *pMenuTab == 49)
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
                            extra += L"                    ";

                            auto FF_WARN0 = CText::getText("FF_WARN0");
                            extra += (FF_WARN0[0] ? FF_WARN0 : L"~p~IMG Files:") + std::wstring(L" ") + std::to_wstring(imgNum) + L" / " + std::to_wstring(imgArrSize);

                            ::PROCESS_MEMORY_COUNTERS pmc;
                            if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc)))
                            {
                                extra += L"; RAM: " + std::to_wstring(pmc.WorkingSetSize / 1000 / 1000) + L" MB";
                            }

                            auto FF_WARN1 = CText::getText("FF_WARN1");
                            if (imgNum >= imgArrSize) extra += FF_WARN1[0] ? FF_WARN1 : L"; ~r~WARNING: 255 IMG limit exceeded, will cause streaming issues.";
                        
                            /*
                            if (bExtraNightShadows || bHeadlightShadows)
                            {
                                extra += L"~n~";
                                extra += L"                    ";
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
                                    extra += L"                    ";
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