module;

#include <common.hxx>
#include <d3dx9tex.h>
#include "snow.h"
#include <chrono>
#include <ctime>

export module snow;

import common;
import comvars;
import natives;
import d3dx9_43;
import fusiondxhook;

#define IDR_SNOWTX 200
#define IDR_BLITPS 201
#define IDR_BLITVS 202
#define IDR_SNOWPS 203
#define IDR_SNOWVS 204

rage::grcTexturePC* vehicle_generic_glasswindows2;
rage::grcTexturePC* vehicle_generic_tyrewallblack;
rage::grcTexturePC* vehicle_generic_glassdirt;
rage::grcTexturePC* vehicle_genericmud_car;
rage::grcTexturePC* vehicle_genericmud_truck;

rage::grcTexturePC* vehicle_generic_glasswindows2_snow;
rage::grcTexturePC* vehicle_generic_tyrewallblack_snow;
rage::grcTexturePC* vehicle_generic_glassdirt_snow;
rage::grcTexturePC* vehicle_genericmud_car_snow;
rage::grcTexturePC* vehicle_genericmud_truck_snow;

static SafetyHookInline shsub_41B920{};
rage::grcTexturePC* __fastcall sub_41B920(rage::grcTextureReference* tex, void* edx)
{
    if (bEnableSnow && *CWeather::CurrentWeather != CWeather::LIGHTNING)
    {
        if (!vehicle_generic_glasswindows2)
        {
            if (std::string_view(tex->m_pszName) == "vehicle_generic_glasswindows2")
                vehicle_generic_glasswindows2 = tex->m_pTexture;
        }
        else if (vehicle_generic_glasswindows2_snow && tex->m_pTexture == vehicle_generic_glasswindows2)
            return vehicle_generic_glasswindows2_snow;

        if (!vehicle_generic_tyrewallblack)
        {
            if (std::string_view(tex->m_pszName).contains("vehicle_generic_tyrewallblack"))
                vehicle_generic_tyrewallblack = tex->m_pTexture;
        }
        else if (vehicle_generic_tyrewallblack_snow && tex->m_pTexture == vehicle_generic_tyrewallblack)
            return vehicle_generic_tyrewallblack_snow;

        if (!vehicle_generic_glassdirt)
        {
            if (std::string_view(tex->m_pszName) == "vehicle_generic_glassdirt")
                vehicle_generic_glassdirt = tex->m_pTexture;
        }
        else if (vehicle_generic_glassdirt_snow && tex->m_pTexture == vehicle_generic_glassdirt)
            return vehicle_generic_glassdirt_snow;

        if (!vehicle_genericmud_car)
        {
            if (std::string_view(tex->m_pszName) == "vehicle_genericmud_car")
                vehicle_genericmud_car = tex->m_pTexture;
        }
        else if (vehicle_genericmud_car_snow && tex->m_pTexture == vehicle_genericmud_car)
            return vehicle_genericmud_car_snow;

        if (!vehicle_genericmud_truck)
        {
            if (std::string_view(tex->m_pszName) == "vehicle_genericmud_truck")
                vehicle_genericmud_truck = tex->m_pTexture;
        }
        else if (vehicle_genericmud_truck_snow && tex->m_pTexture == vehicle_genericmud_truck)
            return vehicle_genericmud_truck_snow;
    }

    return shsub_41B920.fastcall<rage::grcTexturePC*>(tex, edx);
}

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

        auto HasSnow = [](CWeather::eWeatherType type) -> bool
        {
            if (type == CWeather::LIGHTNING)
                return false;
            return true;
        };

        IDirect3DDevice9* device = rage::grcDevice::GetD3DDevice();

        mVolumeIntensity = 1.0f;

        static CWeather::eWeatherType currWeather;

        auto prevWeather = currWeather;
        currWeather = *CWeather::CurrentWeather;
        auto nextWeather = *CWeather::NextWeather;

        if (HasSnow(prevWeather) || HasSnow(currWeather) || HasSnow(nextWeather))
        {
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
            if (!HasSnow(currWeather) && HasSnow(nextWeather))
                threshold.y = *CWeather::NextWeatherPercentage;
            else if (HasSnow(currWeather) && !HasSnow(nextWeather))
                threshold.y = 0.9999f - *CWeather::NextWeatherPercentage;
            else if (!HasSnow(currWeather) && !HasSnow(nextWeather))
                threshold.y = 0.0f;
            else
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
    }

    static inline uint8_t* byte_1723BB0 = nullptr;
    static void LCSSnow()
    {
        if (bEnableSnow)
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

            CSnow::targetSnow = *CWeather::Rain;

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
    }

    struct gtaRainEmitter
    {
        float posX;
        float posY;
        float posZ;
        int unk1;
        float posRangeX;
        float posRangeY;
        float posRangeZ;
        int unk2;
        float LifeRangeOffsetX;
        float LifeRangeOffsetY;
        int unk3;
        int unk4;
        float velX;
        float velY;
        float velZ;
        int unk5;
        float velRangeX;
        float velRangeY;
        float velRangeZ;
        int posRelative;
        int AddCameraOffset;
        float collisionVelocityModifierX;
        float collisionVelocityModifierY;
        float collisionVelocityModifierZ;
        int unk6;
        int unk7;
        int unk8;
        int unk9;
        float collisionLifeModifierX;
        float collisionLifeModifierY;
        int unk10;
        int unk11;
        float collisionPositionModifierX;
        float collisionPositionModifierY;
        float collisionPositionModifierZ;
        int unk12;
        float gustSpacingX;
        float gustSpacingY;
        int unk13[6];
        float disperseRange;
        float disperseAmt;
        float inertia;
        float probablityPhase2;
    };

    struct gtaRainRender
    {
        float motionBlurAmt;
        float radius;
        float radiusVariance;
        float radius2;
        float radius2Variance;
        float lifeFadeX;
        float lifeFadeY;
        int useDirectional;
        int unk1;
        int unk2;
        int unk3;
        float tintColorX;
        float tintColorY;
        float tintColorZ;
        float tintColorW;
        float tintColorPhase2X;
        float tintColorPhase2Y;
        float tintColorPhase2Z;
        float tintColorPhase2W;
        float AnimStart;
        float AnimEnd;
        float NumFrames;
    };

    static inline gtaRainRender* pRainRender = nullptr;
    static inline gtaRainEmitter* pRainEmitter = nullptr;
    static inline gtaRainRender RainRenderCopy;
    static inline gtaRainEmitter RainEmitterCopy;

    static void SetRainRenderParams()
    {
        if (bEnableSnow)
        {
            RainRenderCopy = *pRainRender;
            RainEmitterCopy = *pRainEmitter;

            pRainRender->motionBlurAmt = 0.01f;
            pRainRender->radius = 0.042f;
            pRainRender->radius2 = 0.0f;
            pRainRender->tintColorX = 30.0f;
            pRainRender->tintColorY = 30.0f;
            pRainRender->tintColorZ = 30.0f;
            pRainRender->tintColorW = 0.15f;
            pRainRender->tintColorPhase2X = 1.0f;
            pRainRender->tintColorPhase2Y = 1.0f;
            pRainRender->tintColorPhase2Z = 1.0f;
            pRainRender->tintColorPhase2W = 1.0f;

            pRainEmitter->velX = -10.0f;
            pRainEmitter->velY = -10.0f;
            pRainEmitter->velZ = -5.0f;
            pRainEmitter->velRangeX = 4.0f;
            pRainEmitter->velRangeY = 4.0f;
            pRainEmitter->velRangeZ = -2.2f;
            pRainEmitter->probablityPhase2 = 0.1f;
        }
        else
        {
             *pRainRender = RainRenderCopy;
             *pRainEmitter = RainEmitterCopy;
        }
    }

