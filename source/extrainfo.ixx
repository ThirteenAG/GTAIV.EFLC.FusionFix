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
                            extra += L"                        ";

                            auto FF_WARN0 = CText::getText("FF_WARN0");
                            extra += (FF_WARN0[0] ? FF_WARN0 : L"~p~IMG Files:") + std::wstring(L" ") + std::to_wstring(imgNum) + L" / " + std::to_wstring(imgArrSize);

                            ::PROCESS_MEMORY_COUNTERS pmc;
                            if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc)))
                            {
                                extra += L"; RAM: " + std::to_wstring(pmc.WorkingSetSize / 1000 / 1000) + L" MB";
                            }

                            auto FF_WARN1 = CText::getText("FF_WARN1");
                            if (imgNum >= imgArrSize) extra += FF_WARN1[0] ? FF_WARN1 : L"; ~r~WARNING: 255 IMG limit exceeded, will cause streaming issues.";
                        
                            static auto LamppostShadows = FusionFixSettings.GetRef("PREF_LAMPPOSTSHADOWS");
                            if (LamppostShadows->get() || bHeadlightShadows)
                            {
                                extra += L"~n~";
                                extra += L"                        ";
                                auto FF_WARN2 = std::wstring(CText::getText("FF_WARN2"));
                                auto HeadlightShadow = CText::getText("HeadlightShadow");

                                if (bHeadlightShadows)
                                {
                                    auto pos = FF_WARN2.find(L": ");
                                    if (pos != std::wstring::npos) {
                                        FF_WARN2.replace(pos, 2, L": " + std::wstring(HeadlightShadow) + L" / ");
                                    }
                                }

                                if (FF_WARN2[0])
                                    extra += FF_WARN2;
                                else
                                    extra += L"~r~WARNING: Lamppost shadows may cause visual artifacts and performance issues.";
                            }

                            static auto ViewDistance = FusionFixSettings.GetRef("PREF_VIEW_DISTANCE");
                            static auto DetailQuality = FusionFixSettings.GetRef("PREF_DETAIL_QUALITY");
                            if (ViewDistance->get() > 24 || DetailQuality->get() > 30)
                            {
                                extra += L"~n~";
                                extra += L"                        ";
                                auto FF_WARN3 = CText::getText("FF_WARN3");
                                auto FF_WARN4 = CText::getText("FF_WARN4");
                                auto FF_WARN5 = CText::getText("FF_WARN5");
                                if (ViewDistance->get() > 24 && DetailQuality->get() > 30)
                                    extra += FF_WARN3[0] ? FF_WARN3 : L"~r~WARNING: View Distance slider above 25 and Detail Distance slider above 31 may cause object pop-in.";
                                else if (ViewDistance->get() > 24)
                                    extra += FF_WARN4[0] ? FF_WARN4 : L"~r~WARNING: View Distance slider above 25 may cause object pop-in.";
                                else if (DetailQuality->get() > 30)
                                    extra += FF_WARN5[0] ? FF_WARN5 : L"~r~WARNING: Detail Distance slider above 31 may cause object pop-in.";
                            }

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