module;

#include <common.hxx>
#include <d3dx9tex.h>

export module seasonal.snow;

import comvars;
import natives;
import timecycext;

#define IDR_SNOWTX 200
#define IDR_BLITPS 201
#define IDR_BLITVS 202
#define IDR_SNOWPS 203
#define IDR_SNOWVS 204

export class Snow : public ISeasonal
{
public:
    static auto& Instance() { static Snow s; return static_cast<ISeasonal&>(s); }

    auto Init() -> void override
    {
        CRenderPhaseDeferredLighting_LightsToScreen::OnBuildRenderList() += OnBuildRenderList;

        // Disables rain-related audio (e.g. Surface impacts)
        auto pattern = hook::pattern("F3 0F 10 05 ? ? ? ? F3 0F 11 04 24 E8 ? ? ? ? 51");
        if (!pattern.empty())
        {
            static auto SetRainAudioHook = safetyhook::create_mid(pattern.get_first(8), [](SafetyHookContext& regs)
            {
                if (snowEnabled)
                {
                    regs.xmm0.f32[0] = 0.0f;
                }
            });
        }
        else
        {
            pattern = hook::pattern("D9 1C 24 E8 ? ? ? ? 51 D9 1C 24 E8 ? ? ? ? D8 44 24 08 83 C4 04 5E 59 C3 CC");
            static auto SetRainAudioHook = safetyhook::create_mid(pattern.get_first(3), [](SafetyHookContext& regs)
            {
                if (snowEnabled)
                {
                    *(float*)regs.esp = 0.0f;
                }
            });
        }

        // Disables wetness-related effects (e.g. Reflections, footstep/vehicle interaction sounds, particles)
        pattern = hook::pattern("C7 05 ? ? ? ? 00 00 80 3F F3 0F 10 25 ? ? ? ? F3 0F 10 04");
        if (!pattern.empty())
        {
            pWetness = *pattern.get_first<float*>(2);
            static auto SetRainWetnessHook = safetyhook::create_mid(pattern.get_first(10), [](SafetyHookContext& regs)
            {
                if (snowEnabled)
                {
                    *pWetness = 0.0f;
                }
            });
        }
        else
        {
            pattern = hook::pattern("F3 0F 11 05 ? ? ? ? EB 67");
            pWetness = *pattern.get_first<float*>(4);
            injector::MakeNOP(pattern.get_first(0), 8, true);
            static auto SetRainWetnessHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
            {
                *pWetness = 1.0f;

                if (snowEnabled)
                {
                    *pWetness = 0.0f;
                }
            });
        }

