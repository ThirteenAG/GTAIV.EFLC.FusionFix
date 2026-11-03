module;

#include <common.hxx>

export module lodlights;

import common;

// The game's own distant lights fade in only once the camera is higher than 75 m; on the consoles they are also
// drawn at street level (and in cutscenes). The distant lights of Project2DFX are a separate mod.
class LODLights
{
public:
    LODLights()
    {
        FusionFix::onInitEventAsync() += []()
        {
            static float f0 = 0.0f;
            auto pattern = find_pattern("F3 0F 5C 0D ? ? ? ? F3 0F 11 84 24 ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 59 0D ? ? ? ? 0F 2F C8 F3 0F 11 4C 24", "F3 0F 5C 05 ? ? ? ? F3 0F 59 05 ? ? ? ? 0F 2F C1 76 08 F3 0F 11 4C 24 ? EB 06");
            if (!pattern.empty())
            {
                static ProtectedGameRef<float*> fadeStartHeight;
                fadeStartHeight.SetAddress(pattern.get_first<float*>(4));
                fadeStartHeight = &f0;
            }
        };
    }
} LODLights;
