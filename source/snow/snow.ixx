module;

#include <common.hxx>
#include <d3dx9tex.h>
#include "snow.h"

export module snow;

import common;
import renderthread;
import comvars;
import d3dx9_43;
import fusiondxhook;

#define IDR_SNOWTX 200
#define IDR_BLITPS 201
#define IDR_BLITVS 202
#define IDR_SNOWPS 203
#define IDR_SNOWVS 204

class Snow
{
private:
    struct VertexFormat
    {
        float Pos[3];
        float TexCoord[2];
    };

    static inline float mCfgVolumetricIntensityMultiplier = 1.0f;

    static inline float mVolumeIntensity = 1.0f;

    static inline IDirect3DVertexBuffer9* mQuadVertexBuffer;
    static inline IDirect3DVertexDeclaration9* mQuadVertexDecl;

    static inline IDirect3DVertexShader9* mSnowVS;
    static inline IDirect3DPixelShader9* mSnowPS;
    static inline IDirect3DTexture9* mSnowTexture;

    static inline IDirect3DVertexShader9* mBlitVS;
    static inline IDirect3DPixelShader9* mBlitPS;

    static inline rage::grcRenderTargetPC* mDiffuseRT;
    static inline rage::grcRenderTargetPC* mNormalRT;
    static inline rage::grcRenderTargetPC* mSpecularAoRT;
    static inline rage::grcRenderTargetPC* mStencilRT;
    static inline rage::grcRenderTargetPC* mDepthRT;

    static inline rage::grcRenderTargetPC* mNormalRtCopy;

    static void __fastcall OnDeviceLost()
    {
        mDiffuseRT = nullptr;
        mNormalRT = nullptr;
        mSpecularAoRT = nullptr;
        mStencilRT = nullptr;
        mDepthRT = nullptr;

        if (mQuadVertexBuffer)
        {
            mQuadVertexBuffer->Release();
            mQuadVertexBuffer = nullptr;
        }

        if (mQuadVertexDecl)
        {
            mQuadVertexDecl->Release();
            mQuadVertexBuffer = nullptr;
        }

        if (mSnowTexture)
        {
            mSnowTexture->Release();
            mSnowTexture = nullptr;
        }

        if (mNormalRtCopy)
        {
            mNormalRtCopy->Destroy();
            mNormalRtCopy = nullptr;
        }
    }

    static void __fastcall OnDeviceReset()
    {
        mDiffuseRT = CViewport3DScene::GetDiffuseRT();
        mNormalRT = CViewport3DScene::GetNormalRT();
        mSpecularAoRT = CViewport3DScene::GetSpecularAoRT();
        mDepthRT = CViewport3DScene::GetDepthRT();
        mStencilRT = CViewport3DScene::GetStencilRT();

        auto width = *rage::grcDevice::ms_nActiveWidth;
        auto height = *rage::grcDevice::ms_nActiveHeight;

        rage::grcRenderTargetDesc desc{};
        desc.mMultisampleCount = 0;
        desc.field_0 = 1;
        desc.field_12 = 1;
        desc.mDepthRT = nullptr;
        desc.field_8 = 1;
        desc.field_10 = 1;
        desc.field_11 = 1;
        desc.field_24 = false;
        desc.mFormat = rage::GRCFMT_A2R10G10B10;

        mNormalRtCopy = rage::grcTextureFactory::GetInstance()->CreateRenderTarget("_DEFERRED_GBUFFER_1_COPY", 3, width, height, 32, &desc);

        HMODULE hm = NULL;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&OnDeviceReset, &hm);
        D3DXCreateTextureFromResource(rage::grcDevice::GetD3DDevice(), hm, MAKEINTRESOURCE(IDR_SNOWTX), &mSnowTexture);

