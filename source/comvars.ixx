module;

#include <common.hxx>
#include <D3D9Types.h>

export module comvars;

import common;

#define VALIDATE_SIZE(struc, size) static_assert(sizeof(struc) == size, "Invalid structure size of " #struc)

export unsigned int hashStringLowercaseFromSeed(const char* str, unsigned int seed)
{
    auto hash = seed;
    auto currentChar = str;
    
    if (*str == '"')
        currentChar = str + 1;
    
    while (*currentChar)
    {
        char character = *currentChar;
    
        if (*str == '"' && character == '"')
            break;
    
        ++currentChar;
    
        if ((uint8_t)(character - 'A') <= 25)
        {
            character += 32; // Convert uppercase to lowercase
        }
        else if (character == '\\')
        {
            character = '/';
        }
    
        hash = (1025 * (hash + character) >> 6) ^ 1025 * (hash + character);
    }
    
    return 32769 * (9 * hash ^ (9 * hash >> 11));
}

export class CBaseDC
{
public:
    uint32_t field_1;

public:
    static inline void* AppendAddr;
    void Append()
    {
        reinterpret_cast<void(__thiscall*)(CBaseDC*)>(AppendAddr)(this);
    }

    static inline void* operator_newAddr;
    void* operator new(std::size_t size)
    {
        return reinterpret_cast<void* (__cdecl*)(std::size_t, int32_t)>(operator_newAddr)(size, 0);
    }

    virtual ~CBaseDC() {}
    virtual void DrawCommand() {}
    virtual int32_t GetSize() { return sizeof(CBaseDC); }
};

export class T_CB_Generic_NoArgs : public CBaseDC
{
public:
    void (*cb)();

public:
    T_CB_Generic_NoArgs(void (*c)()) : CBaseDC()
    {
        cb = c;
    }

    void DrawCommand() override
    {
        cb();
    }

