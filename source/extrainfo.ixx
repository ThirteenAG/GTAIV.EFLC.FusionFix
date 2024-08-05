module;

#include <common.hxx>

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
                            extra += L"                         ";
                            extra += L"~p~IMG Files: " + std::to_wstring(imgNum) + L" / " + std::to_wstring(imgArrSize);
                            if (imgNum >= imgArrSize) extra += L"; ~r~WARNING: 255 IMG limit exceeded, will cause streaming issues.";
                        
                            static auto LamppostShadows = FusionFixSettings.GetRef("PREF_LAMPPOSTSHADOWS");
                            if (LamppostShadows->get())
                            {
                                extra += L"~n~";
                                extra += L"                         ";
                                extra += L"~r~WARNING: Lamppost shadows may cause visual artifacts and performance issues.";
                            }

                            static auto ViewDistance = FusionFixSettings.GetRef("PREF_VIEW_DISTANCE");
                            static auto DetailQuality = FusionFixSettings.GetRef("PREF_DETAIL_QUALITY");
                            if (ViewDistance->get() > 24 || DetailQuality->get() > 30)
                            {
                                extra += L"~n~";
                                extra += L"                         ";
                                if (ViewDistance->get() > 24 && DetailQuality->get() > 30)
                                    extra += L"~r~WARNING: View Distance slider above 25 and Detail Distance slider above 31 may cause object pop-in.";
                                else if (ViewDistance->get() > 24)
                                    extra += L"~r~WARNING: View Distance slider above 25 may cause object pop-in.";
                                else if (DetailQuality->get() > 30)
                                    extra += L"~r~WARNING: Detail Distance slider above 31 may cause object pop-in.";
                            }

                            regs.eax = (uintptr_t)extra.c_str();
                        }
                    });
                }
            }
        };
    }
} ExtraInfo;