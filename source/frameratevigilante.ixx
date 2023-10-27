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
        };
    }
} FramerateVigilante;