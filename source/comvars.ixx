module;

#include <common.hxx>
#include <D3D9Types.h>

export module comvars;

import common;

#define VALIDATE_SIZE(struc, size) static_assert(sizeof(struc) == size, "Invalid structure size of " #struc)

export namespace rage
{
    struct Vector3
    {
        float x, y, z;
    };

    struct Vector4
    {
        float x, y, z, w;
    };

    enum grcTextureFormat : uint8_t
    {
        GRCFMT_UNKNOWN,
        GRCFMT_R5G6B5,
        GRCFMT_A8R8G8B8,
        GRCFMT_R16F,
        GRCFMT_R32F,
        GRCFMT_A2R10G10B10,
        GRCFMT_A16B16G16R16F,
        GRCFMT_G16R16,
        GRCFMT_G16R16F,
        GRCFMT_A32B32G32R32F,
        GRCFMT_A16B16G16R16F2,
        GRCFMT_A16B16G16R16,
        GRCFMT_L8,
    };

    inline std::vector<std::pair<grcTextureFormat, D3DFORMAT>> m_Formats
    {
        { GRCFMT_UNKNOWN,        D3DFMT_UNKNOWN },
        { GRCFMT_R5G6B5,         D3DFMT_R5G6B5 },
        { GRCFMT_A8R8G8B8,       D3DFMT_A8R8G8B8 },
        { GRCFMT_R16F,           D3DFMT_R16F },
        { GRCFMT_R32F,           D3DFMT_R32F },
        { GRCFMT_A2R10G10B10,    D3DFMT_A2R10G10B10 },
        { GRCFMT_A16B16G16R16F,  D3DFMT_A16B16G16R16F },
        { GRCFMT_G16R16,         D3DFMT_G16R16 },
        { GRCFMT_G16R16F,        D3DFMT_G16R16F },
        { GRCFMT_A32B32G32R32F,  D3DFMT_A32B32G32R32F },
        { GRCFMT_A16B16G16R16F2, D3DFMT_A16B16G16R16F },
        { GRCFMT_A16B16G16R16,   D3DFMT_A16B16G16R16 },
        { GRCFMT_L8,             D3DFMT_L8 },
    };

    inline grcTextureFormat getEngineTextureFormat(D3DFORMAT format)
    {
        for (auto& pair : m_Formats)
        {
            if (pair.second == format)
                return pair.first;
        }
        return GRCFMT_UNKNOWN;
    }

    inline D3DFORMAT getD3DTextureFormat(grcTextureFormat format)
    {
        for (auto& pair : m_Formats)
        {
            if (pair.first == format)
                return pair.second;
        }
        return D3DFMT_UNKNOWN;
    }

    struct grcViewportWindow
    {
        float X;
        float Y;
        float Width;
        float Height;
        float MinZ;
        float MaxZ;
    };

    class grcViewport
    {
    public:
        float mWorldMatrix44[4][4];
        float mCameraMatrix[4][4];
        float mWorldMatrix[4][4];
        float mWorldViewMatrix[4][4];
        float mWorldViewProjMatrix[4][4];
        float mViewInverseMatrix[4][4];
        float mViewMatrix[4][4];
        float mProjectionMatrix[4][4];
        float mMatrix_200[4][4];//FrustumLRTB?
        float mMatrix_240[4][4];//FrustumNFNF?
        grcViewportWindow mGrcViewportWindow1;
        grcViewportWindow mGrcViewportWindow2;//UnclippedWindow?
        int mWidth;
        int mHeight;
        float mFov;
        float mAspect;
        float mNearClip;
        float mFarClip;
        float field_2C8;
        float field_2CC;
        float mScaleX;
        float mScaleY;
        float field_2D8;
        float field_2DC;
        Vector4 field_2E0;
        bool mIsPerspective;
        char gap_2f1[3];
        int field_2F4;
        int field_2F8;
        int field_2FC;
        Vector4 mFrustumClipPlanes[6];
        int field_360;
        int field_364;
        int field_368;
        int field_36C;
        int field_370;
        int field_374;
        int field_378;
        int field_37C;
        int field_380;
        int field_384;
        int field_388;
        int field_38C;
        int field_390;
        int field_394;
        int field_398;
        int field_39C;
        int field_3A0;
        int field_3A4;
        int field_3A8;
        int field_3AC;
        int field_3B0;
        int field_3B4;
        int field_3B8;
        int field_3BC;
        int field_3C0;
        int field_3C4;
        int field_3C8;
        int field_3CC;
        int field_3D0;
        int field_3D4;
        int field_3D8;
        int field_3DC;
        bool mInvertZInProjectionMatrix;
        char field_3E1[3];
        int field_3E4;
        int field_3E8;
        int field_3EC;
    };