        D3DVERTEXELEMENT9 vertexDeclElements[] =
        {
            {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
            D3DDECL_END()
        };

        rage::grcDevice::GetD3DDevice()->CreateVertexDeclaration(vertexDeclElements, &mQuadVertexDecl);

        rage::grcDevice::GetD3DDevice()->CreateVertexBuffer(6 * sizeof(VertexFormat), 0, 0, D3DPOOL_DEFAULT, &mQuadVertexBuffer, NULL);

        VertexFormat* vertexData;
        mQuadVertexBuffer->Lock(0, 0, (void**)&vertexData, 0);

        vertexData[0] = { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f };
        vertexData[1] = { -1.0f, 1.0f, 0.0f, 0.0f, 0.0f };
        vertexData[2] = { 1.0f, -1.0f, 0.0f, 1.0f, 1.0f };
        vertexData[3] = { -1.0f, 1.0f, 0.0f, 0.0f, 0.0f };
        vertexData[4] = { 1.0f, 1.0f, 0.0f, 1.0f, 0.0f };
        vertexData[5] = { 1.0f, -1.0f, 0.0f, 1.0f, 1.0f };

        mQuadVertexBuffer->Unlock();
    }

    static void Init()
    {
        static bool initialized = false;
        if (initialized)
            return;

        auto onLostCB = rage::grcDevice::Functor0(NULL, OnDeviceLost, NULL, 0);
        auto onResetCB = rage::grcDevice::Functor0(NULL, OnDeviceReset, NULL, 0);
        rage::grcDevice::RegisterDeviceCallbacks(onLostCB, onResetCB);

        HMODULE hm = NULL;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&Init, &hm);

        auto resource = FindResource(hm, MAKEINTRESOURCE(IDR_BLITPS), RT_RCDATA);
        auto blitPS = (DWORD*)LoadResource(hm, resource);
        rage::grcDevice::GetD3DDevice()->CreatePixelShader((DWORD*)blitPS, &mBlitPS);
        FreeResource(blitPS);
        
        resource = FindResource(hm, MAKEINTRESOURCE(IDR_BLITVS), RT_RCDATA);
        auto blitVS = (DWORD*)LoadResource(hm, resource);
        rage::grcDevice::GetD3DDevice()->CreateVertexShader((DWORD*)blitVS, &mBlitVS);
        FreeResource(blitVS);

        resource = FindResource(hm, MAKEINTRESOURCE(IDR_SNOWPS), RT_RCDATA);
        auto snowPS = (DWORD*)LoadResource(hm, resource);
        rage::grcDevice::GetD3DDevice()->CreatePixelShader((DWORD*)snowPS, &mSnowPS);
        FreeResource(snowPS);

        resource = FindResource(hm, MAKEINTRESOURCE(IDR_SNOWVS), RT_RCDATA);
        auto snowVS = (DWORD*)LoadResource(hm, resource);
        rage::grcDevice::GetD3DDevice()->CreateVertexShader((DWORD*)snowVS, &mSnowVS);
        FreeResource(snowVS);

        OnDeviceReset();

