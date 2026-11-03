module;

#include <common.hxx>
#include <aa/Upscaler.hpp>

export module temporal;

import common;
import comvars;
import d3dx9_43;
import settings;

export namespace TemporalAA
{
    using Mode = Antialiasing::Mode;
    bool IsTemporalAntialiasing(int32_t value);
    Mode GetMode();
}

class Temporal
{
public:
    using Matrix = Antialiasing::Matrix;
    using Mode = TemporalAA::Mode;

    struct Camera
    {
        Matrix View;
        Matrix Projection;
        Matrix ViewProjection;
        Matrix RenderedViewProjection;
        Antialiasing::Jitter Jitter;
        float Near{};
        float Far{};
        uint32_t Width{};
        uint32_t Height{};
        bool Valid{};
    };

    static inline Camera Current;
    static inline Camera Previous;
    static inline uint64_t Frame{};
    static inline uint64_t ResolvedFrame{};
    static inline uint64_t DepthFrame{};
    static inline bool HistoryValid{};
    static inline bool LastBridged{};
    static inline Matrix CameraReprojection;
    static inline std::array<float, 4> Wind{};
    static inline std::array<float, 4> PreviousWind{};
    static inline bool SceneActive{};
    static inline bool Internal{};
    static inline std::atomic<bool> Ready{};
    static inline uint32_t JitterPhases{ 8 };
    static inline float WeightMin{ 0.05f };
    static inline float WeightMax{ 0.2f };
    static inline float MotionWeight{ 0.1f };
    static inline float ClipGamma{ 1.25f };
    static inline float CameraCutDistance{ 25.0f };
    static inline float FSRSharpness{};
    static inline bool TrackObjects{ true };
    static inline bool TrackBones{ true };
    static inline Mode LastMode{ Mode::Off };
    static inline HMODULE Module{};
    static inline uint32_t Width{};
    static inline uint32_t Height{};
    static inline uint32_t HistoryIndex{};
    struct Target
    {
        rage::grcRenderTargetPC* Game{};
        IDirect3DSurface9* Surface{};

        explicit operator bool() const
        {
            return Game && Surface;
        }

        rage::grcRenderTargetPC* operator->() const
        {
            return Game;
        }
    };

    static inline rage::grcRenderTargetPC* StencilRT{};
    static inline Target DepthRT;
    static inline Target HistoryRT[2];
    static inline Target BridgeMotionRT;
    static inline Target BridgeColorRT;
    static inline Target BridgeOutputRT;
    static inline IDirect3DPixelShader9* ResolvePS{};
    static inline IDirect3DPixelShader9* DepthPS{};
    static inline IDirect3DPixelShader9* CopyPS{};
    static inline IDirect3DPixelShader9* BridgeMotionPS{};
    static inline IDirect3DStateBlock9* StateBlock{};
    static inline IDirect3DTexture9* BoneTexture{};
    static constexpr uint32_t BoneTexels{ 144 };
    static constexpr uint32_t BoneRows{ 2048 };

    static IDirect3DDevice9* RealDevice()
    {
        return RageDirect3DDevice9::m_pRealDevice ? *RageDirect3DDevice9::m_pRealDevice : nullptr;
    }

    static Antialiasing::Jitter JitterOf(uint32_t frame)
    {
        auto index = frame % JitterPhases + 1;
        return { Antialiasing::Halton(index, 2) - 0.5f, Antialiasing::Halton(index, 3) - 0.5f };
    }

    struct SavedState
    {
        IDirect3DDevice9* Device;
        IDirect3DSurface9* Targets[4]{};
        IDirect3DSurface9* Depth{};
        D3DVIEWPORT9 Viewport{};
        IDirect3DVertexBuffer9* Streams[4]{};
        UINT Offsets[4]{};
        UINT Strides[4]{};
        IDirect3DIndexBuffer9* Indices{};

        explicit SavedState(IDirect3DDevice9* device) : Device(device)
        {
            for (DWORD i = 0; i < 4; ++i)
            {
                Device->GetRenderTarget(i, &Targets[i]);
                Device->GetStreamSource(i, &Streams[i], &Offsets[i], &Strides[i]);
            }
            Device->GetDepthStencilSurface(&Depth);
            Device->GetViewport(&Viewport);
            Device->GetIndices(&Indices);
            StateBlock->Capture();
            Internal = true;
        }

        ~SavedState()
        {
            for (DWORD i = 0; i < 4; ++i)
                Device->SetTexture(i, nullptr);
            for (DWORD i = 1; i < 4; ++i)
                Device->SetRenderTarget(i, nullptr);
            for (DWORD i = 0; i < 4; ++i)
                Device->SetRenderTarget(i, Targets[i]);
            Device->SetDepthStencilSurface(Depth);
            StateBlock->Apply();
            Device->SetViewport(&Viewport);
            for (DWORD i = 0; i < 4; ++i)
            {
                Device->SetStreamSource(i, Streams[i], Offsets[i], Strides[i]);
                Antialiasing::detail::SafeRelease(Targets[i]);
                Antialiasing::detail::SafeRelease(Streams[i]);
            }
            Device->SetIndices(Indices);
            Antialiasing::detail::SafeRelease(Indices);
            Antialiasing::detail::SafeRelease(Depth);
            Internal = false;
        }
    };

