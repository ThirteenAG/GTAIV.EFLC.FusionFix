module;

#include <common.hxx>

export module cutscenecam;

import common;
import comvars;

class CutsceneCam
{
public:
    CutsceneCam()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");
            auto bExtraCutsceneFix = iniReader.ReadInteger("MISC", "ExtraCutsceneFix", 1) != 0;

            // By Sergeanur
            auto pattern = find_pattern("74 20 83 FF 03 74 1B 83", "74 24 8B 44 24 2C");
            injector::WriteMemory<uint8_t>(pattern.get_first(), 0xEB, true);

            pattern = find_pattern("E8 ? ? ? ? 8B 4C 24 2C 5F 5E 33 CC B0 01", "E8 ? ? ? ? 8B 4C 24 2C 5F 5E 5B");

            static void* patchOffset = pattern.get_first();

            static void* originalHookster = injector::GetBranchDestination(patchOffset).get<void*>();

            pattern = find_pattern("C6 44 24 ? ? A1 ? ? ? ? 83 FF 03", "C6 44 24 ? ? 83 F9 03");
            static void* originalHooksterBytePatch = pattern.get_first(4);
            static double incrementalTimeStep = 0.0;

            struct CutsceneCamJitterWorkaround
            {
                float data[320];

                bool OriginalHookster(float a2)
                {
                    return ((bool(__thiscall*)(CutsceneCamJitterWorkaround*, float))originalHookster)(this, a2);
                }

                bool Hookster(float a2)
                {
#if 1
                    //idk what's going on here, but seems better than with timestep check
                    //incrementalTimeStep += *fTimeStep;

                    CutsceneCamJitterWorkaround temp = *this;

                    injector::WriteMemory<uint8_t>(originalHooksterBytePatch, 1, true);
                    bool result = OriginalHookster(a2) != 0.0;

                    CutsceneCamJitterWorkaround temp2 = *this;

                    ///if (incrementalTimeStep < 0.3333)
                    //    return result;

                    *this = temp;

                    injector::WriteMemory<uint8_t>(originalHooksterBytePatch, 0, true);
                    bool result2 = OriginalHookster(a2) != 0.0;

                    temp = *this;

                    if (fabs(temp.data[8] - temp2.data[8]) > 0.03333f
                        || fabs(temp.data[9] - temp2.data[9]) > 0.03333f
                        || fabs(temp.data[10] - temp2.data[10]) > 0.03333f
                        || fabs(temp.data[16] - temp2.data[16]) > 0.3333f
                        || fabs(temp.data[17] - temp2.data[17]) > 0.3333f
                        || fabs(temp.data[18] - temp2.data[18]) > 0.3333f)
                    {
                        //incrementalTimeStep = 0.0;
                        *this = temp2;
                        return result;
                    }
                    return result2;
#else
                    return OriginalHookster(a2) != 0.0;
#endif
                }
            };

            auto dest = &CutsceneCamJitterWorkaround::Hookster;
            injector::MakeCALL(patchOffset, *(void**)&dest, true);

            pattern = find_pattern("E8 ? ? ? ? 8B CD 88 44 24 0F", "E8 ? ? ? ? 8B CF 88 44 24 0F");
            injector::MakeCALL(pattern.get_first(), *(void**)&dest, true);

            if (bExtraCutsceneFix)
            {
                // ???
                pattern = hook::pattern("F3 0F 11 86 ? ? ? ? 5E 5B 8B 4C 24 30 33 CC E8 ? ? ? ? 83 C4 34 C2 04 00");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(), 8, true);

                pattern = hook::pattern("F3 0F 11 86 ? ? ? ? 5F 5E B8 ? ? ? ? 5B 8B 4C 24 30 33 CC E8 ? ? ? ? 83 C4 34 C2 04 00");
                if (!pattern.empty())
                    injector::MakeNOP(pattern.get_first(), 8, true);
            }
        };
    }
} CutsceneCam;