public:
    static void ToggleSnow(bool bValue)
    {
        bEnableSnow = bValue;
        CTimeCycle::Initialise();
        SetRainRenderParams();
    }

    Snow()
    {
        FusionFix::onInitEventAsync() += []()
        {
            if (GetD3DX9_43DLL())
            {
                auto now = std::chrono::system_clock::now();
                auto now_c = std::chrono::system_clock::to_time_t(now);
                auto date = std::localtime(&now_c);

                if ((date->tm_mon == 0 && date->tm_mday <= 2) || (date->tm_mon == 11 && date->tm_mday >= 29))
                {
                    FusionFix::onGameProcessEvent() += []()
                    {
                        auto now = std::chrono::system_clock::now();
                        auto now_c = std::chrono::system_clock::to_time_t(now);
                        auto date = std::localtime(&now_c);

                        if ((date->tm_mon == 0 && date->tm_mday <= 2) || (date->tm_mon == 11 && date->tm_mday >= 30))
                        {
                            static bool bOnce = false;
                            if (!bOnce)
                            {
                                bOnce = true;
                                ToggleSnow(true);
                            }
                        }
                        else if (bEnableSnow)
                        {
                            static bool bOnce = false;
                            if (!bOnce)
                            {
                                bOnce = true;
                                ToggleSnow(false);
                            }
                        }
                    };
                }

                #ifdef _DEBUG
                FusionFix::onGameProcessEvent() += []()
                {
                    static auto oldState = GetAsyncKeyState(VK_F3);
                    auto curState = GetAsyncKeyState(VK_F3);
                    if ((oldState & 0x8000) == 0 && (curState & 0x8000))
                    {
                        ToggleSnow(!bEnableSnow);
                    }
                    oldState = curState;
                };
                #endif

                CRenderPhaseDeferredLighting_LightsToScreen::OnBuildRenderList() += []()
                {
                    Init();
                    auto mBeforeLightingCB = new T_CB_Generic_NoArgs(OnBeforeLighting);
                    if (mBeforeLightingCB)
                        mBeforeLightingCB->Append();
                };

                struct hash_tuple
                {
                    size_t operator()(const std::tuple<int, int, int>& t) const
                    {
                        return get<0>(t) ^ get<1>(t) ^ get<2>(t);
                    }
                };

                static std::unordered_map<std::tuple<int, int, int>, float, hash_tuple> LightVolumeIntensity;
                CRenderPhaseDeferredLighting_LightsToScreen::OnAfterCopyLight() += [](rage::CLightSource* light)
                {
                    if (bEnableSnow)
                    {
                        if (light->mFlags & 8 /*volumetric*/)
                        {
                            LightVolumeIntensity[std::make_tuple(static_cast<int>(light->mPosition.x), static_cast<int>(light->mPosition.y), static_cast<int>(light->mPosition.z))] = 1.0f;
                        }
                        else if (light->mType == rage::LT_SPOT && !(light->mFlags & 0x300)/*vehicles and traffic lights*/)
                        {
                            if (mVolumeIntensity > 0.0f && light->mRadius < 50.0f)
                            {
                                light->mVolumeSize = 1.0f;
                                light->mVolumeScale = 0.5f;
                                light->mFlags |= 8;
                                LightVolumeIntensity[std::make_tuple(static_cast<int>(light->mPosition.x), static_cast<int>(light->mPosition.y), static_cast<int>(light->mPosition.z))] = mVolumeIntensity;
                            }
                        }
                    }
                };

                auto pattern = hook::pattern("C7 84 24 ? ? ? ? ? ? ? ? FF 74 06 44");
                if (!pattern.empty())
                {
                    struct LightsHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            if (bEnableSnow)
                            {
                                auto light = (rage::CLightSource*)(regs.esi + regs.eax);
                                *(float*)(regs.esp + 0xEC) = LightVolumeIntensity[std::make_tuple(static_cast<int>(light->mPosition.x), static_cast<int>(light->mPosition.y), static_cast<int>(light->mPosition.z))];
                            }
                            else
                                *(float*)(regs.esp + 0xEC) = 1.0f;
                        }
                    }; injector::MakeInline<LightsHook>(pattern.get_first(0), pattern.get_first(11));
                }
                else
                {
                    pattern = hook::pattern("F3 0F 11 44 24 ? 8B 54 06 44");
                    struct LightsHook
                    {
                        void operator()(injector::reg_pack& regs)
                        {
                            if (bEnableSnow)
                            {
                                auto light = (rage::CLightSource*)(regs.esi + regs.eax * 1);
                                *(float*)(regs.esp + 0xEC) = LightVolumeIntensity[std::make_tuple(static_cast<int>(light->mPosition.x), static_cast<int>(light->mPosition.y), static_cast<int>(light->mPosition.z))];
                            }
                            else
                                *(float*)(regs.esp + 0x68) = 1.0f;
                        }
                    }; injector::MakeInline<LightsHook>(pattern.get_first(0), pattern.get_first(6));
                }

                pattern = find_pattern("B9 ? ? ? ? E8 ? ? ? ? 8B 4D 0C 51 F3 0F 10 41 ? 8D 41 20", "B9 ? ? ? ? D9 44 24 1C D9 5C 24 04");
                byte_1723BB0 = *pattern.get_first<uint8_t*>(1);

                pattern = find_pattern("89 41 28 F3 0F 10 0D", "50 24 8B 0D ? ? ? ? 89 48 28 F3 0F 10 0D");
                pRainRender = *pattern.get_first<gtaRainRender*>(15);

                pattern = find_pattern("88 41 61 F3 0F 10 0D", "48 60 8A 15 ? ? ? ? 88 50 61");
                pRainEmitter = *pattern.get_first<gtaRainEmitter*>(15);

                NativeOverride::RegisterPhoneCheat("7665550100", []
                {
                    ToggleSnow(!bEnableSnow);
                });

                NativeOverride::RegisterPhoneCheat("2665550100", []
                {
                    bEnableHall = !bEnableHall;
                    ToggleSnow(false);
                });

                // Snow on vehicles: load textures
                pattern = find_pattern("E8 ? ? ? ? 8B 0D ? ? ? ? 83 C4 18 8B 01 6A 00", "E8 ? ? ? ? 83 C4 18 8B 0D");
                static auto FXRain__CTxdStore__setCurrent = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& ctx)
                {
                    vehicle_generic_glasswindows2_snow = CTxdStore::getEntryByKey(CTxdStore::at(ctx.esi), 0, hashStringLowercaseFromSeed("vehicle_generic_glasswindows2_snow", 0));
                    vehicle_generic_tyrewallblack_snow = CTxdStore::getEntryByKey(CTxdStore::at(ctx.esi), 0, hashStringLowercaseFromSeed("vehicle_generic_tyrewallblack_snow", 0));
                    vehicle_generic_glassdirt_snow = CTxdStore::getEntryByKey(CTxdStore::at(ctx.esi), 0, hashStringLowercaseFromSeed("vehicle_generic_glassdirt_snow", 0));
                    vehicle_genericmud_car_snow = CTxdStore::getEntryByKey(CTxdStore::at(ctx.esi), 0, hashStringLowercaseFromSeed("vehicle_genericmud_car_snow", 0));
                    vehicle_genericmud_truck_snow = CTxdStore::getEntryByKey(CTxdStore::at(ctx.esi), 0, hashStringLowercaseFromSeed("vehicle_genericmud_truck_snow", 0));
                });

                // Snow on vehicles: replace textures
                pattern = find_pattern("8B 49 18 85 C9 74 05 8B 01 FF 60 3C", "83 79 18 00 74 0A 8B 49 18 8B 01 8B 50 38", "55 8B EC 83 E4 F0 8B 01 8B 50 38 83 EC 10 FF D2 85 C0 74 0F 8B 10 8B 52 34");
                shsub_41B920 = safetyhook::create_inline(pattern.get_first(), sub_41B920);
                
                // LCS Snow test
                //CRenderPhaseDrawScene::onBeforePostFX() += []()
                //{
                //    auto cb = new T_CB_Generic_NoArgs(LCSSnow);
                //    if (cb)
                //        cb->Append();
                //};
                //
                //FusionFix::onBeforeReset() += []()
                //{
                //    CSnow::Reset();
                //};
            }
        };
    }
} Snow;

extern "C"
{
    bool __declspec(dllexport) IsSnowEnabled()
    {
        return bEnableSnow;
    }

    bool __declspec(dllexport) IsWeatherSnow()
    {
        return CWeather::CurrentWeather && *CWeather::CurrentWeather != CWeather::LIGHTNING;
    }

    void __declspec(dllexport) ToggleSnow()
    {
        Snow::ToggleSnow(!bEnableSnow);
    }
}