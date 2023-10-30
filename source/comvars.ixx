module;

#include <common.hxx>

export module comvars;

import common;

export int32_t* _dwCurrentEpisode;
export uint8_t* CTimer__m_UserPause = nullptr;
export uint8_t* CTimer__m_CodePause = nullptr;
export int32_t* rage__scrEngine__ms_dwNativeTableSize;
export uint32_t** rage__scrEngine__ms_pNatives;
export void* (__stdcall* getNativeAddress)(uint32_t);
export float* fTimeStep;
export HWND gWnd;
export RECT gRect;
export bool bDynamicShadowForTrees;
export bool bMoreShadows = false;

class Common
{
public:
    Common()
    {
        FusionFix::onInitEvent() += []()
        {
            _dwCurrentEpisode = *find_pattern("83 3D ? ? ? ? ? 75 0F 6A 02", "89 35 ? ? ? ? 89 35 ? ? ? ? 6A 00 6A 01").get_first<int32_t*>(2);

            auto pattern = find_pattern("0A 05 ? ? ? ? 0A 05 ? ? ? ? 75 38", "0A 05 ? ? ? ? 0A 05");
            CTimer__m_UserPause = *pattern.get_first<uint8_t*>(2);
            CTimer__m_CodePause = *pattern.get_first<uint8_t*>(8);

            rage__scrEngine__ms_dwNativeTableSize = *find_pattern("8B 35 ? ? ? ? 85 F6 75 06 33 C0 5E C2 04 00 53 57 8B 7C 24 10", "8B 3D ? ? ? ? 85 FF 75 04 33 C0 5F C3").count(2).get(0).get<int32_t*>(2);
            rage__scrEngine__ms_pNatives = *find_pattern("8B 1D ? ? ? ? 8B CF 8B 04 D3 3B C7 74 19 8D 64 24 00 85 C0", "8B 1D ? ? ? ? 8B CE 8B 04 D3 3B C6 74 17 85 C0").count(2).get(0).get<uint32_t**>(2);

            pattern = hook::pattern("56 8B 35 ? ? ? ? 85 F6 75 06");
            if (!pattern.empty())
                getNativeAddress = pattern.count(2).get(1).get<void* (__stdcall)(uint32_t)>(0);

            pattern = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 59 05 ? ? ? ? 8B 43 20 53", "F3 0F 10 05 ? ? ? ? F3 0F 59 44 24 ? 83 C4 04 83 7C 24");
            fTimeStep = *pattern.get_first<float*>(4);
        };
    }
} Common;