        // Snow on vehicles: load textures
        pattern = find_pattern("E8 ? ? ? ? 8B 0D ? ? ? ? 83 C4 18 8B 01 6A 00", "E8 ? ? ? ? 83 C4 18 8B 0D");
        static auto FXRain__CTxdStore__setCurrent = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
        {
            vehicle_generic_glasswindows2_snow = CTxdStore::getEntryByKey(CTxdStore::at(regs.esi), 0, hashStringLowercaseFromSeed("vehicle_generic_glasswindows2_snow", 0));
            vehicle_generic_tyrewallblack_snow = CTxdStore::getEntryByKey(CTxdStore::at(regs.esi), 0, hashStringLowercaseFromSeed("vehicle_generic_tyrewallblack_snow", 0));
            vehicle_generic_glassdirt_snow = CTxdStore::getEntryByKey(CTxdStore::at(regs.esi), 0, hashStringLowercaseFromSeed("vehicle_generic_glassdirt_snow", 0));
            vehicle_genericmud_car_snow = CTxdStore::getEntryByKey(CTxdStore::at(regs.esi), 0, hashStringLowercaseFromSeed("vehicle_genericmud_car_snow", 0));
            vehicle_genericmud_truck_snow = CTxdStore::getEntryByKey(CTxdStore::at(regs.esi), 0, hashStringLowercaseFromSeed("vehicle_genericmud_truck_snow", 0));
        });

        // Snow on vehicles: replace textures
        pattern = find_pattern("8B 49 18 85 C9 74 05 8B 01 FF 60 3C", "83 79 18 00 74 0A 8B 49 18 8B 01 8B 50 38", "55 8B EC 83 E4 F0 8B 01 8B 50 38 83 EC 10 FF D2 85 C0 74 0F 8B 10 8B 52 34");
        shsub_41B920 = safetyhook::create_inline(pattern.get_first(), sub_41B920);

        // Particles: can't change color, disabling
        pattern = find_pattern("83 EC ? FF 35 ? ? ? ? FF 15", "83 EC ? E8 ? ? ? ? 84 C0 0F 85 ? ? ? ? E8");
        shsub_A9F2D0 = safetyhook::create_inline(pattern.get_first(), sub_A9F2D0);

        pattern = find_pattern("81 EC ? ? ? ? A1 ? ? ? ? 33 C4 89 84 24 ? ? ? ? 53 55 56 57 8B BC 24 ? ? ? ? 8D B4 24", "81 EC ? ? ? ? A1 ? ? ? ? 33 C4 89 84 24 ? ? ? ? 55 56 8B B4 24 ? ? ? ? 8D 94 24 ? ? ? ? 57");
        static auto readMaterialsDatHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
        {
            nCurMaterialIndex = 0;
            pMaterials.clear();
        });

        pattern = find_pattern("81 EC ? ? ? ? A1 ? ? ? ? 33 C4 89 84 24 ? ? ? ? 56 8B F1 8D 4C 24", "81 EC ? ? ? ? A1 ? ? ? ? 33 C4 89 84 24 ? ? ? ? 56 F3 0F 10 84 24");
        shparse = safetyhook::create_inline(pattern.get_first(), parse);

        pattern = find_pattern("34 05 ? ? ? ? 56 8B C8 E8 ? ? ? ? 6A", "81 C1 ? ? ? ? E8 ? ? ? ? 6A ? 55");
        pShockingEvents = *pattern.get_first<void**>(2);

        pattern = find_pattern("89 41 28 F3 0F 10 0D", "50 24 8B 0D ? ? ? ? 89 48 28 F3 0F 10 0D");
        pRainRender = *pattern.get_first<gtaRainRender*>(15);
        pStormRender = reinterpret_cast<gtaRainRender*>((uintptr_t)pRainRender + 0x60);

        pattern = find_pattern("88 41 61 F3 0F 10 0D", "48 60 8A 15 ? ? ? ? 88 50 61");
        pRainEmitter = *pattern.get_first<gtaRainEmitter*>(15);
        pStormEmitter = reinterpret_cast<gtaRainEmitter*>((uintptr_t)pRainEmitter + 0xD0);

        pattern = find_pattern("F3 0F 11 05 ? ? ? ? 66 0F 6E 05 ? ? ? ? 0F 5B C0 F3 0F 59 05 ? ? ? ? F3 0F 59 05 ? ? ? ? F3 0F 58 05 ? ? ? ? F3 0F 11 04 24", "F3 0F 11 05 ? ? ? ? F3 0F 2A 05 ? ? ? ? F3 0F 59 05 ? ? ? ? F3 0F 58 05 ? ? ? ? D3 E6");
        if (!pattern.empty())
        {
            dwViewDistance = *pattern.get_first<float*>(4);
        }
    }

    auto Enable() -> void override
    {
        snowEnabled = true;

        unsub_OnAfterCopyLight = CRenderPhaseDeferredLighting_LightsToScreen::OnAfterCopyLight() += OnAfterCopyLight;
        unsub_onCTimeCycleExtInit = CTimeCycleExt::onCTimeCycleExtInit() += onCTimeCycleExtInit;

        CTimeCycle::Initialise();
        applySnowRenderParams();
        applyMaterialsDatParams();
        applyShockingEventsParams();
    }

    auto Disable() -> void override
    {
        snowEnabled = false;

        unsub_OnAfterCopyLight();
        unsub_onCTimeCycleExtInit();

        CTimeCycle::Initialise();
        restoreSnowRenderParams();
        restoreMaterialsDatParams();
        restoreShockingEventsParams();
    }

