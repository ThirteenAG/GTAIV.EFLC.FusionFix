module;

#include <common.hxx>
#include <D3D9Types.h>

export module comvars;

import common;

#define VALIDATE_SIZE(struc, size) static_assert(sizeof(struc) == size, "Invalid structure size of " #struc)

export namespace rage
{
    namespace grcDevice
    {
        int32_t* ms_nActiveWidth = nullptr;
        int32_t* ms_nActiveHeight = nullptr;
        bool* ms_bNoBlockOnLostFocus = nullptr;
    }

    namespace scrEngine
    {
        uint32_t* ms_dwNativeTableSize;
        uint32_t** ms_pNatives;
    }

    namespace scrProgram
    {
        uintptr_t** ms_pGlobals;
        uint32_t* ms_pGlobalsSize;
    }

    namespace grcWindow
    {
        bool* ms_bWindowed;
        bool* ms_bOnTop;
        bool* ms_bFocusLost;
    }
}

export namespace CMenuManager
{
    bool* m_MenuActive = nullptr;
    uint8_t* bLoadscreenShown = nullptr;
}

export namespace CCutscenes
{
    uint32_t* m_dwCutsceneState;
}

export namespace CTimer
{
    float* fTimeStep;
    uint8_t* m_UserPause = nullptr;
    uint8_t* m_CodePause = nullptr;
    int32_t* m_snTimeInMilliseconds = nullptr;
}

export int32_t* _dwCurrentEpisode;
export void* (__stdcall* getNativeAddress)(uint32_t);
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

export struct TexFormat
{
    enum R_D3DFMT
    {
        R_D3DFMT_UNKNOWN,
        R_D3DFMT_R5G6B5,
        R_D3DFMT_A8R8G8B8,
        R_D3DFMT_R16F,
        R_D3DFMT_R32F,
        R_D3DFMT_A2R10G10B10,
        R_D3DFMT_A16B16G16R16F,
        R_D3DFMT_G16R16,
        R_D3DFMT_G16R16F,
        R_D3DFMT_A32B32G32R32F,
        R_D3DFMT_A16B16G16R16F2,
        R_D3DFMT_A16B16G16R16,
        R_D3DFMT_L8,
    };

    static inline std::vector<std::pair<R_D3DFMT, D3DFORMAT>> m_Formats
    {
        { R_D3DFMT_UNKNOWN,        D3DFMT_UNKNOWN },
        { R_D3DFMT_R5G6B5,         D3DFMT_R5G6B5 },
        { R_D3DFMT_A8R8G8B8,       D3DFMT_A8R8G8B8 },
        { R_D3DFMT_R16F,           D3DFMT_R16F },
        { R_D3DFMT_R32F,           D3DFMT_R32F },
        { R_D3DFMT_A2R10G10B10,    D3DFMT_A2R10G10B10 },
        { R_D3DFMT_A16B16G16R16F,  D3DFMT_A16B16G16R16F },
        { R_D3DFMT_G16R16,         D3DFMT_G16R16 },
        { R_D3DFMT_G16R16F,        D3DFMT_G16R16F },
        { R_D3DFMT_A32B32G32R32F,  D3DFMT_A32B32G32R32F },
        { R_D3DFMT_A16B16G16R16F2, D3DFMT_A16B16G16R16F },
        { R_D3DFMT_A16B16G16R16,   D3DFMT_A16B16G16R16 },
        { R_D3DFMT_L8,             D3DFMT_L8 },
    };

    static inline R_D3DFMT getEngineFormat(D3DFORMAT format)
    {
        for (auto& pair : m_Formats)
        {
            if (pair.second == format)
                return pair.first;
        }
        return R_D3DFMT_UNKNOWN;
    }

    static inline D3DFORMAT getD3DFormat(R_D3DFMT format)
    {
        for (auto& pair : m_Formats)
        {
            if (pair.first == format)
                return pair.second;
        }
        return D3DFMT_UNKNOWN;
    }
};

#pragma pack(push, 1)
struct CImgFile
{
    int m_nTimeLow;
    int m_nTimeHigh;
    int field_8;
    int field_C;
    char pszFilename[128];
    int m_pDevice;
    int field_94;
    int m_hFile;
    char field_9C;
    char field_9D;
    char m_bEpisode;
    char m_bEpisodicContent;
}; VALIDATE_SIZE(CImgFile, 160);
#pragma pack(pop)

export namespace CGameConfigReader
{
    CImgFile(*ms_imgFiles)[255];
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
            CTimer::m_UserPause = *pattern.get_first<uint8_t*>(2);
            CTimer::m_CodePause = *pattern.get_first<uint8_t*>(8);

            pattern = find_pattern("A1 ? ? ? ? A3 ? ? ? ? EB 3A", "A1 ? ? ? ? 39 05 ? ? ? ? 76 1F");
            CTimer::m_snTimeInMilliseconds = *pattern.get_first<int32_t*>(1);

