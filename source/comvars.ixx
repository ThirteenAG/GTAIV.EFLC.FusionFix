module;

#include <common.hxx>

export module comvars;

import common;

export int32_t* _dwCurrentEpisode;
export uint8_t* CTimer__m_UserPause = nullptr;
export uint8_t* CTimer__m_CodePause = nullptr;
export int32_t* CTimer__m_snTimeInMilliseconds = nullptr;
export int32_t* rage__grcDevice__ms_nActiveWidth = nullptr;
export int32_t* rage__grcDevice__ms_nActiveHeight = nullptr;
export bool* CMenuManager__m_MenuActive = nullptr;
export uint8_t* bLoadscreenShown = nullptr;
export uint32_t* rage__scrEngine__ms_dwNativeTableSize;
export uint32_t** rage__scrEngine__ms_pNatives;
export uintptr_t** rage__scrProgram__ms_pGlobals;
export uint32_t* rage__scrProgram__ms_pGlobalsSize;
export void* (__stdcall* getNativeAddress)(uint32_t);
export float* fTimeStep;
export HWND gWnd;
export RECT gRect;
export bool bDynamicShadowForTrees;
export bool bMoreShadows = false;
export bool bMainEndScene = false;
export bool bLoadingShown = false;
export int bMenuNeedsUpdate = 0;
export int bMenuNeedsUpdate2 = 0;

export inline LONG getWindowWidth()
{
    return gRect.right - gRect.left;
}

export inline LONG getWindowHeight()
{
    return gRect.bottom - gRect.top;
}

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

            pattern = find_pattern("A1 ? ? ? ? A3 ? ? ? ? EB 3A", "A1 ? ? ? ? 39 05 ? ? ? ? 76 1F");
            CTimer__m_snTimeInMilliseconds = *pattern.get_first<int32_t*>(1);

            pattern = find_pattern("89 15 ? ? ? ? 89 0D ? ? ? ? 89 15 ? ? ? ? A3 ? ? ? ? E8", "8B 0D ? ? ? ? 89 4C 24 04 DB 44 24 04 D9 7C 24 04 0F B7 44 24 ? D8 0E");
            rage__grcDevice__ms_nActiveWidth = *pattern.get_first<int32_t*>(2);

            pattern = find_pattern("A3 ? ? ? ? A3 ? ? ? ? A1 ? ? ? ? 85 C9", "A1 ? ? ? ? 85 C0 74 1F F3 0F 2A 05");
            rage__grcDevice__ms_nActiveHeight = *pattern.get_first<int32_t*>(1);

            pattern = hook::pattern("80 3D ? ? ? ? ? 74 4B E8 ? ? ? ? 84 C0");
            CMenuManager__m_MenuActive = *pattern.get_first<bool*>(2);

            rage__scrEngine__ms_dwNativeTableSize = *find_pattern<2>("8B 35 ? ? ? ? 85 F6 75 06 33 C0 5E C2 04 00 53 57 8B 7C 24 10", "8B 3D ? ? ? ? 85 FF 75 04 33 C0 5F C3").count(2).get(0).get<uint32_t*>(2);
            rage__scrEngine__ms_pNatives = *find_pattern<2>("8B 1D ? ? ? ? 8B CF 8B 04 D3 3B C7 74 19 8D 64 24 00 85 C0", "8B 1D ? ? ? ? 8B CE 8B 04 D3 3B C6 74 17 85 C0").count(2).get(0).get<uint32_t**>(2);

            pattern = find_pattern("A3 ? ? ? ? A1 ? ? ? ? 33 DB F7 E7 0F 90 C3 6A 00", "A3 ? ? ? ? A1 ? ? ? ? 33 C9 BA ? ? ? ? F7 E2 0F 90 C1 6A 00 6A 10");
            rage__scrProgram__ms_pGlobals = *pattern.get_first<uintptr_t**>(1);
            rage__scrProgram__ms_pGlobalsSize = *pattern.get_first<uint32_t*>(6);

            pattern = hook::pattern("56 8B 35 ? ? ? ? 85 F6 75 06");
            if (!pattern.empty())
                getNativeAddress = pattern.count(2).get(1).get<void* (__stdcall)(uint32_t)>(0);

            pattern = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 59 05 ? ? ? ? 8B 43 20 53", "F3 0F 10 05 ? ? ? ? F3 0F 59 44 24 ? 83 C4 04 83 7C 24");
            fTimeStep = *pattern.get_first<float*>(4);
        };
    }
} Common;