    grcViewport** pCurrentViewport = nullptr;
    grcViewport* GetCurrentViewport()
    {
        return *pCurrentViewport;
    }

    VALIDATE_SIZE(grcViewport, 0x3F0);

    struct grcImage
    {
        uint16_t mWidth;
        uint16_t mHeight;
        int32_t mFormat;
        int32_t mType;
        uint16_t mStride;
        uint16_t mDepth;
        void* mPixelData;
        int32_t* field_14;
        grcImage* mNextLevel;
        grcImage* mNextSlice;
        uint32_t mRefCount;
        uint8_t gap24[8];
        int field_2C;
        float field_30[3];
        int32_t field_3C;
        float field_40[3];
        int field_4C;
    };

    VALIDATE_SIZE(grcImage, 0x50);

    class datBase
    {
    protected:
        uint32_t* _vft;
    };

    class pgBase : public datBase
    {
        uint32_t mBlockMap;
    };

    VALIDATE_SIZE(pgBase, 0x8);

    class grcRenderTargetPC;
    struct grcRenderTargetDesc
    {
        grcRenderTargetDesc()
            : field_0(0)
            , mMultisampleCount(0)
            , field_8(1)
            , mLevels(1)
            , field_10(1)
            , field_11(1)
            , field_1C(1)
            , field_24(1)
            , field_26(1)
            , field_28(1)
            , field_12(0)
            , mDepthRT(0)
            , field_18(0)
            , field_20(0)
            , field_25(0)
            , field_27(0)
            , field_29(0)
            , field_2A(0)
            , mFormat(GRCFMT_UNKNOWN)
        {}

        char field_0;
        int mMultisampleCount;
        char field_8;
        int mLevels;
        char field_10;
        char field_11;
        char field_12;
        grcRenderTargetPC* mDepthRT;
        char field_18;
        int field_1C;
        int field_20;
        bool field_24;
        char field_25;
        char field_26;
        char field_27;
        char field_28;
        char field_29;
        char field_2A;
        alignas(4) grcTextureFormat mFormat;
    };

    class grcRenderTarget : public pgBase
    {
    public:
        uint8_t field_8;
        uint8_t field_9;
        int16_t field_A;
        int32_t field_C;
        int32_t field_10;
        uint32_t mType;
        int32_t* field_18;
        int16_t field_1C;
        int16_t field_1E;
        int32_t field_20;
        int16_t field_24;
        int16_t field_26;
        int32_t field_28;
        uint16_t field_2C;
        uint16_t field_2E;
    };

    VALIDATE_SIZE(grcRenderTargetDesc, 0x30);

    class grcRenderTargetPC : public  grcRenderTarget
    {
    public:
        char* mName;
        IDirect3DTexture9* mD3DTexture;
        IDirect3DSurface9* mD3DSurface;
        uint16_t mWidth;
        uint16_t mHeight;
        grcTextureFormat mFormat;
        uint8_t mIndex;
        bool mBitsPerPixel;
        uint8_t mMultisampleCount;
        bool field_44;
        uint8_t gap45[2];
        uint8_t field_47;
        uint32_t mLevels;
        bool field_4C;
        bool field_4D;
        uint8_t field_4E;

        void Destroy(bool a2 = true)
        {
            auto func = (void(__thiscall*)(grcRenderTargetPC*, bool))(_vft[0]);
            func(this, a2);
        }
    };

    VALIDATE_SIZE(grcRenderTargetPC, 0x50);

