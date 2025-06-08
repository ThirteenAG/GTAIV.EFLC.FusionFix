module;

#include <common.hxx>

export module frameratevigilante;

import common;
import comvars;

injector::hook_back<double(__fastcall*)(void* _this, void* edx, void* a2, void* a3)> hbsub_A18510;
double __fastcall sub_A18510(void* _this, void* edx, void* a2, void* a3)
{
    return hbsub_A18510.fun(_this, edx, a2, a3) * (*CTimer::fTimeStep / (1.0f / 30.0f));
}

SafetyHookInline NATIVE_SLIDE_OBJECT_C_T{};
DWORD* __cdecl NATIVE_SLIDE_OBJECT_C(DWORD* a1) {
    DWORD* v1 = (DWORD*)a1[2];
    float* v2 = (float*)v1;
    rage::Vector3* delta = (rage::Vector3*)&v2[4];

    delta->x *= (*CTimer::fTimeStep / (1.0f / 30.0f));
    delta->y *= (*CTimer::fTimeStep / (1.0f / 30.0f));
    delta->z *= (*CTimer::fTimeStep / (1.0f / 30.0f));

    return NATIVE_SLIDE_OBJECT_C_T.ccall<DWORD*>(a1);
}
class FramerateVigilante
{
public:
    FramerateVigilante()
    {
        FusionFix::onInitEventAsync() += []()
        {
            // Handbrake Cam (test)
            auto pattern = find_pattern("E8 ? ? ? ? D9 5C 24 7C F3 0F 10 4C 24", "E8 ? ? ? ? D9 5C 24 70 F3 0F 10 44 24 ? F3 0F 58 86");
            hbsub_A18510.fun = injector::MakeCALL(pattern.get_first(0), sub_A18510).get();

            // By Sergeanur
            pattern = hook::pattern("F3 0F 10 45 ? 51 8B CF F3 0F 11 04 24 E8 ? ? ? ? 8A 8F");
            if (!pattern.empty())
            {
                struct FramerateVigilanteHook1
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        float f = std::clamp(*(float*)(regs.ebp + 0x08), 1.0f / 150.0f, FLT_MAX);
                        *(float*)(regs.ebp + 0x08) = f;
                        regs.xmm0.f32[0] = f;
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

            pattern = hook::pattern("F3 0F 10 05 ? ? ? ? F3 0F 58 C1 F3 0F 11 05 ? ? ? ? EB 36");
            if (!pattern.empty())
            {
                static auto f1032790 = *pattern.get_first<float*>(4);
                struct LoadingTextSpeed
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        regs.xmm0.f32[0] = *f1032790 * *CTimer::fTimeStep;
                        regs.xmm0.f32[0] += regs.xmm1.f32[0];
                    }
                }; injector::MakeInline<LoadingTextSpeed>(pattern.get_first(0), pattern.get_first(12));
            }

            pattern = hook::pattern("F3 0F 58 0D ? ? ? ? 0F 5B C0 F3 0F 11 0D");
            if (!pattern.empty())
            {
                struct LoadingTextSparks
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        regs.xmm1.f32[0] += 0.085f * *CTimer::fTimeStep;
                    }
                }; injector::MakeInline<LoadingTextSparks>(pattern.get_first(0), pattern.get_first(8));
            }
            else
            {
                pattern = hook::pattern("F3 0F 58 05 ? ? ? ? F3 0F 2A 0D");
                struct LoadingTextSparks
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        regs.xmm0.f32[0] += 0.085f * *CTimer::fTimeStep;
                    }
                }; injector::MakeInline<LoadingTextSparks>(pattern.get_first(0), pattern.get_first(8));
            }

            // CD/busy spinner
            pattern = hook::pattern("F3 0F 58 05 ? ? ? ? 33 C0 A3 ? ? ? ? F3 0F 11 05");
            if (!pattern.empty())
            {
                struct CDSpinnerHook
                {
                    void operator()(injector::reg_pack& regs)
                    {
                        regs.xmm0.f32[0] += *CTimer::fTimeStep * 5.0f;
                    }
                }; injector::MakeInline<CDSpinnerHook>(pattern.get_first(0), pattern.get_first(8));
            }
            
            // Cop blips
            pattern = hook::pattern("F3 0F 10 4C 24 ? 0F 28 C1 F3 0F 59 C2");
            if (!pattern.empty())
            {
                static int CustomFrameCounter = 0;

                static auto CounterHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
                {
                    static float accumulator = 0.0f;
                    accumulator += (*CTimer::fTimeStep / (1.0f / 30.0f));
                    int increment = static_cast<int>(accumulator);
                    CustomFrameCounter += increment;
                    accumulator -= increment;
                });

                pattern = hook::pattern("A1 ? ? ? ? 6B C0 15");
                if (!pattern.empty())
                    injector::WriteMemory(pattern.get_first(1), &CustomFrameCounter, true);
            }

            // NATIVE_SLIDE_OBJECT speed-up fix
            // fixes Windows Cleaning platforms and sliding gates
            // Acceleration/Deacceleration is still broken for Window Cleaning Platforms
            pattern = hook::pattern("56 8B 74 24 ? 8B 46 ? 83 78 ? ? F3 0F 10 40 ? F3 0F 10 48 ? F3 0F 10 50 ? F3 0F 10 58 ? F3 0F 10 60 ? F3 0F 10 68 ? 0F 95 44 24 ? FF 74 24 ? 83 EC ? F3 0F 11 44 24 ? F3 0F 11 4C 24 ? F3 0F 11 54 24 ? F3 0F 11 5C 24 ? F3 0F 11 64 24 ? F3 0F 11 2C 24 FF 30 E8 ? ? ? ? 0F B6 C8 8B 06 83 C4 ? 89 08 5E C3 CC CC CC CC 56 8B 74 24 ? 51");
            if (!pattern.empty()) {
                NATIVE_SLIDE_OBJECT_C_T = safetyhook::create_inline(pattern.get_first(), &NATIVE_SLIDE_OBJECT_C);
            }

        };
    }
} FramerateVigilante;