    int32_t GetSize() override
    {
        return sizeof(T_CB_Generic_NoArgs);
    }
};

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
        GRCFMT_D24S8 = 0xE,
        GRCFMT_X8R8G8B8 = 0x10,
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
        { GRCFMT_D24S8,          D3DFMT_D24S8 },
        { GRCFMT_X8R8G8B8,       D3DFMT_X8R8G8B8 },
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
        uint8_t mBitsPerPixel;
        uint8_t mMultisampleCount;
        bool field_44;
        uint8_t gap45[2];
        uint8_t field_47;
        uint32_t mLevels;
        bool field_4C;
        bool field_4D;
        uint8_t field_4E;

        void Destroy(uint8_t a2 = 1)
        {
            auto func = (void(__thiscall*)(grcRenderTargetPC*, uint8_t))(_vft[0]);
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

    struct grcTextureReferenceBase : public pgBase
    {
        char field_8;
        char field_9;
        int16_t m_wUsageCount;
        int field_C;
    };

    struct grcTextureReference : public grcTextureReferenceBase
    {
        int field_10;
        const char* m_pszName;
        grcTexturePC* m_pTexture;
    };

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
                auto SetCallback = (void(__thiscall*)(FunctorBase*, void*, void(__fastcall*)(), void*, uint32_t))SetCallbackAddr;
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

        static inline std::unordered_map<std::string, grcTexturePC*> TextureCache;
        static inline std::unordered_map<std::string, grcRenderTargetPC*> RTCache;
        static inline SafetyHookInline shCreateTexture{};
        static grcTexturePC* __fastcall CreateTexture(grcTextureFactoryPC* _this, void* edx, const char* name, void* a3)
        {
            auto ret = shCreateTexture.fastcall<grcTexturePC*>(_this, edx, name, a3);
            TextureCache[name] = ret;
            return ret;
        }

        static inline SafetyHookInline shCreateRT{};
        static grcRenderTargetPC* __stdcall CreateRT(const char* name, int32_t a2, uint32_t width, uint32_t height, uint32_t bitsPerPixel, grcRenderTargetDesc* desc)
        {
            // Scale phone screen/phone camera rendertarget resolution with game resolution.
            if(std::string_view(name) == "PHONE_SCREEN" || std::string_view(name) == "PHOTO")
            {
                auto res = (int32_t)(std::ceil((float)*rage::grcDevice::ms_nActiveHeight / 720.0f) * 256.0f);
                width  = res;
                height = res;
            }

            // Force water surface rendertarget resolution to always be 256x256. This matches the water tiling on the console version.
            if(std::string_view(name) == "WATER_SURFACE0_COLOUR" || std::string_view(name) == "WATER_SURFACE1_COLOUR")
            {
                width  = 256;
                height = 256;
            }

            auto ret = shCreateRT.stdcall<grcRenderTargetPC*>(name, a2, width, height, bitsPerPixel, desc);
            
            //     if(strcmp("WATER_REFLECTION_COLOUR", name) == 0) { ret->mMultisampleCount=1; }
            //else if(strcmp("WATER_REFLECTION_DEPTH" , name) == 0) { ret->mMultisampleCount=1; }
            //else if(strcmp("REFLECTION_MAP_COLOUR"  , name) == 0) { ret->mMultisampleCount=1; }
            //else if(strcmp("REFLECTION_MAP_DEPTH"   , name) == 0) { ret->mMultisampleCount=1; }

            RTCache[name] = ret;
            return ret;
        }

        static grcTexturePC* GetTextureByName(const char* name)
        {
            if (TextureCache.contains(name))
                return TextureCache[name];
            return nullptr;
        }

        static grcRenderTargetPC* GetRTByName(const char* name)
        {
            if (RTCache.contains(name))
                return RTCache[name];
            return nullptr;
        }

        static grcTexturePC* FindTextureByRawPointer(IDirect3DTexture9* ptr)
        {
            for (auto& [name, tex] : TextureCache)
            {
                if (tex->mD3DTexture == ptr)
                    return tex;
            }
            return nullptr;
        }

        static grcRenderTargetPC* FindRTByRawPointer(IDirect3DTexture9* ptr)
        {
            for (auto& [name, rt] : RTCache)
            {
                if (rt->mD3DTexture == ptr)
                    return rt;
            }
            return nullptr;
        }
    };

    VALIDATE_SIZE(grcTextureFactoryPC, 0x74);

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
        static inline std::unordered_map<int, std::vector<uint8_t>> GlobalParams;
        static inline std::map<unsigned int, std::map<int, std::vector<uint8_t>>> ShaderInfoParams;
        static inline std::map<unsigned int, std::pair<int, int>> ShaderInfoParamHashes;

        static inline void* pfngetParamIndex = nullptr;
        static int getParamIndex(grmShaderInfo* instance, const char* name, int a3)
        {
            auto func = (int(__thiscall*)(grmShaderInfo* instance, const char* name, int a3))pfngetParamIndex;
            return func(instance, name, a3);
        }

        static int getParamIndex(unsigned int shader_hash, unsigned int name_hash, int a3)
        {
            auto j = 1;
            for (auto i = (int*)(ShaderInfoParamHashes[shader_hash].first + 0xC); j < ShaderInfoParamHashes[shader_hash].second; i += 0xC, ++j)
            {
                if (i[0] == name_hash || i[1] == name_hash)
                    return j;
            }
            return 0;
        }

        static inline SafetyHookInline shsub_436D70{};
        static void __fastcall setShaderParam(grmShaderInfo* _this, void* edx, void* a2, int index, void* in, int a5, int a6, int a7)
        {
            size_t j = 1;
            if (_this->m_parameters.wCount)
            {
                auto sv = std::string_view(_this->m_pszShaderPath);
                auto shader_name = sv.substr(sv.find_last_of('/') + 1);
                auto hash = hashStringLowercaseFromSeed(shader_name.data(), 0);
                ShaderInfoParamHashes[hash] = { _this->m_parameters.pData, _this->m_parameters.wSize };
                ShaderInfoParams[hash][index].assign((uint8_t*)in, (uint8_t*)in + a5);
            }
            return shsub_436D70.fastcall(_this, edx, a2, index, in, a5, a6, a7);
        }

        static inline SafetyHookInline shsetGlobalParam{};
        static void __cdecl setGlobalParam(int index, void* data, int a3)
        {
            GlobalParams[index].assign((uint8_t*)data, (uint8_t*)data + 16); //assuming 4x4, maybe needs fixing
            return shsetGlobalParam.ccall(index, data, a3);
        }

        static inline grmShaderInfo_Parameter* globalShaderParameters = nullptr;
        static inline uint32_t* dwGlobalShaderParameterCount = nullptr;
        static grmShaderInfo_Parameter* getGlobalShaderInfoParam(const char* name)
        {
            for (uint32_t i = 0; i < *dwGlobalShaderParameterCount; i++)
            {
                if (std::string_view(name) == std::string_view(globalShaderParameters[i].pszName))
                    return &globalShaderParameters[i];
            }
            return nullptr;
        }

        static inline int(__cdecl* getGlobalParameterIndexByName)(const char* a1) = nullptr;
        static float* getGlobalParam(const char* name)
        {
            auto i = getGlobalParameterIndexByName(name);
            if (GlobalParams.contains(i))
                return reinterpret_cast<float*>(GlobalParams[i].data());
            else
            {
                auto it = getGlobalShaderInfoParam(name);
                return it && it->pValue ? reinterpret_cast<float*>(it->pValue) : nullptr;
            }
            return nullptr;
        }

        static float* getParam(const char* shaderName, const char* paramName)
        {
            auto hash = hashStringLowercaseFromSeed(paramName, 0);

            for (auto& it : ShaderInfoParams)
            {
                if (it.first == hashStringLowercaseFromSeed(shaderName, 0))
                 {
                    auto i = getParamIndex(it.first, hash, 1);
                    if (i)
                        return reinterpret_cast<float*>(it.second[i].data());
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

export namespace CWeather
{
    enum eWeatherType : uint32_t
    {
        EXTRASUNNY,
        SUNNY,
        SUNNY_WINDY,
        CLOUDY,
        RAIN,
        DRIZZLE,
        FOGGY,
        LIGHTNING
    };

    float* Rain = nullptr;
    eWeatherType* CurrentWeather = nullptr;
    eWeatherType* NextWeather = nullptr;
    float* NextWeatherPercentage = nullptr;
}

export namespace RageDirect3DDevice9
{
    enum eTexture : uint32_t
    {
        unk,
        unk2,
        HDRTex,
    };

    IDirect3DDevice9** m_pRealDevice = nullptr;

    IDirect3DTexture9** g_TexturesBySampler = nullptr;

    IDirect3DTexture9* GetTexture(uint32_t index)
    {
        if (index < 272)
            return g_TexturesBySampler[index];
        return nullptr;
    }
}

export class CRenderPhaseDeferredLighting_LightsToScreen
{
public:
    static FusionFix::Event<>& OnBuildRenderList() {
        static FusionFix::Event<> BuildRenderListEvent;
        return BuildRenderListEvent;
    }
    static FusionFix::Event<rage::CLightSource*>& OnAfterCopyLight() {
        static FusionFix::Event<rage::CLightSource*> AfterCopyLightEvent;
        return AfterCopyLightEvent;
    }

    static inline SafetyHookInline shBuildRenderList{};
    static inline SafetyHookInline shCopyLight{};

    static void __fastcall BuildRenderList(CBaseDC* _this, void* edx)
    {
        OnBuildRenderList().executeAll();
        return shBuildRenderList.fastcall(_this, edx);
    }

    static rage::CLightSource* __fastcall CopyLight(void* _this, void* edx, void* a2)
    {
        auto ret = shCopyLight.fastcall<rage::CLightSource*>(_this, edx, a2);
        OnAfterCopyLight().executeAll(ret);
        return ret;
    }
};

export class CRenderPhaseDrawScene
{
public:
    static FusionFix::Event<>& OnBuildRenderList() {
        static FusionFix::Event<> BuildRenderListEvent;
        return BuildRenderListEvent;
    }
    //static FusionFix::Event<>& onBeforePostFX() {
    //    static FusionFix::Event<> BeforePostFX;
    //    return BeforePostFX;
    //}
    //static FusionFix::Event<>& onAfterPostFX() {
    //    static FusionFix::Event<> AfterPostFX;
    //    return AfterPostFX;
    //}

    static inline SafetyHookInline shBuildRenderList{};
    static void __fastcall BuildRenderList(CBaseDC* _this, void* edx)
    {  
        OnBuildRenderList().executeAll();
        shBuildRenderList.fastcall<void*>(_this, edx);
    }
};

export namespace CTxdStore
{
    rage::grcTexturePC* (__fastcall* getEntryByKey)(int* _this, void* edx, unsigned int a2);
    int* (__cdecl* at)(int);
}

export int32_t* _dwCurrentEpisode;
export void* (__stdcall* getNativeAddress)(uint32_t);
export HWND gWnd;
export RECT gRect;
export bool bDynamicShadowForTrees = true;
export bool bMoreShadows = false;
export bool bLoadingShown = false;
export int bMenuNeedsUpdate = 0;
export int bMenuNeedsUpdate2 = 0;
export bool bEnableSnow = false;
export bool bEnableHall = false;
export bool bFixAutoExposure = true;

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
        auto pattern = find_pattern("56 57 8B F9 8B 07 FF 50 08 25", "56 8B F1 8B 06 8B 50 08 57 FF D2 25");
        CBaseDC::AppendAddr = pattern.get_first(0);

        pattern = find_pattern("53 56 57 8B 7C 24 10 FF 74 24 14", "8B 44 24 08 56 57 8B 7C 24 0C 8B F7");
        CBaseDC::operator_newAddr = pattern.get_first(0);

        _dwCurrentEpisode = *find_pattern("83 3D ? ? ? ? ? 75 0F 6A 02", "89 35 ? ? ? ? 89 35 ? ? ? ? 6A 00 6A 01").get_first<int32_t*>(2);

        pattern = find_pattern("0A 05 ? ? ? ? 0A 05 ? ? ? ? 75 38", "0A 05 ? ? ? ? 0A 05");
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

        pattern = find_pattern("56 6A 00 FF 74 24 0C E8 ? ? ? ? 8B 35", "8B 44 24 04 56 6A 00 50 E8 ? ? ? ? 8B 35");
        rage::grmShaderInfo::getGlobalParameterIndexByName = pattern.get_first<int(__cdecl)(const char*)>(0);

        pattern = find_pattern("8B 54 24 08 85 D2 74 62", "56 8B 74 24 0C 85 F6 74 62");
        rage::grmShaderInfo::shsub_436D70 = safetyhook::create_inline(pattern.get_first(0), rage::grmShaderInfo::setShaderParam);

        pattern = find_pattern("81 C6 ? ? ? ? 42 57", "81 C6 ? ? ? ? 55 A3");
        rage::grmShaderInfo::globalShaderParameters = *pattern.get_first<rage::grmShaderInfo_Parameter*>(2);

        pattern = find_pattern("8B 15 ? ? ? ? 8D 34 52", "8B 15 ? ? ? ? 83 C2 FF 33 C0");
        rage::grmShaderInfo::dwGlobalShaderParameterCount = *pattern.get_first<uint32_t*>(2);

        pattern = find_pattern("83 3D ? ? ? ? ? 75 61 8B 44 24 04", "83 3D ? ? ? ? ? 75 5D");
        rage::grmShaderInfo::shsetGlobalParam = safetyhook::create_inline(pattern.get_first(0), rage::grmShaderInfo::setGlobalParam);

        pattern = find_pattern("55 8B EC 83 E4 F0 81 EC ? ? ? ? 53 8B D9 56 F7 83", "55 8B EC 83 E4 F0 81 EC ? ? ? ? 53 56 57 8B F9 33 DB");
        CRenderPhaseDeferredLighting_LightsToScreen::shBuildRenderList = safetyhook::create_inline(pattern.get_first(), CRenderPhaseDeferredLighting_LightsToScreen::BuildRenderList);

        pattern = find_pattern("E8 ? ? ? ? F3 0F 10 44 24 ? 51 F3 0F 11 04 24 56 E8 ? ? ? ? 83 C4 08 FF 05", "E8 ? ? ? ? D9 44 24 0C 51 D9 1C 24 56 E8 ? ? ? ? 83 C4 08");
        CRenderPhaseDeferredLighting_LightsToScreen::shCopyLight = safetyhook::create_inline(injector::GetBranchDestination(pattern.get_first()).get<void*>(), CRenderPhaseDeferredLighting_LightsToScreen::CopyLight);

        pattern = find_pattern("55 8B EC 83 E4 F0 83 EC 18 56 57 8B F9 83 BF", "55 8B EC 83 E4 F0 83 EC 24 53 56 8B D9 83 BB");
        CRenderPhaseDrawScene::shBuildRenderList = safetyhook::create_inline(pattern.get_first(0), CRenderPhaseDrawScene::BuildRenderList);

        pattern = hook::pattern("F3 0F 11 05 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? F3 0F 11 0D");
        if (pattern.empty())
            pattern = hook::pattern("F3 0F 11 05 ? ? ? ? E8 ? ? ? ? 84 C0 74 15 E8 ? ? ? ? 84 C0");
        CWeather::Rain = *pattern.get_first<float*>(4);

        pattern = find_pattern("A1 ? ? ? ? 83 C4 08 8B CF", "A1 ? ? ? ? 80 3F 04");
        CWeather::CurrentWeather = *pattern.get_first<CWeather::eWeatherType*>(1);
        
        pattern = find_pattern("A1 ? ? ? ? 89 46 4C A1", "A1 ? ? ? ? 77 05 A1 ? ? ? ? 80 3F 04");
        CWeather::NextWeather = *pattern.get_first<CWeather::eWeatherType*>(1);
        
        pattern = hook::pattern("F3 0F 10 05 ? ? ? ? 8B 44 24 0C 8B 4C 24 04");
        CWeather::NextWeatherPercentage = *pattern.get_first<float*>(4);

        pattern = find_pattern("BF ? ? ? ? F3 AB B8", "BF ? ? ? ? F3 AB B8 ? ? ? ? B9");
        RageDirect3DDevice9::g_TexturesBySampler = *pattern.get_first<IDirect3DTexture9**>(1);

        pattern = find_pattern("81 EC ? ? ? ? 8D 04 24 68 ? ? ? ? FF B4 24", "8B 44 24 04 81 EC ? ? ? ? 68");
        rage::grcTextureFactoryPC::shCreateTexture = safetyhook::create_inline(pattern.get_first(0), rage::grcTextureFactoryPC::CreateTexture);
        
        pattern = find_pattern("53 8B 5C 24 08 56 33 F6 57 39 35 ? ? ? ? 7E 25 8B 3C B5", "53 8B 5C 24 08 56 57 33 FF 39 3D", "53 8B 5C 24 08 56 33 F6 39 35 ? ? ? ? 57 7E 29 8B 3C B5 ? ? ? ? 8B 07 8B 50 14 53 8B CF FF D2 50 E8 ? ? ? ? 83 C4 08 85 C0");
        rage::grcTextureFactoryPC::shCreateRT = safetyhook::create_inline(pattern.get_first(0), rage::grcTextureFactoryPC::CreateRT);

        pattern = find_pattern("53 55 56 57 8B F9 85 FF 74 3F", "53 55 8B 6C 24 0C 56 57 EB 06 8D 9B 00 00 00 00 0F B7 51 14 33 FF 83 EA 01 78 26 8B 59 10", "85 C9 53 55 56 57 74 40 8B 6C 24 14 8D 64 24 00");
        CTxdStore::getEntryByKey = pattern.get_first<rage::grcTexturePC*(__fastcall)(int*, void*, unsigned int)>(0);

        pattern = hook::pattern("68 ? ? ? ? 68 ? ? ? ? 68 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 8B C8 E8 ? ? ? ? A3 ? ? ? ? 5E");
        CTxdStore::at = (int* (__cdecl*)(int))injector::ReadMemory<uint32_t>(pattern.get_first(1), true);

        pattern = find_pattern("A3 ? ? ? ? C7 05 ? ? ? ? ? ? ? ? E8 ? ? ? ? A1", "A3 ? ? ? ? C7 05 ? ? ? ? ? ? ? ? E8 ? ? ? ? 8B 0D");
        RageDirect3DDevice9::m_pRealDevice = *pattern.get_first<IDirect3DDevice9**>(1);
    }
} Common;