    class grcTexturePC : pgBase
    {
    public:
        uint8_t field_8;
        uint8_t mDepth;
        uint16_t mRefCount;
        int32_t field_C;
        int32_t field_10;
        char* mName;
        IDirect3DTexture9* mD3DTexture;
        uint16_t mWidth;
        uint16_t mHeight;
        int32_t mFormat;
        uint16_t mStride;
        uint8_t mTextureType;
        uint8_t mMipCount;
        D3DVECTOR field_28;
        D3DVECTOR field_34;
        grcTexturePC* mPrevious;
        grcTexturePC* mNext;
        void* mPixelData;
        uint8_t field_4C;
        uint8_t field_4D;
        uint8_t field_4E;
        uint8_t field_4F;

        bool Init()
        {
            auto func = (grcTexturePC * (__thiscall*)(grcTexturePC*))(_vft[4]);
            return func(this);
        }
    };

    VALIDATE_SIZE(grcTexturePC, 0x50);

    namespace grcDevice
    {
        struct grcResolveFlags
        {
            grcResolveFlags()
                : Depth(1.0f)
                , BlurKernelSize(1.0f)
                , Color(0)
                , Stencil(0)
                , ColorExpBias(0)
                , ClearColor(false)
                , ClearDepthStencil(false)
                , BlurResult(false)
                , NeedResolve(true)
                , MipMap(true)
            {}

            float Depth;
            float BlurKernelSize;
            uint32_t Color;
            uint32_t Stencil;
            int ColorExpBias;
            bool ClearColor;
            bool ClearDepthStencil;
            bool BlurResult;
            bool NeedResolve;
            bool MipMap;
        };
    }

    class grcTextureFactoryPC;
    class grcTextureFactory
    {
    protected:
        uint32_t* _vft;

    public:
        bool field_4;
        char _gap5[3];

        static inline grcTextureFactoryPC** g_pTextureFactory = nullptr;
        static grcTextureFactoryPC* GetInstance()
        {
            return *g_pTextureFactory;
        }
    };

    class grcTextureFactoryPC : public grcTextureFactory
    {
    public:
        IDirect3DSurface9* mPrevRenderTargets[3];
        int32_t field_14;
        int32_t field_18;
        int32_t field_1C;
        int32_t field_20;
        int32_t field_24;
        IDirect3DSurface9* mD3D9Surfaces[4];
        uint8_t gap38[4];
        int32_t field_3C;
        int32_t field_40;
        int32_t field_44;
        IDirect3DSurface9* field_48;
        IDirect3DSurface9* mDepthStencilSurface;
        IDirect3DSurface9* field_50;
        grcRenderTargetPC* field_54;
        int32_t field_58;
        int32_t field_5C;
        int32_t field_60;
        int32_t field_64;
        int32_t field_68;
        int32_t field_6C;
        int32_t field_70;

        //virtuals
        grcTexturePC* Create(grcImage* image, void* arg2)
        {
            auto func = (grcTexturePC * (__stdcall*)(grcImage*, void*))(_vft[2]);
            return func(image, arg2);
        }

        grcRenderTargetPC* __stdcall CreateRenderTarget(const char* name, int32_t a2, uint32_t width, uint32_t height, uint32_t bitsPerPixel, grcRenderTargetDesc* desc)
        {
            auto func = (grcRenderTargetPC * (__stdcall*)(const char*, int32_t, uint32_t, uint32_t, uint32_t, grcRenderTargetDesc*))(_vft[14]);
            return func(name, a2, width, height, bitsPerPixel, desc);
        }

        void LockRenderTarget(uint32_t index, grcRenderTargetPC* color, grcRenderTargetPC* depth, uint32_t a5 = 0, bool a6 = 1, uint32_t mip = 0)
        {
            auto func = (void(__thiscall*)(grcTextureFactory*, uint32_t, grcRenderTargetPC*, grcRenderTargetPC*, uint32_t, bool, uint32_t))(_vft[15]);
            func(this, index, color, depth, a5, a6, mip);
        }

