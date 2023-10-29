module;

#include <common.hxx>

export module frameratevigilante;

import common;

class FramerateVigilante
{
public:
    FramerateVigilante()
    {
        FusionFix::onInitEvent() += []()
        {
            // By Sergeanur
            auto pattern = hook::pattern("F3 0F 10 45 ? 51 8B CF F3 0F 11 04 24 E8 ? ? ? ? 8A 8F");
            if (!pattern.empty())
            {
                struct FramerateVigilanteHook1
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        float f = std::clamp(*(float*)(regs.ebp + 0x08), 1.0f / 150.0f, FLT_MAX);
                        _asm { movss xmm0, dword ptr[f] }
                    }
                }; injector::MakeInline<FramerateVigilanteHook1>(pattern.get_first(0));
            }
            else
            {
                pattern = hook::pattern("8B BE ? ? ? ? 33 C9 85 FF 7E 47 8A 5D 0C 33 D2 8D A4 24 ? ? ? ? 3B CF 7D 0A 8B 86 ? ? ? ? 03 C2 EB 02 33 C0 F6 80 ? ? ? ? ? 74 11 83 B8 ? ? ? ? ? 74 08 84 DB 0F 85 ? ? ? ? 83 C1 01 81 C2 ? ? ? ? 3B 8E ? ? ? ? 7C C5 D9 45 08 51 8B CE D9 1C 24 E8 ? ? ? ? 8A 86");
                struct FramerateVigilanteHook1
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        regs.edi = *(uint32_t*)(regs.esi + 0xFD4);
                        *(float*)(regs.ebp + 0x08) = std::clamp(*(float*)(regs.ebp + 0x08), 1.0f / 150.0f, FLT_MAX);
                    }
                }; injector::MakeInline<FramerateVigilanteHook1>(pattern.get_first(0), pattern.get_first(6));
            }
        };
    }
} FramerateVigilante;