    static void Sampler(IDirect3DDevice9* device, DWORD stage, IDirect3DBaseTexture9* texture, D3DTEXTUREFILTERTYPE filter = D3DTEXF_POINT)
    {
        device->SetTexture(stage, texture);
        device->SetSamplerState(stage, D3DSAMP_MINFILTER, filter);
        device->SetSamplerState(stage, D3DSAMP_MAGFILTER, filter);
        device->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        device->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        device->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        device->SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
    }

    static bool Draw(IDirect3DDevice9* device, IDirect3DSurface9* target, IDirect3DPixelShader9* shader)
    {
        for (DWORD i = 1; i < 4; ++i)
            device->SetRenderTarget(i, nullptr);
        device->SetDepthStencilSurface(nullptr);
        if (FAILED(device->SetRenderTarget(0, target)))
            return false;
        D3DVIEWPORT9 viewport{ 0, 0, Width, Height, 0.0f, 1.0f };
        device->SetViewport(&viewport);
        device->SetVertexShader(nullptr);
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
        device->SetPixelShader(shader);
        for (auto state : { D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_ALPHABLENDENABLE, D3DRS_ALPHATESTENABLE,
            D3DRS_STENCILENABLE, D3DRS_SCISSORTESTENABLE, D3DRS_SRGBWRITEENABLE, D3DRS_FOGENABLE, D3DRS_CLIPPLANEENABLE })
            device->SetRenderState(state, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, 15);
        struct Vertex
        {
            float X, Y, Z, W, U, V;
        };
        auto w = static_cast<float>(Width) - 0.5f;
        auto h = static_cast<float>(Height) - 0.5f;
        Vertex vertices[]
        {
            { -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f },
            { -0.5f, h, 0.0f, 1.0f, 0.0f, 1.0f },
            { w, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f },
            { w, h, 0.0f, 1.0f, 1.0f, 1.0f }
        };
        return SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex)));
    }

    static Target CreateTarget(const char* name, rage::grcTextureFormat format, uint32_t bits)
    {
        rage::grcRenderTargetDesc desc{};
        desc.field_0 = 1;
        desc.field_12 = 1;
        desc.field_8 = 1;
        desc.field_10 = 1;
        desc.field_11 = 1;
        desc.mFormat = format;
        desc.field_24 = false;
        auto factory = rage::grcTextureFactory::GetInstance();
        auto target = factory->CreateRenderTarget(name, 3, Width, Height, bits, &desc);
        if (target)
        {
            rage::grcDevice::grcResolveFlags flags{};
            factory->LockRenderTarget(0, target, nullptr);
            factory->UnlockRenderTarget(0, &flags);
        }
        IDirect3DSurface9* surface{};
        if (target && target->mD3DTexture)
            target->mD3DTexture->GetSurfaceLevel(0, &surface);
        return { target, surface };
    }

    static void LoadShaders(IDirect3DDevice9* device)
    {
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&LoadShaders), &Module);
        auto load = [&](int id, IDirect3DPixelShader9*& shader)
        {
            if (shader)
                return;
            auto resource = FindResourceW(Module, MAKEINTRESOURCEW(id), RT_RCDATA);
            auto data = resource ? LoadResource(Module, resource) : nullptr;
            auto code = data ? LockResource(data) : nullptr;
            if (code)
                device->CreatePixelShader(static_cast<const DWORD*>(code), &shader);
        };
        load(3001, ResolvePS);
        load(3002, DepthPS);
        load(3003, CopyPS);
        load(3004, BridgeMotionPS);
    }

    static void ReleaseResources()
    {
        Ready = false;
        SceneActive = false;
        if (auto device = RealDevice())
            device->SetTexture(D3DVERTEXTEXTURESAMPLER2, nullptr);
        Antialiasing::detail::TheUpscaler().BeforeReset();
        for (auto target : { &DepthRT, &HistoryRT[0], &HistoryRT[1], &BridgeMotionRT, &BridgeColorRT, &BridgeOutputRT })
        {
            Antialiasing::detail::SafeRelease(target->Surface);
            if (target->Game)
            {
                rage::grcTextureFactoryPC::RTCache.erase((*target)->mName);
                (*target)->Destroy();
                *target = {};
            }
        }
        Antialiasing::detail::SafeRelease(BoneTexture);
        Antialiasing::detail::SafeRelease(StateBlock);
        Entities.clear();
        Current = {};
        Previous = {};
        CameraViewports = {};
        SceneViewport = nullptr;
        Width = Height = 0;
        HistoryValid = false;
        DepthFrame = ResolvedFrame = 0;
    }

    static void CreateResources(uint32_t width, uint32_t height)
    {
        if (DepthRT && Width == width && Height == height)
            return;
        ReleaseResources();
        Width = width;
        Height = height;
        auto device = RealDevice();
        if (!device || !width || !height)
            return;
        LoadShaders(device);
        DepthRT = CreateTarget("TemporalDepth", rage::GRCFMT_R32F, 32);
        HistoryRT[0] = CreateTarget("TemporalHistory0", rage::GRCFMT_A16B16G16R16F, 64);
        HistoryRT[1] = CreateTarget("TemporalHistory1", rage::GRCFMT_A16B16G16R16F, 64);
        if (Antialiasing::detail::TheUpscaler().BridgeAvailable(device))
        {
            BridgeMotionRT = CreateTarget("TemporalBridgeMotion", rage::GRCFMT_G16R16F, 32);
            BridgeColorRT = CreateTarget("TemporalBridgeColor", rage::GRCFMT_A16B16G16R16F, 64);
            BridgeOutputRT = CreateTarget("TemporalBridgeOutput", rage::GRCFMT_A16B16G16R16F, 64);
        }
        device->CreateTexture(BoneTexels, BoneRows, 1, D3DUSAGE_DYNAMIC, D3DFMT_A32B32G32R32F, D3DPOOL_DEFAULT, &BoneTexture, nullptr);
        device->CreateStateBlock(D3DSBT_ALL, &StateBlock);
        Ready = DepthRT && HistoryRT[0] && HistoryRT[1] && StateBlock && ResolvePS && CopyPS && DepthPS;
        Antialiasing::detail::Log("Temporal targets: %ux%u, %s", Width, Height, Ready ? "ready" : "unavailable");
        InstallHooks();
    }

    struct DrawKey
    {
        void* Entity{};
        IDirect3DVertexBuffer9* Vertices{};
        IDirect3DIndexBuffer9* Indices{};
        IDirect3DVertexShader9* Shader{};
        UINT Offset{};
        UINT Stride{};
        INT BaseVertex{};
        UINT StartIndex{};
        UINT PrimitiveCount{};

        auto operator<=>(const DrawKey&) const = default;
    };

    struct DrawHistory
    {
        Matrix Transform;
        Matrix PreviousTransform;
        std::array<float, BoneTexels * 4> Bones{};
        uint64_t Frame{};
        uint64_t PreviousFrame{};
        int32_t Row{ -1 };
        bool Skinned{};
    };

    static inline std::map<DrawKey, DrawHistory> Entities;
    static inline std::vector<void*> EntityStack;
    static inline HRESULT(__stdcall* RealDraw)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT){};
    static inline void** HookedVTable{};

    static void InstallHooks()
    {
        auto device = RealDevice();
        auto vtable = *reinterpret_cast<void***>(device);
        if (vtable != HookedVTable && vtable[82] != reinterpret_cast<void*>(&DrawIndexedPrimitive))
        {
            RealDraw = reinterpret_cast<decltype(RealDraw)>(vtable[82]);
            injector::WriteMemory(&vtable[82], &DrawIndexedPrimitive, true);
            HookedVTable = vtable;
        }
    }

    static void UploadBones()
    {
        for (auto& [key, history] : Entities)
            history.Row = -1;
        if (!BoneTexture)
            return;
        D3DLOCKED_RECT lock{};
        if (FAILED(BoneTexture->LockRect(0, &lock, nullptr, D3DLOCK_DISCARD)))
            return;
        uint32_t row{};
        for (auto& [key, history] : Entities)
        {
            if (!history.Skinned || history.Frame + 1 != Frame || row == BoneRows)
                continue;
            history.Row = row;
            std::memcpy(static_cast<char*>(lock.pBits) + row * lock.Pitch, history.Bones.data(), sizeof(history.Bones));
            ++row;
        }
        BoneTexture->UnlockRect(0);
    }

    static HRESULT __stdcall DrawIndexedPrimitive(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, INT baseVertex,
        UINT minIndex, UINT numVertices, UINT startIndex, UINT primitiveCount)
    {
        if (SceneActive && !Internal)
            SetPreviousTransform(device, baseVertex, startIndex, primitiveCount);
        return RealDraw(device, type, baseVertex, minIndex, numVertices, startIndex, primitiveCount);
    }

    static void SetPreviousTransform(IDirect3DDevice9* device, INT baseVertex, UINT startIndex, UINT primitiveCount)
    {
        float constants[16]{};
        device->GetVertexShaderConstantF(8, constants, 4);
        auto current = Matrix::From(constants);
        auto unjittered = current;
        for (int row = 0; row < 4; ++row)
        {
            unjittered.m[row][0] -= current.m[row][3] * 2.0 * Current.Jitter.X / Width;
            unjittered.m[row][1] += current.m[row][3] * 2.0 * Current.Jitter.Y / Height;
        }
        auto previous = current * CameraReprojection;
        DrawHistory* record{};
        bool previousBones{};
        if (!EntityStack.empty())
        {
            DrawKey key;
            key.Entity = EntityStack.back();
            key.BaseVertex = baseVertex;
            key.StartIndex = startIndex;
            key.PrimitiveCount = primitiveCount;
            device->GetStreamSource(0, &key.Vertices, &key.Offset, &key.Stride);
            device->GetIndices(&key.Indices);
            device->GetVertexShader(&key.Shader);
            auto vertices = key.Vertices;
            auto indices = key.Indices;
            auto shader = key.Shader;
            Antialiasing::detail::SafeRelease(vertices);
            Antialiasing::detail::SafeRelease(indices);
            Antialiasing::detail::SafeRelease(shader);
            record = &Entities[key];
            if (record->Frame != Frame)
            {
                record->PreviousTransform = record->Transform;
                record->PreviousFrame = record->Frame;
                record->Frame = Frame;
                record->Transform = unjittered;
                IDirect3DVertexDeclaration9* declaration{};
                device->GetVertexDeclaration(&declaration);
                D3DVERTEXELEMENT9 elements[MAXD3DDECLLENGTH]{};
                UINT count{};
                record->Skinned = false;
                if (declaration && SUCCEEDED(declaration->GetDeclaration(elements, &count)))
                {
                    for (UINT i = 0; i < count && elements[i].Stream != 0xFF; ++i)
                        record->Skinned |= TrackBones && elements[i].Usage == D3DDECLUSAGE_BLENDINDICES;
                }
                Antialiasing::detail::SafeRelease(declaration);
                if (record->Skinned)
                    device->GetVertexShaderConstantF(64, record->Bones.data(), BoneTexels);
            }
            if (record->PreviousFrame + 1 == Frame && Previous.Valid)
            {
                previous = record->PreviousTransform;
                previousBones = record->Skinned && record->Row >= 0 && BoneTexture;
            }
        }
        previous.To(constants);
        device->SetVertexShaderConstantF(240, constants, 4);
        float bones[8]
        {
            1.0f / BoneTexels, record && record->Row >= 0 ? (record->Row + 0.5f) / BoneRows : 0.0f, 0.0f, 0.0f,
            previousBones ? 1.0f : 0.0f, 0.5f, 1.5f, 2.5f
        };
        device->SetVertexShaderConstantF(244, bones, 2);
        float wind[4]{};
        device->GetVertexShaderConstantF(233, wind, 1);
        if (wind[0] != 0.0f)
            std::copy_n(wind, 4, Wind.begin());
        device->SetVertexShaderConstantF(246, Previous.Valid && wind[0] != 0.0f ? PreviousWind.data() : wind, 1);
        Sampler(device, D3DVERTEXTEXTURESAMPLER2, BoneTexture);
        float pixel[8]
        {
            1.0f / Width, 1.0f / Height, -Current.Jitter.X / Width, -Current.Jitter.Y / Height,
            0.5f, -0.5f, 0.5f, 0.0f
        };
        device->SetPixelShaderConstantF(200, pixel, 2);
        DWORD mask{};
        device->GetRenderState(D3DRS_COLORWRITEENABLE3, &mask);
        device->SetRenderState(D3DRS_COLORWRITEENABLE3, mask | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
    }

    static void OnEntityBegin(void* entity)
    {
        if (SceneActive)
            EntityStack.push_back(entity);
    }

    static void OnEntityEnd(void*)
    {
        if (SceneActive && !EntityStack.empty())
            EntityStack.pop_back();
    }

    static void OnSceneBegin()
    {
        auto viewport = rage::GetCurrentViewport();
        StencilRT = rage::grcTextureFactoryPC::GetRTByName("_STENCIL_BUFFER_");
        if (!viewport || !Ready || !StencilRT || !StencilRT->mD3DTexture)
            return;
        Previous = Current;
        Current.View = Matrix::From(&viewport->mViewMatrix[0][0]);
        Current.Projection = Matrix::From(&viewport->mProjectionMatrix[0][0]);
        Current.RenderedViewProjection = Current.View * Current.Projection;
        Current.Jitter.X = -(viewport->mProjectionMatrix[2][0] - viewport->field_2D8) * viewport->mWidth * 0.5f;
        Current.Jitter.Y = (viewport->mProjectionMatrix[2][1] - viewport->field_2DC) * viewport->mHeight * 0.5f;
        Current.Projection.m[2][0] = viewport->field_2D8;
        Current.Projection.m[2][1] = viewport->field_2DC;
        Current.ViewProjection = Current.View * Current.Projection;
        Current.Width = viewport->mWidth;
        Current.Height = viewport->mHeight;
        Current.Near = viewport->mNearClip;
        Current.Far = viewport->mFarClip;
        Current.Valid = viewport->mIsPerspective && Current.Width == Width && Current.Height == Height &&
            Current.Near > 0.0f && Current.Far > Current.Near;
        CameraReprojection = Current.RenderedViewProjection.Inverse() * (Previous.Valid ? Previous.ViewProjection : Current.ViewProjection);
        ++Frame;
        PreviousWind = Wind;
        auto mode = TemporalAA::GetMode();
        Antialiasing::detail::TheUpscaler().Update(RealDevice());
        if (mode != LastMode || !Previous.Valid || !Current.Valid || ResolvedFrame + 1 != Frame)
            HistoryValid = false;
        LastMode = mode;
        auto a = Current.View.Inverse();
        auto b = Previous.View.Inverse();
        double distance{};
        double dot{};
        for (int i = 0; i < 3; ++i)
        {
            distance += (a.m[3][i] - b.m[3][i]) * (a.m[3][i] - b.m[3][i]);
            dot += a.m[2][i] * b.m[2][i];
        }
        HistoryValid &= distance < CameraCutDistance * CameraCutDistance && dot > 0.5;
        EntityStack.clear();
        std::erase_if(Entities, [](const auto& entry) { return entry.second.Frame + 1 < Frame; });
        if (mode == Mode::Off || !Current.Valid)
            return;
        UploadBones();
        SceneActive = true;
    }

    static void OnSceneEnd()
    {
        SceneActive = false;
        EntityStack.clear();
        auto depth = rage::grcTextureFactoryPC::GetRTByName("_DEFERRED_GBUFFER_3_");
        if (LastMode == Mode::Off || !Current.Valid || !DepthRT || !DepthPS || !depth || !depth->mD3DTexture)
            return;
        auto device = RealDevice();
        SavedState state(device);
        float camera[]{ Current.Near, Current.Far, 0.0f, 0.0f };
        device->SetPixelShaderConstantF(2, camera, 1);
        Sampler(device, 2, depth->mD3DTexture);
        if (Draw(device, DepthRT.Surface, DepthPS))
            DepthFrame = Frame;
    }

    static bool Resolve(IDirect3DDevice9* device, IDirect3DTexture9* scene, IDirect3DSurface9* output)
    {
        if (!device || !scene || !output || LastMode == Mode::Off || DepthFrame != Frame || ResolvedFrame == Frame ||
            !Ready || !StencilRT || !StencilRT->mD3DTexture || !HistoryRT[0] || !HistoryRT[1] || !ResolvePS || !CopyPS || !StateBlock)
            return false;
        D3DSURFACE_DESC sceneDesc{}, outputDesc{};
        if (FAILED(scene->GetLevelDesc(0, &sceneDesc)) || FAILED(output->GetDesc(&outputDesc)) ||
            sceneDesc.Width != Width || sceneDesc.Height != Height || outputDesc.Width != Width || outputDesc.Height != Height)
            return false;
        SavedState state(device);
        float previousClip[16];
        CameraReprojection.To(previousClip);
        device->SetPixelShaderConstantF(7, previousClip, 4);
        float jitter[]{ Current.Jitter.X / Width, Current.Jitter.Y / Height, 0.0f, 0.0f };
        device->SetPixelShaderConstantF(11, jitter, 1);
        float pixel[]{ 1.0f / Width, 1.0f / Height, 0.0f, 0.0f };
        device->SetPixelShaderConstantF(0, pixel, 1);
        Sampler(device, 2, DepthRT->mD3DTexture);
        bool bridged{};
        if (LastMode == Mode::DLAA || LastMode == Mode::FSR)
        {
            if (BridgeColorRT && BridgeMotionRT && BridgeOutputRT && BridgeMotionPS)
            {
                Sampler(device, 0, scene);
                bool ready = Draw(device, BridgeColorRT.Surface, CopyPS);
                Sampler(device, 0, nullptr);
                Sampler(device, 1, StencilRT->mD3DTexture);
                ready &= Draw(device, BridgeMotionRT.Surface, BridgeMotionPS);
                Antialiasing::detail::UpscalerFrame data;
                data.Color = BridgeColorRT->mD3DTexture;
                data.Depth = DepthRT->mD3DTexture;
                data.Motion = BridgeMotionRT->mD3DTexture;
                data.Output = BridgeOutputRT->mD3DTexture;
                data.Width = Width;
                data.Height = Height;
                data.JitterPixels = Current.Jitter;
                data.CameraNear = Current.Near;
                data.CameraFar = Current.Far;
                data.CameraFovY = 2.0f * std::atan(1.0f / static_cast<float>(Current.Projection.m[1][1]));
                static LARGE_INTEGER lastTime{};
                LARGE_INTEGER now{}, frequency{};
                QueryPerformanceCounter(&now);
                QueryPerformanceFrequency(&frequency);
                data.FrameTimeMs = lastTime.QuadPart ? std::clamp(static_cast<float>(1000.0 * (now.QuadPart - lastTime.QuadPart) / frequency.QuadPart), 1.0f, 200.0f) : 16.6f;
                lastTime = now;
                data.Reset = !HistoryValid;
                data.DLSSPreset = Antialiasing::detail::Config().DLSSPreset;
                data.Sharpness = LastMode == Mode::FSR ? FSRSharpness : 0.0f;
                auto backend = LastMode == Mode::DLAA ? Antialiasing::Backend::DLSS : Antialiasing::Backend::FSR;
                bridged = ready && Antialiasing::detail::TheUpscaler().Evaluate(backend, data);
            }
        }
        auto next = HistoryIndex ^ 1;
        if (bridged)
        {
            Sampler(device, 0, BridgeOutputRT->mD3DTexture);
            if (!Draw(device, output, CopyPS))
                return false;
            HistoryValid = true;
        }
        else
        {
            HistoryValid &= !LastBridged;
            Sampler(device, 0, scene);
            Sampler(device, 1, StencilRT->mD3DTexture);
            Sampler(device, 2, DepthRT->mD3DTexture);
            Sampler(device, 3, HistoryRT[HistoryIndex]->mD3DTexture, D3DTEXF_LINEAR);
            float constants[12]
            {
                1.0f / Width, 1.0f / Height, 0.0f, 0.0f,
                WeightMin, WeightMax, ClipGamma, HistoryValid ? 1.0f : 0.0f,
                Current.Near, Current.Far, MotionWeight, 0.0f
            };
            device->SetPixelShaderConstantF(0, constants, 3);
            if (!Draw(device, HistoryRT[next].Surface, ResolvePS))
                return false;
            Sampler(device, 3, nullptr);
            Sampler(device, 0, HistoryRT[next]->mD3DTexture);
            if (!Draw(device, output, CopyPS))
                return false;
            HistoryIndex = next;
            HistoryValid = true;
        }
        LastBridged = bridged;
        ResolvedFrame = Frame;
        return true;
    }

    static inline void(__fastcall* RegenerateMatrices)(rage::grcViewport*, void*){};

    static void RemoveJitterAfterResolve()
    {
        if (!RegenerateMatrices || ResolvedFrame != Frame)
            return;
        auto viewport = rage::GetCurrentViewport();
        if (!viewport || !viewport->mIsPerspective || viewport->mWidth != Current.Width || viewport->mHeight != Current.Height)
            return;
        viewport->mProjectionMatrix[2][0] = viewport->field_2D8;
        viewport->mProjectionMatrix[2][1] = viewport->field_2DC;
        RegenerateMatrices(viewport, nullptr);
    }
    static inline uint32_t JitterCounter = 0;

    static inline std::array<rage::grcViewport*, 4> CameraViewports{};
    static inline rage::grcViewport* SceneViewport = nullptr;

    static bool SameCamera(const rage::grcViewport* a, const rage::grcViewport* b)
    {
        return a->mWidth == b->mWidth && a->mHeight == b->mHeight && a->mFov == b->mFov && a->mNearClip == b->mNearClip &&
            a->mFarClip == b->mFarClip && a->mProjectionMatrix[0][0] == b->mProjectionMatrix[0][0] &&
            a->mProjectionMatrix[1][1] == b->mProjectionMatrix[1][1];
    }

    static bool IsCameraScene()
    {
        if (!CRenderPhase::sm_pCurrent || !*CRenderPhase::sm_pCurrent)
            return false;

        auto phaseViewport = reinterpret_cast<rage::grcViewport*>(*CRenderPhase::sm_pCurrent + 176);
        if (!phaseViewport->mIsPerspective)
            return false;

        if (!CameraViewports[0])
            return true;

        for (auto viewport : CameraViewports)
        {
            if (viewport && SameCamera(viewport, phaseViewport))
            {
                SceneViewport = viewport;
                return true;
            }
        }
        return false;
    }

    static inline injector::hook_back<void(__fastcall*)(void*, void*, rage::grcViewport*, float, float, float, float)> hbSetCameraPerspective;
    static void __fastcall SetCameraPerspective(void* _this, void* edx, rage::grcViewport* viewport, float fov, float aspect, float nearClip, float farClip)
    {
        if (viewport && std::find(CameraViewports.begin(), CameraViewports.end(), viewport) == CameraViewports.end())
        {
            std::rotate(CameraViewports.rbegin(), CameraViewports.rbegin() + 1, CameraViewports.rend());
            CameraViewports[0] = viewport;
        }

        if (!viewport || viewport != SceneViewport || TemporalAA::GetMode() == TemporalAA::Mode::Off || viewport->mWidth <= 0 || viewport->mHeight <= 0)
            return hbSetCameraPerspective.fun(_this, edx, viewport, fov, aspect, nearClip, farClip);

        auto frame = CTimer::m_frameCount ? *CTimer::m_frameCount : JitterCounter++;
        auto jitter = JitterOf(frame);

        auto shiftX = viewport->field_2D8;
        auto shiftY = viewport->field_2DC;
        viewport->field_2D8 = shiftX - 2.0f * jitter.X / static_cast<float>(viewport->mWidth);
        viewport->field_2DC = shiftY + 2.0f * jitter.Y / static_cast<float>(viewport->mHeight);
        hbSetCameraPerspective.fun(_this, edx, viewport, fov, aspect, nearClip, farClip);
        viewport->field_2D8 = shiftX;
        viewport->field_2DC = shiftY;
    }


    class CTemporalDC : public CBaseDC
    {
    public:
        enum class Type : uint32_t
        {
            SceneBegin, SceneEnd, EntityBegin, EntityEnd
        };

        Type type;
        void* entity;

    public:
        CTemporalDC(Type t, void* e = nullptr) : CBaseDC()
        {
            type = t;
            entity = e;
        }

        void DrawCommand() override
        {
            switch (type)
            {
            case Type::SceneBegin: OnSceneBegin(); break;
            case Type::SceneEnd: OnSceneEnd(); break;
            case Type::EntityBegin: OnEntityBegin(entity); break;
            case Type::EntityEnd: OnEntityEnd(entity); break;
            }
        }

        int32_t GetSize() override
        {
            return sizeof(CTemporalDC);
        }
    };

    static inline bool bBuildingSceneList = false;
    static inline bool bMarkingEntities = false;  // while the scene list is built with AA on

    static void Append(CTemporalDC::Type type, void* entity = nullptr)
    {
        auto dc = new CTemporalDC(type, entity);
        if (dc)
            dc->Append();
    }

    template<size_t N>
    struct AddToDrawListHook
    {
        static inline SafetyHookInline hook{};
        static int __fastcall AddToDrawList(void* entity, void* edx, int a2, int a3, int a4, int a5)
        {
            if (!bMarkingEntities)
                return hook.unsafe_thiscall<int>(entity, a2, a3, a4, a5);

            Append(CTemporalDC::Type::EntityBegin, entity);
            auto result = hook.unsafe_thiscall<int>(entity, a2, a3, a4, a5);
            Append(CTemporalDC::Type::EntityEnd, entity);
            return result;
        }
    };


    Temporal()
    {
        FusionFix::onInitEventAsync() += []()
        {
            SettingsReader reader;
            Antialiasing::Options options;
            options.BridgeName = L"Local\\GTAIV.EFLC.FusionFix.Upscaler";
            options.ClientName = "GTAIV.EFLC.FusionFix";
            options.Log = GetThisModulePath() / L"GTAIV.EFLC.FusionFix.AntiAliasing.log";
            options.HubLog = GetThisModulePath() / L"GTAIV.EFLC.FusionFix.Upscaler.log";
            options.DLSSPreset = std::clamp(reader.ReadInteger("TEMPORAL", "DLSSPreset", 0), 0, 15);
            options.D3D9On12Interop = reader.ReadInteger("TEMPORAL", "D3D9on12Interop", 0) != 0;
            Antialiasing::detail::Config() = options;
            JitterPhases = std::clamp(reader.ReadInteger("TEMPORAL", "JitterPhases", 8), 2, 128);
            WeightMin = std::clamp(reader.ReadFloat("TEMPORAL", "CurrentFrameWeightMin", 0.05f), 0.01f, 1.0f);
            WeightMax = std::clamp(reader.ReadFloat("TEMPORAL", "CurrentFrameWeightMax", 0.2f), WeightMin, 1.0f);
            MotionWeight = std::max(reader.ReadFloat("TEMPORAL", "CurrentFrameWeightPerPixelOfMotion", 0.1f), 0.0f);
            ClipGamma = std::clamp(reader.ReadFloat("TEMPORAL", "VarianceClipGamma", 1.25f), 0.5f, 4.0f);
            CameraCutDistance = std::max(reader.ReadFloat("TEMPORAL", "CameraCutDistance", 25.0f), 1.0f);
            FSRSharpness = std::clamp(reader.ReadFloat("TEMPORAL", "FSRSharpness", 0.0f), 0.0f, 1.0f);
            TrackObjects = reader.ReadInteger("TEMPORAL", "ObjectMotionVectors", 1) != 0;
            TrackBones = reader.ReadInteger("TEMPORAL", "SkinnedMotionVectors", 1) != 0;
            auto pattern = hook::pattern("F3 0F 11 04 24 53 E8 ? ? ? ? 5F 5E 5B 8B E5");
            if (!pattern.empty())
                hbSetCameraPerspective.fun = injector::MakeCALL(pattern.get_first(6), SetCameraPerspective, true).get();
            else
            {
                pattern = hook::pattern("B9 ? ? ? ? D9 47 54 D9 5C 24 08 D9 44 24 ? D9 5C 24 04 D9 47 50 D9 1C 24 56 E8");
                if (!pattern.empty())
                    hbSetCameraPerspective.fun = injector::MakeCALL(pattern.get_first(27), SetCameraPerspective, true).get();
            }

            pattern = find_pattern("55 8B EC 83 E4 F8 51 56 8B F1 8D 86 80 00 00 00 50 8D 86 80 01 00 00 50 8D 8E C0 00 00 00 E8",
                "56 8B F1 8D 86 80 00 00 00 50 8D 86 80 01 00 00 50 8D 8E C0 00 00 00 E8");
            if (!pattern.empty())
                RegenerateMatrices = reinterpret_cast<decltype(RegenerateMatrices)>(pattern.get_first(0));

            pattern = find_pattern("83 EC 14 53 55 8B 6C 24 20 8B D9", "83 EC 14 55 8B 6C 24 1C 83 FD 01");
            if (!pattern.empty())
                AddToDrawListHook<0>::hook = safetyhook::create_inline(pattern.get_first(0), AddToDrawListHook<0>::AddToDrawList);

            pattern = find_pattern("83 EC 44 A1 ? ? ? ? 33 C4 89 44 24 40 8A 44 24 50", "83 EC 30 53 8B 5C 24 3C 56 8B F1");
            if (!pattern.empty())
                AddToDrawListHook<1>::hook = safetyhook::create_inline(pattern.get_first(0), AddToDrawListHook<1>::AddToDrawList);

            pattern = find_pattern("83 EC 08 56 8B F1 F7 86 10 02 00 00 00 00 10 00", "51 56 8B F1 F7 86 10 02 00 00 00 00 10 00");
            if (!pattern.empty())
                AddToDrawListHook<2>::hook = safetyhook::create_inline(pattern.get_first(0), AddToDrawListHook<2>::AddToDrawList);

            pattern = find_pattern("53 8B D9 8B 83 14 03 00 00", "56 8B F1 8B 86 14 03 00 00");
            if (!pattern.empty())
                AddToDrawListHook<3>::hook = safetyhook::create_inline(pattern.get_first(0), AddToDrawListHook<3>::AddToDrawList);

            pattern = hook::pattern("51 56 8B F1 83 7E 34 00");
            if (!pattern.empty())
                AddToDrawListHook<4>::hook = safetyhook::create_inline(pattern.get_first(0), AddToDrawListHook<4>::AddToDrawList);


            FusionFixSettings.SetAvailability("PREF_ANTIALIASING", [](int32_t value)
            {
                if (value == FusionFixSettings.AntialiasingText.eTAA)
                    return GetD3DX9_43DLL() != nullptr;
                if (value == FusionFixSettings.AntialiasingText.eDLAA || value == FusionFixSettings.AntialiasingText.eFSR)
                {
                    auto backend = value == FusionFixSettings.AntialiasingText.eDLAA ? Antialiasing::Backend::DLSS : Antialiasing::Backend::FSR;
                    return Antialiasing::detail::TheUpscaler().BridgeAvailable(RealDevice()) && Antialiasing::detail::TheUpscaler().IsAvailable(backend);
                }
                return true;
            });
            FusionFixSettings.RefreshAvailability("PREF_ANTIALIASING");
            auto refresh = []()
            {
                static uint32_t generation{ UINT32_MAX };
                auto current = Antialiasing::detail::TheUpscaler().Generation();
                if (generation != current)
                {
                    generation = current;
                    FusionFixSettings.RefreshAvailability("PREF_ANTIALIASING");
                }
            };
            FusionFix::onGameProcessEvent() += refresh;
            FusionFix::onMenuDrawingEvent() += refresh;
            FusionFix::onShutdownEvent() += []() { Antialiasing::detail::TheUpscaler().Shutdown(); };
            CRenderPhaseDeferredLighting_SceneToGBuffer::OnBuildRenderList() += []()
            {
                bBuildingSceneList = IsCameraScene();
                bMarkingEntities = bBuildingSceneList && TrackObjects && TemporalAA::GetMode() != Mode::Off;
                if (bBuildingSceneList)
                    Append(CTemporalDC::Type::SceneBegin);
            };
            CRenderPhaseDeferredLighting_SceneToGBuffer::OnAfterBuildRenderList() += []()
            {
                if (bBuildingSceneList)
                    Append(CTemporalDC::Type::SceneEnd);
                bBuildingSceneList = false;
                bMarkingEntities = false;
            };
        };
    }
} Temporal;