        initialized = true;
    }

    static void OnBeforeLighting()
    {
        if (!bEnableSnow)
            return;

        IDirect3DDevice9* device = rage::grcDevice::GetD3DDevice();

        mVolumeIntensity = 1.0f;

        //static CWeather::eWeatherType currWeather;
        //
        //CWeather::eWeatherType prevWeather = currWeather;
        //currWeather = CWeather::GetCurrentWeather();
        //CWeather::eWeatherType nextWeather = CWeather::GetNextWeather();

        DWORD prevMinFilters[5];
        DWORD prevMagFilters[5];
        for (uint32_t i = 0; i < 5; i++)
        {
            device->GetSamplerState(i, D3DSAMP_MINFILTER, &prevMinFilters[i]);
            device->GetSamplerState(i, D3DSAMP_MAGFILTER, &prevMagFilters[i]);

            device->SetSamplerState(i, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            device->SetSamplerState(i, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        }

        rage::grcViewport* currGrcViewport = rage::GetCurrentViewport();

        rage::Vector4 pixelOffset;
        pixelOffset.x = (-1.0f / mNormalRT->mWidth);
        pixelOffset.y = (1.0f / mNormalRT->mHeight);
        device->SetVertexShaderConstantF(5, &pixelOffset.x, 1);

        //copy normal gbuffer
        {
            rage::grcTextureFactoryPC::GetInstance()->LockRenderTarget(0, mNormalRtCopy, nullptr);
            device->SetVertexShader(mBlitVS);
            device->SetPixelShader(mBlitPS);

            device->SetVertexDeclaration(mQuadVertexDecl);
            device->SetStreamSource(0, mQuadVertexBuffer, 0, sizeof(VertexFormat));

            device->SetTexture(0, mNormalRT->mD3DTexture);

            device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

            rage::grcDevice::grcResolveFlags resolveFlags{};
            rage::grcTextureFactoryPC::GetInstance()->UnlockRenderTarget(0, &resolveFlags);
        }

        //render snow
        rage::grcTextureFactoryPC::GetInstance()->LockRenderTarget(0, mDiffuseRT, nullptr);
        rage::grcTextureFactoryPC::GetInstance()->LockRenderTarget(1, mNormalRT, nullptr);
        rage::grcTextureFactoryPC::GetInstance()->LockRenderTarget(2, mSpecularAoRT, nullptr);

        device->SetVertexShader(mSnowVS);
        device->SetPixelShader(mSnowPS);

        device->SetVertexDeclaration(mQuadVertexDecl);
        device->SetStreamSource(0, mQuadVertexBuffer, 0, sizeof(VertexFormat));

        rage::Vector4 projParams;
        float a = currGrcViewport->mFarClip / (currGrcViewport->mFarClip - currGrcViewport->mNearClip);
        projParams.x = currGrcViewport->field_2CC;
        projParams.y = currGrcViewport->field_2C8;
        projParams.z = 1.0f / (0.0f - (a * currGrcViewport->mNearClip));
        projParams.w = projParams.z * a;

        device->SetVertexShaderConstantF(0, &projParams.x, 1);
        device->SetVertexShaderConstantF(1, &currGrcViewport->mViewInverseMatrix[0][0], 4);

        rage::Vector4 threshold;
        //if (!HasSnow(currWeather) && HasSnow(nextWeather))
        //    threshold.y = CWeather::GetNextWeatherPercentage();
        //else if (HasSnow(currWeather) && !HasSnow(nextWeather))
        //    threshold.y = 0.9999f - CWeather::GetNextWeatherPercentage();
        //else if (!HasSnow(currWeather) && !HasSnow(nextWeather))
        //    threshold.y = 0.0f;
        //else
            threshold.y = 0.9999f;

        mVolumeIntensity = threshold.y * 4.0f * mCfgVolumetricIntensityMultiplier;

        threshold.y = pow(threshold.y, 0.20f);
        threshold.x = max(0.9999f, (threshold.y / (threshold.y + 0.15f)) * 1.15f);

        device->SetPixelShaderConstantF(0, &threshold.x, 1);
        device->SetPixelShaderConstantF(1, &projParams.x, 1);
        device->SetPixelShaderConstantF(2, &currGrcViewport->mViewInverseMatrix[0][0], 4);

        device->SetTexture(0, mNormalRtCopy->mD3DTexture);
        device->SetTexture(1, mSpecularAoRT->mD3DTexture);
        device->SetTexture(2, mStencilRT->mD3DTexture);
        device->SetTexture(3, mDepthRT->mD3DTexture);
        device->SetTexture(4, mSnowTexture);

        device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);

        rage::grcDevice::grcResolveFlags resolveFlags{};
        rage::grcTextureFactoryPC::GetInstance()->UnlockRenderTarget(0, &resolveFlags);
        rage::grcTextureFactoryPC::GetInstance()->UnlockRenderTarget(1, &resolveFlags);
        rage::grcTextureFactoryPC::GetInstance()->UnlockRenderTarget(2, &resolveFlags);

        for (uint32_t i = 0; i < 5; i++)
        {
            device->SetSamplerState(i, D3DSAMP_MINFILTER, prevMinFilters[i]);
            device->SetSamplerState(i, D3DSAMP_MAGFILTER, prevMagFilters[i]);
        }
    }

    static void OnBuildRenderList()
    {
        Init();

        auto mBeforeLightingCB = new T_CB_Generic_NoArgs(OnBeforeLighting);

        if (mBeforeLightingCB)
            mBeforeLightingCB->Append();
    }

    static void OnAfterCopyLight(rage::CLightSource* light)
    {
        //CLightSource doesnt have a member to control the volume intensity so
        //i abuse type casting to use mTxdId for it as im p sure its only used for headlights anyway

        if (bEnableSnow)
        {
            if (light->mFlags & 8 /*volumetric*/)
            {
                *(float*)&light->mTxdId = 1.0f;
            }
            else if (light->mType == rage::LT_SPOT && !(light->mFlags & 0x300)/*vehicles and traffic lights*/)
            {
                if (mVolumeIntensity > 0.0f && light->mRadius < 50.0f)
                {
                    light->mVolumeSize = 1.0f;
                    light->mVolumeScale = 0.5f;
                    //light->field_64 = -1; //not really sure what this is but setting it to -1 makes the light not cast shadows
                    light->mFlags |= 8;
                    *(float*)&light->mTxdId = mVolumeIntensity;
                    //light->mPosition.z += 0.1f;
                }
            }
        }
    }

    class CRenderPhaseDeferredLighting_LightsToScreen
    {
    public:
        static inline SafetyHookInline shBuildRenderList{};
        static inline SafetyHookInline shCopyLight{};

        static void* __fastcall BuildRenderList(CBaseDC* _this, void* edx)
        {
            OnBuildRenderList();
            return shBuildRenderList.fastcall<void*>(_this, edx);
        }

        static rage::CLightSource* __fastcall CopyLight(void* _this, void* edx, void* a2)
        {
            auto ret = shCopyLight.fastcall<rage::CLightSource*>(_this, edx, a2);
            OnAfterCopyLight(ret);
            return ret;
        }
    };

    static inline uint8_t* byte_1723BB0 = nullptr;
    static inline float* CWeatherRain = nullptr;
    static inline SafetyHookInline shPostFXWrap{};
    static int __cdecl PostFXWrap(char edx)
    {
        auto ret = shPostFXWrap.ccall<int>(edx);

        {
            static CSnow::RwMatrix GviewMatrix;
            static CSnow::RwMatrix camMatrix;

            static float ts = 0.0f;
            ts = *CTimer::fTimeStep / 50.0f * 1000.0f;
            if (CTimer::m_UserPause && CTimer::m_CodePause)
            {
                if (*CTimer::m_UserPause || *CTimer::m_CodePause)
                {
                    ts = 0.0f;
                }
            }
            
            CSnow::targetSnow = *CWeatherRain;

            auto pViewport = rage::GetCurrentViewport();

            camMatrix.right = { pViewport->mCameraMatrix[0][0], pViewport->mCameraMatrix[0][1], pViewport->mCameraMatrix[0][2] };
            camMatrix.up = { pViewport->mCameraMatrix[1][0], pViewport->mCameraMatrix[1][1], pViewport->mCameraMatrix[1][2] };
            camMatrix.at = { pViewport->mCameraMatrix[2][0], pViewport->mCameraMatrix[2][1], pViewport->mCameraMatrix[2][2] };
            camMatrix.pos = { pViewport->mCameraMatrix[3][0], pViewport->mCameraMatrix[3][1], pViewport->mCameraMatrix[3][2] };

            GviewMatrix.right = { pViewport->mWorldViewProjMatrix[0][0], pViewport->mWorldViewProjMatrix[0][1], pViewport->mWorldViewProjMatrix[0][2] };
            GviewMatrix.at = { pViewport->mWorldViewProjMatrix[1][0], pViewport->mWorldViewProjMatrix[1][1], pViewport->mWorldViewProjMatrix[1][2] };
            GviewMatrix.up = { pViewport->mWorldViewProjMatrix[2][0], pViewport->mWorldViewProjMatrix[2][1], pViewport->mWorldViewProjMatrix[2][2] };
            GviewMatrix.pos = { pViewport->mWorldViewProjMatrix[3][0], pViewport->mWorldViewProjMatrix[3][1], pViewport->mWorldViewProjMatrix[3][2] };

            CSnow::AddSnow(rage::grcDevice::GetD3DDevice(), *rage::grcDevice::ms_nActiveWidth, *rage::grcDevice::ms_nActiveHeight, &camMatrix, &GviewMatrix, &ts, byte_1723BB0[12]);
        }

        return ret;
    }

public:
    Snow()
    {
        FusionFix::onInitEvent() += []()
        {
            if (GetD3DX9_43DLL())
            {

            }
        };
    }
} Snow;