        void UnlockRenderTarget(uint32_t index, grcDevice::grcResolveFlags* resolveFlags, int32_t unused = -1)
        {
            auto func = (void(__thiscall*)(grcTextureFactory*, uint32_t, grcDevice::grcResolveFlags*, int32_t))(_vft[16]);
            func(this, index, resolveFlags, unused);
        }
    };

    VALIDATE_SIZE(grcTextureFactoryPC, 0x74);

    namespace grcDevice
    {
        IDirect3DDevice9** ms_pD3DDevice = nullptr;
        int32_t* ms_nActiveWidth = nullptr;
        int32_t* ms_nActiveHeight = nullptr;
        bool* ms_bNoBlockOnLostFocus = nullptr;

        IDirect3DDevice9* GetD3DDevice()
        {
            return *ms_pD3DDevice;
        }

        void* SetCallbackAddr;
        class FunctorBase
        {
        public:
            FunctorBase()
            {
                memset(mMemFunc, 0xAA, 8);
                mCallee = 0;
            }

            FunctorBase(void* callee, void(__fastcall* function)(), void* mf, uint32_t size)
            {
                auto SetCallback = (void(__thiscall *)(FunctorBase*, void*, void(__fastcall*)(), void*, uint32_t))SetCallbackAddr;
                SetCallback(this, callee, function, mf, size);
            }

            union
            {
                void(__fastcall* mFunction)();
                uint8_t mMemFunc[8];
            };

            uint32_t mCallee;
        };

        class Functor0 : public FunctorBase
        {
        public:
            Functor0(void* callee, void(__fastcall* function)(), void* mf, uint32_t size)
                : FunctorBase(callee, function, mf, size)
            {
                mThunk = Translator;
            }

        private:
            void(__cdecl* mThunk)(FunctorBase*);

            static void Translator(FunctorBase* functor)
            {
                functor->mFunction();
            }
        };

        VALIDATE_SIZE(Functor0, 0x10);

        void(__cdecl* RegisterDeviceCallbacks)(Functor0 onLost, Functor0 onReset);
    }

    enum eLightType
    {
        LT_POINT = 0x0,
        LT_DIR = 0x1,
        LT_SPOT = 0x2,
        LT_AO = 0x3,
        LT_CLAMPED = 0x4,
    };

    class CLightSource
    {
    public:
        Vector3 mDirection;
        int32_t field_C;
        Vector3 mTangent;
        int32_t field_1C;
        Vector3 mPosition;
        int32_t field_2C;
        Vector4 mColor;
        float mIntensity;
        eLightType mType;
        int32_t mFlags;
        int32_t mTxdId;
        int32_t field_50;
        float mRadius;
        float mInnerConeAngle;
        float mOuterConeAngle;
        int32_t field_60;
        int32_t field_64;
        int32_t field_68;
        int32_t field_6C;
        float mVolumeSize;
        float mVolumeScale;
        uint8_t gap78[8];
    };

    VALIDATE_SIZE(CLightSource, 0x80);

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

    struct grmShaderInfo_Parameter
    {
        char nbType;
        char nbCount;
        char nbValueLength;
        char nbAnnotationsCount;
        const char* pszName;
        const char* pszDescription;
        int dwNameHash;
        int dwDescriptionHash;
        int pAnnotations;
        void* pValue;
        int16_t m_wVertexFragmentRegister;
        int16_t m_wPixelFragmentRegister;
        int pdwParameterHashes;
        int field_24;
        int field_28;
        int field_2C;
    };

    VALIDATE_SIZE(grmShaderInfo_Parameter, 0x30);

    struct sysArray
    {
        int pData;
        int16_t wCount;
        int16_t wSize;
    };

    struct grmShaderInfo_Values
    {
        int ppValues;
        int pOwner;
        int dwSize;
        int dwTotalDataSize;
        int pnbArraySizes;
        int dwEffectHash;
        int field_18;
        int field_1C;
        int field_20;
    };

    class grmShaderInfo
    {
        sysArray m_techniques;
        sysArray m_parameters;
        sysArray m_vsFragments;
        sysArray m_psFragments;
        const char* m_pszShaderPath;
        int m_dwEffectHash;
        int m_dwFileTimeLow;
        int m_dwFileTimeHigh;
        grmShaderInfo_Values m_values;
        int m_pNext;