private:
    static inline auto snowEnabled = false;

    static inline std::function<void()> unsub_OnAfterCopyLight;
    static inline std::function<void()> unsub_onCTimeCycleExtInit;

    struct VertexFormat
    {
        float Pos[3];
        float TexCoord[2];
    };

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

    static inline float* pWetness = nullptr;

    static inline float* dwViewDistance = nullptr;

    static auto __fastcall OnDeviceLost() -> void
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
            mQuadVertexDecl = nullptr;
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

    static auto __fastcall OnDeviceReset() -> void
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

        auto* device = rage::grcDevice::GetD3DDevice();
        if (!device)
            return;

        // Load snow texture
        {
            HMODULE hm = NULL;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&OnDeviceReset, &hm);
            if (FAILED(D3DXCreateTextureFromResource(device, hm, MAKEINTRESOURCE(IDR_SNOWTX), &mSnowTexture)))
                mSnowTexture = nullptr;
        }

        // Create vertex declaration
        if (!mQuadVertexDecl)
        {
            D3DVERTEXELEMENT9 vertexDeclElements[] =
            {
                {0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
                {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
                D3DDECL_END()
            };

            if (FAILED(device->CreateVertexDeclaration(vertexDeclElements, &mQuadVertexDecl)))
            {
                mQuadVertexDecl = nullptr;
                return;
            }
        }

        // Create and fill the fullscreen-quad VB
        if (!mQuadVertexBuffer)
        {
            auto hr = device->CreateVertexBuffer(
                6u * sizeof(VertexFormat),
                D3DUSAGE_WRITEONLY,
                0,
                D3DPOOL_DEFAULT,
                &mQuadVertexBuffer,
                nullptr
            );

            if (FAILED(hr) || !mQuadVertexBuffer)
            {
                mQuadVertexBuffer = nullptr;
                return;
            }

            VertexFormat* vertexData = nullptr;
            hr = mQuadVertexBuffer->Lock(0, 0, reinterpret_cast<void**>(&vertexData), 0);
            if (FAILED(hr) || !vertexData)
            {
                mQuadVertexBuffer->Release();
                mQuadVertexBuffer = nullptr;
                return;
            }

            vertexData[0] = { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f };
            vertexData[1] = { -1.0f,  1.0f, 0.0f, 0.0f, 0.0f };
            vertexData[2] = { 1.0f, -1.0f, 0.0f, 1.0f, 1.0f };
            vertexData[3] = { -1.0f,  1.0f, 0.0f, 0.0f, 0.0f };
            vertexData[4] = { 1.0f,  1.0f, 0.0f, 1.0f, 0.0f };
            vertexData[5] = { 1.0f, -1.0f, 0.0f, 1.0f, 1.0f };

            mQuadVertexBuffer->Unlock();
        }
    }

    static auto OnBuildRenderListInit() -> void
    {
        auto onLostCB = rage::grcDevice::Functor0(NULL, OnDeviceLost, NULL, 0);
        auto onResetCB = rage::grcDevice::Functor0(NULL, OnDeviceReset, NULL, 0);
        rage::grcDevice::RegisterDeviceCallbacks(onLostCB, onResetCB);

        HMODULE hm = NULL;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&OnBuildRenderListInit, &hm);

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
    }

    static auto OnBuildRenderList() -> void
    {
        static auto initialized = false;
        if (!initialized)
        {
            initialized = true;
            OnBuildRenderListInit();
        }

        if (!snowEnabled)
            return;

        // Skip if any required GPU resource is unavailable (e.g., during/after a device reset)
        if (!mQuadVertexBuffer || !mQuadVertexDecl || !mBlitVS || !mBlitPS || !mSnowVS || !mSnowPS ||
            !mDiffuseRT || !mNormalRT || !mSpecularAoRT || !mStencilRT || !mDepthRT || !mNormalRtCopy || !mSnowTexture)
        {
            return;
        }

        auto mBeforeLightingCB = new T_CB_Generic_NoArgs(OnBeforeLighting);
        if (mBeforeLightingCB)
            mBeforeLightingCB->Append();
    }

    static auto OnBeforeLighting() -> void
    {
        auto HasSnow = [](CWeather::eWeatherType /*type*/) -> bool
        {
            return true;
        };

        auto device = rage::grcDevice::GetD3DDevice();

        static CWeather::eWeatherType currWeather;

        auto prevWeather = currWeather;
        currWeather = *CWeather::OldWeatherType;
        auto nextWeather = *CWeather::NewWeatherType;

        if (HasSnow(prevWeather) || HasSnow(currWeather) || HasSnow(nextWeather))
        {
            DWORD prevMinFilters[5];
            DWORD prevMagFilters[5];
            for (uint32_t i = 0; i < 5; ++i)
            {
                device->GetSamplerState(i, D3DSAMP_MINFILTER, &prevMinFilters[i]);
                device->GetSamplerState(i, D3DSAMP_MAGFILTER, &prevMagFilters[i]);

                device->SetSamplerState(i, D3DSAMP_MINFILTER, D3DTEXF_POINT);
                device->SetSamplerState(i, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            }

            auto* currGrcViewport = rage::GetCurrentViewport();

            rage::Vector4 pixelOffset;
            pixelOffset.x = (-1.0f / mNormalRT->mWidth);
            pixelOffset.y = (1.0f / mNormalRT->mHeight);
            device->SetVertexShaderConstantF(5, &pixelOffset.x, 1);

            // copy normal gbuffer
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

            // render snow
            rage::grcTextureFactoryPC::GetInstance()->LockRenderTarget(0, mDiffuseRT, nullptr);
            rage::grcTextureFactoryPC::GetInstance()->LockRenderTarget(1, mNormalRT, nullptr);
            rage::grcTextureFactoryPC::GetInstance()->LockRenderTarget(2, mSpecularAoRT, nullptr);

            device->SetVertexShader(mSnowVS);
            device->SetPixelShader(mSnowPS);

            device->SetVertexDeclaration(mQuadVertexDecl);
            device->SetStreamSource(0, mQuadVertexBuffer, 0, sizeof(VertexFormat));

            rage::Vector4 projParams;
            const auto a = currGrcViewport->mFarClip / (currGrcViewport->mFarClip - currGrcViewport->mNearClip);
            projParams.x = currGrcViewport->field_2CC;
            projParams.y = currGrcViewport->field_2C8;
            projParams.z = 1.0f / (0.0f - (a * currGrcViewport->mNearClip));
            projParams.w = projParams.z * a;

            device->SetVertexShaderConstantF(0, &projParams.x, 1);
            device->SetVertexShaderConstantF(1, &currGrcViewport->mViewInverseMatrix[0][0], 4);

            rage::Vector4 threshold;
            if (!HasSnow(currWeather) && HasSnow(nextWeather))
                threshold.y = *CWeather::InterpolationValue;
            else if (HasSnow(currWeather) && !HasSnow(nextWeather))
                threshold.y = 0.9999f - *CWeather::InterpolationValue;
            else if (!HasSnow(currWeather) && !HasSnow(nextWeather))
                threshold.y = 0.0f;
            else
                threshold.y = 0.9999f;

            threshold.y = pow(threshold.y, 0.20f);
            threshold.x = std::max(0.9999f, (threshold.y / (threshold.y + 0.15f)) * 1.15f);

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

            for (uint32_t i = 0; i < 5; ++i)
            {
                device->SetSamplerState(i, D3DSAMP_MINFILTER, prevMinFilters[i]);
                device->SetSamplerState(i, D3DSAMP_MAGFILTER, prevMagFilters[i]);
            }
        }
    }

    static auto OnAfterCopyLight(rage::CLightSource* light) -> void
    {
        const CWeather::eWeatherType CurrentWeather = CWeather::GetOldWeatherType();
        const CWeather::eWeatherType NextWeather = CWeather::GetNewWeatherType();
        const float InterpolationValue = CWeather::GetWeatherInterpolationValue();

        int CurrentCamera;
        rage::Vector3 CameraPosition;
        Natives::GetRootCam(&CurrentCamera);
        Natives::GetCamPos(CurrentCamera, &CameraPosition.x, &CameraPosition.y, &CameraPosition.z);

        static auto Smoothstep = [](float Edge0, float Edge1, float X)
        {
            float NormalizedX = std::clamp((X - Edge0) / (Edge1 - Edge0), 0.0f, 1.0f);

            return NormalizedX * NormalizedX * (3.0f - 2.0f * NormalizedX);
        };

        if (HasVolumes(CurrentWeather) || HasVolumes(NextWeather))
        {
            // Include spotlights with a radius between 8 and 20 (Most light models in lamppost.img are within that range)
            // and exclude lights previously volumetric, vehicle lights, traffic lights, fill lights and cutscene lights
            if (light->mType == rage::LT_SPOT && light->mRadius >= 8.0f && light->mRadius <= 20.0f && !(light->mFlags & 0x398))
            {
                // Append the light shaft flag
                light->mFlags |= 8;

                // Distance fading setup
                float DeltaX = CameraPosition.x - light->mPosition.x;
                float DeltaY = CameraPosition.y - light->mPosition.y;
                float DeltaZ = CameraPosition.z - light->mPosition.z;

                float Distance = std::sqrt(DeltaX * DeltaX + DeltaY * DeltaY + DeltaZ * DeltaZ);

                float FadeStart = 30.0f * *dwViewDistance;
                float FadeEnd = 105.0f * *dwViewDistance;

                float DistanceFade = 1.0f - Smoothstep(FadeStart, FadeEnd, Distance);

                // Transition from no volumes to volumes
                if (!HasVolumes(CurrentWeather) && HasVolumes(NextWeather))
                {
                    light->mVolumeIntensity = 4.0f * LightVolumeIntensities(NextWeather) * InterpolationValue * DistanceFade;
                    light->mVolumeScale = LightVolumeScales(NextWeather) * InterpolationValue * DistanceFade;
                }
                // Transition between volumes
                else if (HasVolumes(CurrentWeather) && HasVolumes(NextWeather))
                {
                    float CurrentVolumeIntensity = LightVolumeIntensities(CurrentWeather);
                    float NextVolumeIntensity = LightVolumeIntensities(NextWeather);

                    float CurrentVolumeScale = LightVolumeScales(CurrentWeather);
                    float NextVolumeScale = LightVolumeScales(NextWeather);

                    light->mVolumeIntensity = 4.0f * (CurrentVolumeIntensity + (NextVolumeIntensity - CurrentVolumeIntensity) * InterpolationValue) * DistanceFade;
                    light->mVolumeScale = (CurrentVolumeScale + (NextVolumeScale - CurrentVolumeScale) * InterpolationValue) * DistanceFade;
                }
                // Transition from volumes to no volumes
                else if (HasVolumes(CurrentWeather) && !HasVolumes(NextWeather))
                {
                    light->mVolumeIntensity = 4.0f * LightVolumeIntensities(CurrentWeather) * (1.0f - InterpolationValue) * DistanceFade;
                    light->mVolumeScale = LightVolumeScales(CurrentWeather) * (1.0f - InterpolationValue) * DistanceFade;
                }
            }
        }
    }

    static auto onCTimeCycleExtInit() -> void
    {
        CTimeCycleExt::Initialise(currentEpisodePath() / "pc" / "data" / "snowext.dat");
    }

    static auto HasVolumes(CWeather::eWeatherType Type) -> bool
    {
        switch (Type)
        {
            case CWeather::SUNNY_WINDY: return true;
            case CWeather::CLOUDY:      return true;
            case CWeather::RAIN:        return true;
            case CWeather::DRIZZLE:     return true;
            case CWeather::FOGGY:       return true;
            case CWeather::LIGHTNING:   return true;
            default: return false;
        }
    }

    static auto LightVolumeIntensities(CWeather::eWeatherType Type) -> float
    {
        switch (Type)
        {
            case CWeather::SUNNY_WINDY: return 1.1f;
            case CWeather::CLOUDY:      return 1.15f;
            case CWeather::RAIN:        return 1.25f;
            case CWeather::DRIZZLE:     return 1.2f;
            case CWeather::FOGGY:       return 1.4f;
            case CWeather::LIGHTNING:   return 1.3f;
            default: return 0.0f;
        }
    }

    static auto LightVolumeScales(CWeather::eWeatherType Type) -> float
    {
        switch (Type)
        {
            case CWeather::SUNNY_WINDY: return 0.1f;
            case CWeather::CLOUDY:      return 0.15f;
            case CWeather::RAIN:        return 0.25f;
            case CWeather::DRIZZLE:     return 0.2f;
            case CWeather::FOGGY:       return 0.4f;
            case CWeather::LIGHTNING:   return 0.3f;
            default: return 0.0f;
        }
    }

    static inline rage::grcTexturePC* vehicle_generic_glasswindows2;
    static inline rage::grcTexturePC* vehicle_generic_tyrewallblack;
    static inline rage::grcTexturePC* vehicle_generic_glassdirt;
    static inline rage::grcTexturePC* vehicle_genericmud_car;
    static inline rage::grcTexturePC* vehicle_genericmud_truck;

    static inline rage::grcTexturePC* vehicle_generic_glasswindows2_snow;
    static inline rage::grcTexturePC* vehicle_generic_tyrewallblack_snow;
    static inline rage::grcTexturePC* vehicle_generic_glassdirt_snow;
    static inline rage::grcTexturePC* vehicle_genericmud_car_snow;
    static inline rage::grcTexturePC* vehicle_genericmud_truck_snow;

    static inline SafetyHookInline shsub_41B920{};
    static auto __fastcall sub_41B920(rage::grcTextureReference* tex, void* edx) -> rage::grcTexturePC*
    {
        if (snowEnabled)
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

        return shsub_41B920.unsafe_fastcall<rage::grcTexturePC*>(tex, edx);
    }

    static inline SafetyHookInline shsub_A9F2D0{};
    static auto __stdcall sub_A9F2D0(unsigned int hash, int a2, char a3) -> void*
    {
        if (snowEnabled)
        {
            static std::vector<unsigned int> list = {
                0x9EA7160A, // bullet impact smoke
                0x107fae47, // bullet impact smoke
                0xC0D41722, // footsteps
                0xA73931FF, // tires1
                0xFF029DA7, // tires2
                0x52901658, // tires3
                0x0F94D257, // tires4
            };

            if (std::any_of(std::begin(list), std::end(list), [hash](const auto& i) { return i == hash; }))
                return nullptr;
        }

        return shsub_A9F2D0.unsafe_stdcall<void*>(hash, a2, a3);
    }

    struct MaterialBackup
    {
        int16_t originalFxGroup;
        int16_t originalHeliFx;
    };

    static inline auto nCurMaterialIndex = 0;
    static inline std::vector<std::pair<phMaterialGta::phMaterialGta*, MaterialBackup>> pMaterials;

    static inline SafetyHookInline shparse{};
    static auto __fastcall parse(phMaterialGta::phMaterialGta* _this, void* edx, const char* a2, float fVersion) -> void
    {
        shparse.unsafe_fastcall(_this, edx, a2, fVersion);
        pMaterials.emplace_back(_this, MaterialBackup{ _this->m_wFxGroup, _this->m_wHeliFx });
        nCurMaterialIndex++;
    }

    static inline std::vector<std::string> materialList = {
        "concrete", "concrete_freeway", "painted_road", "plaster_board",
        "rumble_strip", "tarmac", "brick_cobble", "paving_slabs",
        "rooftile", "gravel", "railway_gravel", "dirt_dry", "twigs",
        "mud_soft", "bushes", "flowers", "leaves_pile"
    };

    static auto applyMaterialsDatParams() -> void
    {
        for (auto& it : pMaterials)
        {
            if (std::any_of(std::begin(materialList), std::end(materialList), [&it](const auto& str) { return str == it.first->m_pszName; }))
            {
                it.first->m_wFxGroup = phMaterialGta::GRASS;
                it.first->m_wHeliFx = phMaterialGta::HELIFX_DIRT;
            }
        }
    }

    static auto restoreMaterialsDatParams() -> void
    {
        for (auto& it : pMaterials)
        {
            if (std::any_of(std::begin(materialList), std::end(materialList), [&it](const auto& str) { return str == it.first->m_pszName; }))
            {
                it.first->m_wFxGroup = it.second.originalFxGroup;
                it.first->m_wHeliFx = it.second.originalHeliFx;
            }
        }
    }

    static inline void** pShockingEvents = nullptr;
    static inline std::array<uint8_t, ShockingEvents::ShockingEventSize> originalDrivingOnPavement;
    static inline std::array<uint8_t, ShockingEvents::ShockingEventSize> originalMadDriver;

    static auto applyShockingEventsParams() -> void
    {
        memcpy(originalDrivingOnPavement.data(), pShockingEvents + ((ShockingEvents::ShockingEventDataFullSize * ShockingEvents::DrivingOnPavement) / sizeof(void*)) + (ShockingEvents::ShockingEventIndexSize / sizeof(void*)), ShockingEvents::ShockingEventSize);
        memcpy(originalMadDriver.data(), pShockingEvents + ((ShockingEvents::ShockingEventDataFullSize * ShockingEvents::MadDriver) / sizeof(void*)) + (ShockingEvents::ShockingEventIndexSize / sizeof(void*)), ShockingEvents::ShockingEventSize);

        memset(pShockingEvents + ((ShockingEvents::ShockingEventDataFullSize * ShockingEvents::DrivingOnPavement) / sizeof(void*)) + (ShockingEvents::ShockingEventIndexSize / sizeof(void*)), 0, ShockingEvents::ShockingEventSize);
        memset(pShockingEvents + ((ShockingEvents::ShockingEventDataFullSize * ShockingEvents::MadDriver) / sizeof(void*)) + (ShockingEvents::ShockingEventIndexSize / sizeof(void*)), 0, ShockingEvents::ShockingEventSize);
    }

    static auto restoreShockingEventsParams() -> void
    {
        memcpy(pShockingEvents + ((ShockingEvents::ShockingEventDataFullSize * ShockingEvents::DrivingOnPavement) / sizeof(void*)) + (ShockingEvents::ShockingEventIndexSize / sizeof(void*)), originalDrivingOnPavement.data(), ShockingEvents::ShockingEventSize);
        memcpy(pShockingEvents + ((ShockingEvents::ShockingEventDataFullSize * ShockingEvents::MadDriver) / sizeof(void*)) + (ShockingEvents::ShockingEventIndexSize / sizeof(void*)), originalMadDriver.data(), ShockingEvents::ShockingEventSize);
    }

    static inline gtaRainRender* pRainRender = nullptr;
    static inline gtaRainEmitter* pRainEmitter = nullptr;
    static inline gtaRainRender RainRenderCopy;
    static inline gtaRainEmitter RainEmitterCopy;

    static inline gtaRainRender* pStormRender = nullptr;
    static inline gtaRainEmitter* pStormEmitter = nullptr;
    static inline gtaRainRender StormRenderCopy;
    static inline gtaRainEmitter StormEmitterCopy;

    static auto applySnowRenderParams() -> void
    {
        RainRenderCopy = *pRainRender;
        RainEmitterCopy = *pRainEmitter;
        StormRenderCopy = *pStormRender;
        StormEmitterCopy = *pStormEmitter;

        pRainRender->motionBlurAmt = 0.01f;
        pRainRender->radius = 0.042f;
        pRainRender->radius2 = 0.0f;
        pRainRender->tintColorX = 10.0f;
        pRainRender->tintColorY = 10.0f;
        pRainRender->tintColorZ = 10.0f;
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

        pStormRender->motionBlurAmt = 0.01f;
        pStormRender->radius = 0.042f;
        pStormRender->radius2 = 0.0f;
        pStormRender->tintColorX = 10.0f;
        pStormRender->tintColorY = 10.0f;
        pStormRender->tintColorZ = 10.0f;
        pStormRender->tintColorW = 0.15f;
        pStormRender->tintColorPhase2X = 1.0f;
        pStormRender->tintColorPhase2Y = 1.0f;
        pStormRender->tintColorPhase2Z = 1.0f;
        pStormRender->tintColorPhase2W = 1.0f;

        pStormEmitter->velX = -10.0f;
        pStormEmitter->velY = -10.0f;
        pStormEmitter->velZ = -5.0f;
        pStormEmitter->velRangeX = 4.0f;
        pStormEmitter->velRangeY = 4.0f;
        pStormEmitter->velRangeZ = -2.2f;
        pStormEmitter->probablityPhase2 = 0.1f;
    }

    static auto restoreSnowRenderParams() -> void
    {
        *pRainRender = RainRenderCopy;
        *pRainEmitter = RainEmitterCopy;
        *pStormRender = StormRenderCopy;
        *pStormEmitter = StormEmitterCopy;
    }
};