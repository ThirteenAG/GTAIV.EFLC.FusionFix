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
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");

            auto bExtraInfo = iniReader.ReadInteger("MISC", "ExtraInfo", 1) != 0;

            if (bExtraInfo)
            {
                auto pattern = hook::pattern("05 ? ? ? ? E9 ? ? ? ? E8 ? ? ? ? 84 C0");
                if (!pattern.empty())
                {
                    struct MS_PAUSED_HOOK
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            static std::wstring extra = L"";
                            regs.eax += 0x78;

                            if (CGameConfigReader::ms_imgFiles)
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
                                extra += L"IMG Files: " + std::to_wstring(imgNum) + L" / " + std::to_wstring(imgArrSize);
                                if (imgNum >= imgArrSize) extra += L"; WARNING: IMG Files limit reached!";

                                regs.eax = (uintptr_t)extra.c_str();
                            }
                        }
                    }; injector::MakeInline<MS_PAUSED_HOOK>(pattern.get_first(0));
                }
            }
        };
    }
} ExtraInfo;