            pattern = find_pattern("89 15 ? ? ? ? 89 0D ? ? ? ? 89 15 ? ? ? ? A3 ? ? ? ? E8", "8B 0D ? ? ? ? 89 4C 24 04 DB 44 24 04 D9 7C 24 04 0F B7 44 24 ? D8 0E");
            rage::grcDevice::ms_nActiveWidth = *pattern.get_first<int32_t*>(2);

            pattern = find_pattern("A3 ? ? ? ? A3 ? ? ? ? A1 ? ? ? ? 85 C9", "A1 ? ? ? ? 85 C0 74 1F F3 0F 2A 05");
            rage::grcDevice::ms_nActiveHeight = *pattern.get_first<int32_t*>(1);

            pattern = hook::pattern("80 3D ? ? ? ? ? 74 4B E8 ? ? ? ? 84 C0");
            CMenuManager::m_MenuActive = *pattern.get_first<bool*>(2);

            rage::scrEngine::ms_dwNativeTableSize = *find_pattern<2>("8B 35 ? ? ? ? 85 F6 75 06 33 C0 5E C2 04 00 53 57 8B 7C 24 10", "8B 3D ? ? ? ? 85 FF 75 04 33 C0 5F C3").count(2).get(0).get<uint32_t*>(2);
            rage::scrEngine::ms_pNatives = *find_pattern<2>("8B 1D ? ? ? ? 8B CF 8B 04 D3 3B C7 74 19 8D 64 24 00 85 C0", "8B 1D ? ? ? ? 8B CE 8B 04 D3 3B C6 74 17 85 C0").count(2).get(0).get<uint32_t**>(2);

            pattern = find_pattern("A3 ? ? ? ? A1 ? ? ? ? 33 DB F7 E7 0F 90 C3 6A 00", "A3 ? ? ? ? A1 ? ? ? ? 33 C9 BA ? ? ? ? F7 E2 0F 90 C1 6A 00 6A 10");
            rage::scrProgram::ms_pGlobals = *pattern.get_first<uintptr_t**>(1);
            rage::scrProgram::ms_pGlobalsSize = *pattern.get_first<uint32_t*>(6);

            pattern = find_pattern("80 3D ? ? ? ? ? 53 56 8A FA", "80 3D ? ? ? ? ? 53 8A 5C 24 1C");
            CMenuManager::bLoadscreenShown = *pattern.get_first<uint8_t*>(2);

            pattern = hook::pattern("56 8B 35 ? ? ? ? 85 F6 75 06");
            if (!pattern.empty())
                getNativeAddress = pattern.count(2).get(1).get<void* (__stdcall)(uint32_t)>(0);

            pattern = find_pattern("F3 0F 10 05 ? ? ? ? F3 0F 59 05 ? ? ? ? 8B 43 20 53", "F3 0F 10 05 ? ? ? ? F3 0F 59 44 24 ? 83 C4 04 83 7C 24");
            CTimer::fTimeStep = *pattern.get_first<float*>(4);

            pattern = hook::pattern("BE ? ? ? ? 8D 44 24 0C 50 8D 46 10 50");
            if (!pattern.empty())
                CGameConfigReader::ms_imgFiles = *pattern.get_first<decltype(CGameConfigReader::ms_imgFiles)>(1);

            pattern = hook::pattern("A1 ? ? ? ? 83 F8 08 74 05");
            CCutscenes::m_dwCutsceneState = *pattern.get_first<uint32_t*>(1);

            pattern = find_pattern("80 3D ? ? ? ? ? 74 0E 68 ? ? ? ? 53", "80 3D ? ? ? ? ? 74 13 68");
            rage::grcWindow::ms_bWindowed = *pattern.get_first<bool*>(2);

            pattern = find_pattern("80 3D ? ? ? ? ? 74 07 BF", "80 3D ? ? ? ? ? 74 07 B8 ? ? ? ? EB 02 33 C0 8B 55 F4");
            rage::grcWindow::ms_bOnTop = *pattern.get_first<bool*>(2);

            pattern = find_pattern("C6 05 ? ? ? ? ? 85 C0 74 02 FF D0 E8", "C6 05 ? ? ? ? ? 74 02 FF D0 C6 05");
            rage::grcWindow::ms_bFocusLost = *pattern.get_first<bool*>(2);

            pattern = find_pattern("80 3D ? ? ? ? ? 74 29 80 3D ? ? ? ? ? 75 1A", "80 3D ? ? ? ? ? 74 2F 80 3D ? ? ? ? ? 75 20");
            rage::grcDevice::ms_bNoBlockOnLostFocus = *pattern.get_first<bool*>(2);
        };
    }
} Common;