    public:
        static inline std::map<grmShaderInfo*, std::map<int, std::vector<uint8_t>>> ShaderInfoParams;

        static inline void* pfngetParamIndex = nullptr;
        static int getParamIndex(grmShaderInfo* instance, const char* name, int a3)
        {
            auto func = (int(__thiscall*)(grmShaderInfo* instance, const char* name, int a3))pfngetParamIndex;
            return func(instance, name, a3);
        }

        static inline SafetyHookInline shsub_436D70{};
        static void __fastcall setShaderParam(grmShaderInfo* _this, void* edx, void* a2, int index, void* in, int a5, int a6, int a7)
        {
            ShaderInfoParams[_this][index].assign((uint8_t*)in, (uint8_t*)in + a5);
            return shsub_436D70.fastcall(_this, edx, a2, index, in, a5, a6, a7);
        }

        static inline grmShaderInfo_Parameter* globalShaderParameters = nullptr;
        static inline uint32_t* dwGlobalShaderParameterCount = nullptr;

        static grmShaderInfo_Parameter* getGlobalParam(const char* name)
        {
            for (auto i = 0; i < *dwGlobalShaderParameterCount; i++)
            {
                if (std::string_view(name) == std::string_view(globalShaderParameters[i].pszName))
                    return &globalShaderParameters[i];
            }
            return nullptr;
        }

        static float* getParam(const char* shaderName, const char* paramName)
        {
            for (auto& it : ShaderInfoParams)
            {
                if (std::string_view(it.first->m_pszShaderPath).ends_with(shaderName))
                {
                    auto i = getParamIndex(it.first, paramName, 1);
                    if (i)
                        return reinterpret_cast<float*>(ShaderInfoParams[it.first][i].data());
                    else
                        break;
                }
            }
            return nullptr;
        }
    };
}

export namespace CViewport3DScene
{
    rage::grcRenderTargetPC** pGBufferRTs = nullptr;
    rage::grcRenderTargetPC** pStencilRT = nullptr;

    rage::grcRenderTargetPC* GetDiffuseRT()
    {
        return pGBufferRTs[0];
    }

    rage::grcRenderTargetPC* GetNormalRT()
    {
        return pGBufferRTs[1];
    }

    rage::grcRenderTargetPC* GetSpecularAoRT()
    {
        return pGBufferRTs[2];
    }

    rage::grcRenderTargetPC* GetDepthRT()
    {
        return pGBufferRTs[3];
    }

