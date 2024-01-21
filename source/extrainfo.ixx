module;

#include <common.hxx>

export module extrainfo;

import common;
import comvars;

class ExtraInfo
{
public:
    ExtraInfo()
    {
        FusionFix::onInitEvent() += []()
        {
            CIniReader iniReader("");

            auto bExraInfo = iniReader.ReadInteger("MISC", "ExtraInfo", 1) != 0;

            if (bExraInfo)
            {
                auto pattern = hook::pattern("05 ? ? ? ? E9 ? ? ? ? E8 ? ? ? ? 84 C0");
                if (!pattern.empty())
                {
                    struct MS_PAUSED_HOOK
                    {
                        void operator()(SafetyHookContext& regs)
                        {
                            static std::wstring extra = L"";
                            regs.eax += 0x78;

                            if (pCGameConfigReader__ms_imgFiles)
                            {
                                auto s = std::wstring_view((wchar_t*)regs.eax);
                                auto imgNum = 0;
                                auto imgArrSize = 0;
                                for (auto& it : *pCGameConfigReader__ms_imgFiles)
                                {
                                    if (it.m_hFile != -1)
                                        imgNum++;
                                    imgArrSize++;
                                }
                                extra = s;
                                extra += L"~n~";
                                extra += L"                         ";
                                extra += L"IMG Files: " + std::to_wstring(imgNum) + L" / " + std::to_wstring(imgArrSize);
                                if (imgNum >= imgArrSize) extra += L"; WARNING: IMG Files limit reached!";

                                regs.eax = (uintptr_t)extra.c_str();
                            }
                        }
                    }; injector::MakeInline2<MS_PAUSED_HOOK>(pattern.get_first(0));
                }
            }
        };
    }
} ExtraInfo;