export namespace TemporalAA
{
    bool IsTemporalAntialiasing(int32_t value)
    {
        return value == FusionFixSettings.AntialiasingText.eTAA || value == FusionFixSettings.AntialiasingText.eDLAA || value == FusionFixSettings.AntialiasingText.eFSR;
    }

    Mode GetMode()
    {
        if (!Temporal::Ready)
            return Mode::Off;
        static auto aa = FusionFixSettings.GetRef("PREF_ANTIALIASING");
        if (!aa)
            return Mode::Off;
        auto value = aa->get();
        if (value == FusionFixSettings.AntialiasingText.eTAA)
            return Mode::TAA;
        if (value == FusionFixSettings.AntialiasingText.eDLAA)
            return Mode::DLAA;
        if (value == FusionFixSettings.AntialiasingText.eFSR)
            return Mode::FSR;
        return Mode::Off;
    }

    void LoadShaders(IDirect3DDevice9* device)
    {
        Temporal::LoadShaders(device);
    }

    void CreateResources(uint32_t width, uint32_t height)
    {
        Temporal::CreateResources(width, height);
    }

    void ReleaseResources()
    {
        Temporal::ReleaseResources();
    }

    bool Resolve(IDirect3DDevice9* device, IDirect3DTexture9* scene, IDirect3DTexture9*, IDirect3DSurface9* output)
    {
        return Temporal::Resolve(device, scene, output);
    }

    bool IsSceneResolved()
    {
        return Temporal::Frame != 0 && Temporal::ResolvedFrame == Temporal::Frame;
    }

    void OnPostFXDrawn()
    {
        Temporal::RemoveJitterAfterResolve();
    }
}
