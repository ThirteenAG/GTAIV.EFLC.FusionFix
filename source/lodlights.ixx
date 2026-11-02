module;

#include <common.hxx>

export module lodlights;

import common;

// Vanilla coastal lights have no height gate; inland lights require camera Z > 75 on PC and Xbox.
class LODLights
{
public:
    LODLights()
    {
        // FusionFix::onInitEventAsync() += []()
        // {
        //     static float f0 = 0.0f;
        //     auto pattern = find_pattern("F3 0F 5C 0D ? ? ? ? F3 0F 11 84 24 ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 59 0D ? ? ? ? 0F 2F C8 F3 0F 11 4C 24", "F3 0F 5C 05 ? ? ? ? F3 0F 59 05 ? ? ? ? 0F 2F C1 76 08 F3 0F 11 4C 24 ? EB 06");
        //     if (!pattern.empty())
        //     {
        //         static ProtectedGameRef<float*> fadeStartHeight;
        //         fadeStartHeight.SetAddress(pattern.get_first<float*>(4));
        //         fadeStartHeight = &f0;
        //     }
        // };
    }
} LODLights;