    rage::grcRenderTargetPC* GetStencilRT()
    {
        return *pStencilRT;
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

export namespace CTimeCycle
{
    void(__cdecl* Initialise)() = nullptr;
}

export int32_t* _dwCurrentEpisode;
export void* (__stdcall* getNativeAddress)(uint32_t);
export HWND gWnd;
export RECT gRect;
export bool bDynamicShadowForTrees;
export bool bMoreShadows = false;
export bool bLoadingShown = false;
export int bMenuNeedsUpdate = 0;
export int bMenuNeedsUpdate2 = 0;
export bool bEnableSnow = false;

export inline LONG getWindowWidth()
{
    return gRect.right - gRect.left;
}

export inline LONG getWindowHeight()
{
    return gRect.bottom - gRect.top;
}

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
        _dwCurrentEpisode = *find_pattern("83 3D ? ? ? ? ? 75 0F 6A 02", "89 35 ? ? ? ? 89 35 ? ? ? ? 6A 00 6A 01").get_first<int32_t*>(2);

        auto pattern = find_pattern("0A 05 ? ? ? ? 0A 05 ? ? ? ? 75 38", "0A 05 ? ? ? ? 0A 05");
        CTimer::m_UserPause = *pattern.get_first<uint8_t*>(2);
        CTimer::m_CodePause = *pattern.get_first<uint8_t*>(8);

        pattern = find_pattern("A1 ? ? ? ? A3 ? ? ? ? EB 3A", "A1 ? ? ? ? 39 05 ? ? ? ? 76 1F");
        CTimer::m_snTimeInMilliseconds = *pattern.get_first<int32_t*>(1);

        pattern = find_pattern("83 3D ? ? ? ? ? 74 17 8B 4D 14", "83 3D ? ? ? ? ? 74 15 8B 44 24 1C", "83 3D ? ? ? ? ? 74 EF");
        rage::grcDevice::ms_pD3DDevice = *pattern.get_first<IDirect3DDevice9**>(2);

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

        pattern = find_pattern("8B 0D ? ? ? ? 53 56 33 F6", "53 56 8B 35 ? ? ? ? 33 D2");
        rage::grcDevice::RegisterDeviceCallbacks = pattern.get_first<void(__cdecl)(rage::grcDevice::Functor0, rage::grcDevice::Functor0)>(0);

        pattern = find_pattern("8B 44 24 04 56 8B F1 85 C0 74 38", "8B 44 24 04 85 C0 56 8B F1 74 38");
        rage::grcDevice::SetCallbackAddr = pattern.get_first<void*>(0);

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

        pattern = find_pattern("8B 35 ? ? ? ? 75 14", "8B 3D ? ? ? ? 75 14 6A 00");
        rage::pCurrentViewport = *pattern.get_first<rage::grcViewport**>(2);

        pattern = find_pattern("A3 ? ? ? ? E8 ? ? ? ? 83 EC 0C", "A3 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 5E");
        rage::grcTextureFactory::g_pTextureFactory = *pattern.get_first<rage::grcTextureFactoryPC**>(1);

        pattern = find_pattern("A3 ? ? ? ? FF 35 ? ? ? ? 8B 01 FF 35 ? ? ? ? 6A 03 68 ? ? ? ? FF 50 38 8B 0D", "A3 ? ? ? ? 8B 01 8B 40 38 6A 03 68 ? ? ? ? FF D0 8B 0D ? ? ? ? A3 ? ? ? ? 8B 11 8B 52 38");
        CViewport3DScene::pGBufferRTs = *pattern.get_first<rage::grcRenderTargetPC**>(1);

        pattern = find_pattern("A3 ? ? ? ? FF 35 ? ? ? ? C7 44 24 ? ? ? ? ? 8B 01", "A3 ? ? ? ? C7 44 24 ? ? ? ? ? 8B 01 8B 40 38 6A 03 68 ? ? ? ? FF D0 80 7C 24");
        CViewport3DScene::pStencilRT = *pattern.get_first<rage::grcRenderTargetPC**>(1);

        pattern = find_pattern("55 8B EC 83 E4 F0 81 EC ? ? ? ? 8B 0D ? ? ? ? 53 0F B7 41 04", "55 8B EC 83 E4 F0 81 EC ? ? ? ? A1 ? ? ? ? 33 C4 89 84 24 ? ? ? ? 8B 0D ? ? ? ? 0F B7 41 04");
        CTimeCycle::Initialise = pattern.get_first<void(__cdecl)()>(0);

        pattern = find_pattern("56 57 6A 00 FF 74 24 10 8B F9 E8 ? ? ? ? 0F B7 77 0C", "8B 44 24 04 56 57 6A 00 50 8B F9");
        rage::grmShaderInfo::pfngetParamIndex = pattern.get_first<void*>(0);

        pattern = find_pattern("81 C6 ? ? ? ? 42 57", "81 C6 ? ? ? ? 55 A3");
        rage::grmShaderInfo::globalShaderParameters = *pattern.get_first<rage::grmShaderInfo_Parameter*>(2);
        
        pattern = find_pattern("8B 15 ? ? ? ? 8D 34 52", "8B 15 ? ? ? ? 83 C2 FF 33 C0");
        rage::grmShaderInfo::dwGlobalShaderParameterCount = *pattern.get_first<uint32_t*>(2);

        pattern = find_pattern("8B 54 24 08 85 D2 74 62", "56 8B 74 24 0C 85 F6 74 62");
        rage::grmShaderInfo::shsub_436D70 = safetyhook::create_inline(pattern.get_first(0), rage::grmShaderInfo::setShaderParam);
    }
} Common;