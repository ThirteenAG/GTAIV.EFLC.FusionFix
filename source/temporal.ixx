module;

#include <common.hxx>

export module temporal;

import common;
import comvars;
import d3dx9_43;
import settings;
import upscaler;

#define IDR_TEMPORAL_VS_RIGID           3001
#define IDR_TEMPORAL_VS_SKINNED         3002
#define IDR_TEMPORAL_PS_VELOCITY        3003
#define IDR_TEMPORAL_VS_BONEWRITE       3004
#define IDR_TEMPORAL_PS_BONEWRITE       3005
#define IDR_TEMPORAL_PS_CAMERA          3006
#define IDR_TEMPORAL_PS_RESOLVE         3007
#define IDR_TEMPORAL_PS_UPSCALER_DEPTH  3008
#define IDR_TEMPORAL_PS_OPAQUE_LUMA     3009
#define IDR_TEMPORAL_PS_REACTIVE        3010

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p)=NULL; } }
#endif

// Temporal anti-aliasing for the scene.
//
// - The projection of the main camera viewport is offset by a sub-pixel Halton sequence every frame.
//   Render phases copy the whole grcViewport, so the jitter reaches every scene pass, and the render
//   thread reads it back from the projection it actually rendered with.
// - Motion vectors: camera motion is reprojected from the depth buffer. Moving entities (peds,
//   vehicles, objects) are wrapped with draw list markers while the G-buffer render list is built; their
//   draws are captured on the render thread and drawn again after the G-buffer pass with the world
//   matrix and bones of the previous frame.
// - Motion vectors and the depth used by the resolve, DLAA and FSR are taken right after the G-buffer
//   pass, before glass and other transparent geometry is drawn: through a window they describe what is
//   behind it, which is also what the color mostly shows. Depth taken later would describe the glass,
//   and reprojecting the scene behind it with that depth smears it.
// - Particles, glass and other transparent geometry have no motion vectors of their own. The luminance of
//   the scene is copied right after the fog pass, which comes after water and before transparent geometry
//   and visual effects; where the final scene differs from it, the resolve and FSR follow the current
//   frame more (reactive mask).
// - The resolve runs on the HDR scene before the game's post processing downsamples it for bloom and
//   exposure (bloom from the jittered scene would jitter), and so before depth of field and tone mapping.
//   DLAA and FSR replace it when they are available (see upscaler.ixx), with the same jitter and motion
//   vectors.

namespace TemporalMath
{
    struct Matrix
    {
        double m[4][4]{};

        static Matrix Identity()
        {
            Matrix r;
            for (int i = 0; i < 4; ++i)
                r.m[i][i] = 1.0;
            return r;
        }

        static Matrix From(const float(&f)[4][4])
        {
            Matrix r;
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    r.m[i][j] = f[i][j];
            return r;
        }

        void To(float* f) const
        {
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    f[i * 4 + j] = static_cast<float>(m[i][j]);
        }

        Matrix operator*(const Matrix& b) const
        {
            Matrix r;
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    r.m[i][j] = m[i][0] * b.m[0][j] + m[i][1] * b.m[1][j] + m[i][2] * b.m[2][j] + m[i][3] * b.m[3][j];
            return r;
        }

        Matrix Inverse() const
        {
            // Gauss-Jordan with partial pivoting
            Matrix a = *this;
            Matrix r = Identity();
            for (int c = 0; c < 4; ++c)
            {
                int pivot = c;
                for (int i = c + 1; i < 4; ++i)
                    if (std::abs(a.m[i][c]) > std::abs(a.m[pivot][c]))
                        pivot = i;
                if (std::abs(a.m[pivot][c]) < 1e-12)
                    return Identity();
                std::swap(a.m[c], a.m[pivot]);
                std::swap(r.m[c], r.m[pivot]);
                double inv = 1.0 / a.m[c][c];
                for (int j = 0; j < 4; ++j)
                {
                    a.m[c][j] *= inv;
                    r.m[c][j] *= inv;
                }
                for (int i = 0; i < 4; ++i)
                {
                    if (i == c)
                        continue;
                    double f = a.m[i][c];
                    for (int j = 0; j < 4; ++j)
                    {
                        a.m[i][j] -= f * a.m[c][j];
                        r.m[i][j] -= f * r.m[c][j];
                    }
                }
            }
            return r;
        }
    };
}

export namespace TemporalAA
{
    enum class Mode
    {
        Off, TAA, DLAA, FSR
    };

    struct FrameCamera
    {
        TemporalMath::Matrix View;
        TemporalMath::Matrix Projection;        // as rendered, jittered
        TemporalMath::Matrix ProjectionNoJitter;
        TemporalMath::Matrix ViewProjectionNoJitter;
        float JitterNdc[2]{};                   // P[2][0] and P[2][1] minus their unjittered values
        float JitterPixels[2]{};                // how far the rendered content moved, in pixels (y down)
        float Near = 0.0f;
        float Far = 0.0f;
        float FovY = 0.0f;                      // radians
        int32_t Width = 0;
        int32_t Height = 0;
        uint32_t Frame = 0;
        bool Valid = false;
    };

    // Filled on the render thread when the G-buffer pass starts
    FrameCamera CurrentCamera;
    FrameCamera PreviousCamera;

    // Render thread frame counter, advanced once per scene
    uint32_t SceneFrame = 0;

    bool IsTemporalAntialiasing(int32_t value);
    Mode GetMode();
    bool IsMotionVectorsReady();
    IDirect3DTexture9* GetMotionVectors();
    bool HistoryValid();
    void OnFogDrawn();
}

class Temporal
{
public:
    // Settings from the ini
    static inline int32_t nJitterPhases = 8;
    static inline float fCurrentWeightMin = 0.08f;
    static inline float fCurrentWeightMax = 0.2f;
    static inline float fCurrentWeightPerPixel = 0.1f;
    static inline float fVarianceClipGamma = 1.25f;
    static inline float fLumaWeight = 1.0f;
    static inline float fObjectDepthBias = 0.00002f;
    static inline bool bObjectMotionVectors = true;
    static inline bool bSkinnedMotionVectors = true;
    static inline float fCameraCutDistance = 25.0f;
    static inline int32_t nDLSSPreset = 0;
    static inline float fFSRSharpness = 0.0f;
    static inline bool bReactiveMask = true;
    static inline float fReactiveScale = 2.0f;
    static inline float fReactiveMax = 0.75f;

    static inline HMODULE hm = NULL;

    // ---------------------------------------------------------------------------------------------
    // Resources, owned by PostFX device callbacks

    static inline rage::grcRenderTargetPC* MotionRT = nullptr;
    static inline rage::grcRenderTargetPC* HistoryRT[2] = {};
    static inline rage::grcRenderTargetPC* BoneRT = nullptr;
    static inline rage::grcRenderTargetPC* DepthRT = nullptr;       // standard [0, 1] depth before transparent geometry
    static inline rage::grcRenderTargetPC* OpaqueRT = nullptr;      // scene luminance before transparent geometry
    static inline rage::grcRenderTargetPC* ReactiveRT = nullptr;    // reactive mask for FSR
    static inline uint32_t OpaqueFrame = 0;                          // SceneFrame OpaqueRT was written in
    static inline uint32_t HistoryIndex = 0;
    static inline uint32_t HistoryFrame = 0;          // SceneFrame the history was written in
    static inline int32_t HistoryWidth = 0;
    static inline int32_t HistoryHeight = 0;
    static inline bool bVertexTextureSupported = false;

    static constexpr int32_t BoneTexels = 144;        // 48 bones, 3 rows each
    static constexpr int32_t BoneRows = 512;          // skinned draws per frame with previous bones

    static inline IDirect3DVertexShader9* VelocityRigidVS = nullptr;
    static inline IDirect3DVertexShader9* VelocitySkinnedVS = nullptr;
    static inline IDirect3DPixelShader9* VelocityPS = nullptr;
    static inline IDirect3DVertexShader9* BoneWriteVS = nullptr;
    static inline IDirect3DPixelShader9* BoneWritePS = nullptr;
    static inline IDirect3DPixelShader9* CameraMotionPS = nullptr;
    static inline IDirect3DPixelShader9* ResolvePS = nullptr;
    static inline IDirect3DPixelShader9* DepthPS = nullptr;
    static inline IDirect3DPixelShader9* OpaqueLumaPS = nullptr;
    static inline IDirect3DPixelShader9* ReactivePS = nullptr;
    static inline IDirect3DVertexDeclaration9* BoneWriteDecl = nullptr;

    static bool ShadersLoaded()
    {
        return VelocityRigidVS && VelocitySkinnedVS && VelocityPS && BoneWriteVS && BoneWritePS && CameraMotionPS && ResolvePS && DepthPS && BoneWriteDecl;
    }

    static void LoadShaders(IDirect3DDevice9* pDevice)
    {
        auto loadCompiledShader = [&](int resourceID, auto& shader) -> bool
        {
            if (shader)
                return true;
            HRSRC hRes = FindResourceW(hm, MAKEINTRESOURCEW(resourceID), RT_RCDATA);
            if (!hRes) return false;
            HGLOBAL hGlob = LoadResource(hm, hRes);
            if (!hGlob) return false;
            void* buffer = LockResource(hGlob);
            if (!buffer) return false;
            using ShaderType = std::remove_reference_t<decltype(shader)>;
            if constexpr (std::is_same_v<ShaderType, IDirect3DPixelShader9*>)
                return pDevice->CreatePixelShader((DWORD*)buffer, &shader) == S_OK && shader;
            else if constexpr (std::is_same_v<ShaderType, IDirect3DVertexShader9*>)
                return pDevice->CreateVertexShader((DWORD*)buffer, &shader) == S_OK && shader;
            return false;
        };

        loadCompiledShader(IDR_TEMPORAL_VS_RIGID, VelocityRigidVS);
        loadCompiledShader(IDR_TEMPORAL_VS_SKINNED, VelocitySkinnedVS);
        loadCompiledShader(IDR_TEMPORAL_PS_VELOCITY, VelocityPS);
        loadCompiledShader(IDR_TEMPORAL_VS_BONEWRITE, BoneWriteVS);
        loadCompiledShader(IDR_TEMPORAL_PS_BONEWRITE, BoneWritePS);
        loadCompiledShader(IDR_TEMPORAL_PS_CAMERA, CameraMotionPS);
        loadCompiledShader(IDR_TEMPORAL_PS_RESOLVE, ResolvePS);
        loadCompiledShader(IDR_TEMPORAL_PS_UPSCALER_DEPTH, DepthPS);
        loadCompiledShader(IDR_TEMPORAL_PS_OPAQUE_LUMA, OpaqueLumaPS);
        loadCompiledShader(IDR_TEMPORAL_PS_REACTIVE, ReactivePS);

        if (!BoneWriteDecl)
        {
            D3DVERTEXELEMENT9 elements[] =
            {
                { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
                { 0, 16, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
                D3DDECL_END()
            };
            pDevice->CreateVertexDeclaration(elements, &BoneWriteDecl);
        }

        IDirect3D9* d3d = nullptr;
        if (pDevice->GetDirect3D(&d3d) == S_OK && d3d)
        {
            D3DDEVICE_CREATION_PARAMETERS cp{};
            D3DDISPLAYMODE mode{};
            pDevice->GetCreationParameters(&cp);
            d3d->GetAdapterDisplayMode(cp.AdapterOrdinal, &mode);
            bVertexTextureSupported = SUCCEEDED(d3d->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType, mode.Format,
                D3DUSAGE_QUERY_VERTEXTEXTURE, D3DRTYPE_TEXTURE, D3DFMT_A32B32G32R32F));
            d3d->Release();
        }
    }

public:
    static void CreateResources(uint32_t width, uint32_t height)
    {
        rage::grcRenderTargetDesc desc{};
        desc.mMultisampleCount = 0;
        desc.field_0 = 1;
        desc.field_12 = 1;
        desc.mDepthRT = nullptr;
        desc.field_8 = 1;
        desc.field_10 = 1;
        desc.field_11 = 1;
        desc.field_24 = false;

        auto CreateEmptyRT = [](const char* name, int32_t a2, uint32_t w, uint32_t h, uint32_t bitsPerPixel, rage::grcRenderTargetDesc* d) -> rage::grcRenderTargetPC*
        {
            auto rt = rage::grcTextureFactory::GetInstance()->CreateRenderTarget(name, a2, w, h, bitsPerPixel, d);
            rage::grcDevice::grcResolveFlags resolveFlags{};
            rage::grcTextureFactoryPC::GetInstance()->LockRenderTarget(0, rt, nullptr);
            rage::grcTextureFactoryPC::GetInstance()->UnlockRenderTarget(0, &resolveFlags);
            return rt;
        };

        desc.mFormat = rage::GRCFMT_G16R16F;
        MotionRT = CreateEmptyRT("TemporalMotion", 3, width, height, 32, &desc);

        desc.mFormat = rage::GRCFMT_A16B16G16R16F;
        HistoryRT[0] = CreateEmptyRT("TemporalHistory0", 3, width, height, 64, &desc);
        HistoryRT[1] = CreateEmptyRT("TemporalHistory1", 3, width, height, 64, &desc);

        if (bVertexTextureSupported && bSkinnedMotionVectors)
        {
            desc.mFormat = rage::GRCFMT_A32B32G32R32F;
            BoneRT = CreateEmptyRT("TemporalBones", 3, BoneTexels, BoneRows, 128, &desc);
        }

        desc.mFormat = rage::GRCFMT_R32F;
        DepthRT = CreateEmptyRT("TemporalDepth", 3, width, height, 32, &desc);

        if (bReactiveMask)
        {
            desc.mFormat = rage::GRCFMT_R16F;
            OpaqueRT = CreateEmptyRT("TemporalOpaque", 3, width, height, 16, &desc);
            ReactiveRT = CreateEmptyRT("TemporalReactive", 3, width, height, 16, &desc);
        }

        HistoryWidth = width;
        HistoryHeight = height;
        HistoryFrame = 0;
    }

    static void ReleaseResources()
    {
        auto destroy = [](rage::grcRenderTargetPC*& rt)
        {
            if (rt)
            {
                rt->Destroy();
                rt = nullptr;
            }
        };
        destroy(MotionRT);
        destroy(HistoryRT[0]);
        destroy(HistoryRT[1]);
        destroy(BoneRT);
        destroy(DepthRT);
        destroy(OpaqueRT);
        destroy(ReactiveRT);
        HistoryFrame = 0;
        UpscalerFrame = 0;
        OpaqueFrame = 0;
    }

    static bool ResourcesReady()
    {
        return MotionRT && MotionRT->mD3DTexture && HistoryRT[0] && HistoryRT[0]->mD3DTexture && HistoryRT[1] && HistoryRT[1]->mD3DTexture &&
            DepthRT && DepthRT->mD3DTexture && ShadersLoaded();
    }

    // ---------------------------------------------------------------------------------------------
    // Jitter, main thread

    static inline std::atomic<uint32_t> ResolveFrame = 0;   // SceneFrame of the last resolve
    static inline uint32_t JitterCounter = 0;

    static float Halton(uint32_t index, uint32_t base)
    {
        float f = 1.0f;
        float r = 0.0f;
        while (index > 0)
        {
            f /= static_cast<float>(base);
            r += f * static_cast<float>(index % base);
            index /= base;
        }
        return r;
    }

    static bool IsJitterActive()
    {
        if (TemporalAA::GetMode() == TemporalAA::Mode::Off)
            return false;

        // Never jitter an image that is not resolved: e.g. post processing is off or failed to start
        auto frame = TemporalAA::SceneFrame;
        auto resolved = ResolveFrame.load();
        return resolved != 0 && frame - resolved < 10;
    }

    static inline rage::grcViewport* CameraViewport = nullptr;

    // The G-buffer phase of the game viewport, not of the phone camera or of front end scenes: its viewport
    // is a copy of the camera viewport
    static bool IsCameraScene()
    {
        if (!CRenderPhase::sm_pCurrent || !*CRenderPhase::sm_pCurrent)
            return false;

        auto phaseViewport = reinterpret_cast<rage::grcViewport*>(*CRenderPhase::sm_pCurrent + 176);
        if (!phaseViewport->mIsPerspective)
            return false;

        if (!CameraViewport)
            return true;

        return phaseViewport->mWidth == CameraViewport->mWidth && phaseViewport->mHeight == CameraViewport->mHeight &&
            phaseViewport->mFov == CameraViewport->mFov && phaseViewport->mNearClip == CameraViewport->mNearClip;
    }

    static inline injector::hook_back<void(__fastcall*)(void*, void*, rage::grcViewport*, float, float, float, float)> hbSetCameraPerspective;
    static void __fastcall SetCameraPerspective(void* _this, void* edx, rage::grcViewport* viewport, float fov, float aspect, float nearClip, float farClip)
    {
        CameraViewport = viewport;

        if (!viewport || !IsJitterActive() || viewport->mWidth <= 0 || viewport->mHeight <= 0)
            return hbSetCameraPerspective.fun(_this, edx, viewport, fov, aspect, nearClip, farClip);

        // NVIDIA recommends at least 16 phases for DLSS, 32 or more preferred
        auto phases = static_cast<uint32_t>(std::clamp(nJitterPhases, 2, 128));
        auto mode = TemporalAA::GetMode();
        if (mode == TemporalAA::Mode::DLAA)
            phases = std::max(phases, 32u);
        else if (mode == TemporalAA::Mode::FSR)
            phases = std::max(phases, 16u);
        auto index = (JitterCounter++ % phases) + 1;
        auto jitterX = Halton(index, 2) - 0.5f;
        auto jitterY = Halton(index, 3) - 0.5f;

        // Perspective() copies these projection offsets into P[2][0] and P[2][1]. With P[2][3] = -1 the
        // rendered content moves by -offset in NDC, so this shifts the image right/down by jitterX/jitterY pixels.
        auto shiftX = viewport->field_2D8;
        auto shiftY = viewport->field_2DC;
        viewport->field_2D8 = shiftX - 2.0f * jitterX / static_cast<float>(viewport->mWidth);
        viewport->field_2DC = shiftY + 2.0f * jitterY / static_cast<float>(viewport->mHeight);
        hbSetCameraPerspective.fun(_this, edx, viewport, fov, aspect, nearClip, farClip);
        viewport->field_2D8 = shiftX;
        viewport->field_2DC = shiftY;
    }

    // ---------------------------------------------------------------------------------------------
    // Draw list markers

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
            if (!bBuildingSceneList || !bObjectMotionVectors)
                return hook.unsafe_thiscall<int>(entity, a2, a3, a4, a5);

            Append(CTemporalDC::Type::EntityBegin, entity);
            auto result = hook.unsafe_thiscall<int>(entity, a2, a3, a4, a5);
            Append(CTemporalDC::Type::EntityEnd, entity);
            return result;
        }
    };

    // ---------------------------------------------------------------------------------------------
    // Render thread: scene camera

    static void OnSceneBegin()
    {
        using namespace TemporalAA;

        auto viewport = rage::GetCurrentViewport();
        if (!viewport)
            return;

        ++SceneFrame;

        if (CurrentCamera.Valid)
            PreviousCamera = CurrentCamera;

        FrameCamera camera;
        camera.View = TemporalMath::Matrix::From(viewport->mViewMatrix);
        camera.Projection = TemporalMath::Matrix::From(viewport->mProjectionMatrix);
        camera.ProjectionNoJitter = camera.Projection;
        camera.Width = viewport->mWidth;
        camera.Height = viewport->mHeight;
        camera.Near = viewport->mNearClip;
        camera.Far = viewport->mFarClip;
        camera.FovY = viewport->mFov * 0.017453292f;
        camera.Frame = SceneFrame;

        if (viewport->mIsPerspective)
        {
            camera.JitterNdc[0] = viewport->mProjectionMatrix[2][0] - viewport->field_2D8;
            camera.JitterNdc[1] = viewport->mProjectionMatrix[2][1] - viewport->field_2DC;
            camera.ProjectionNoJitter.m[2][0] = viewport->field_2D8;
            camera.ProjectionNoJitter.m[2][1] = viewport->field_2DC;
        }

        camera.JitterPixels[0] = -camera.JitterNdc[0] * 0.5f * static_cast<float>(camera.Width);
        camera.JitterPixels[1] = camera.JitterNdc[1] * 0.5f * static_cast<float>(camera.Height);
        camera.ViewProjectionNoJitter = camera.View * camera.ProjectionNoJitter;
        camera.Valid = viewport->mIsPerspective && camera.Width > 0 && camera.Height > 0 && camera.Near > 0.0f && camera.Far > camera.Near;
        CurrentCamera = camera;

        Upscaler::Update();
        BeginCapture();
    }

    // ---------------------------------------------------------------------------------------------
    // Render thread: moving entities

    struct DeclInfo
    {
        uint32_t Streams = 0;
        bool Skinned = false;
    };

    struct DrawRecord
    {
        IDirect3DIndexBuffer9* IndexBuffer = nullptr;
        IDirect3DVertexBuffer9* VertexBuffer = nullptr;
        UINT StartIndex = 0;
        UINT PrimitiveCount = 0;
        UINT NumVertices = 0;
        float World[16]{};
        int32_t Bones = -1;            // offset in the bone pool of the same frame
    };

    struct EntityRecord
    {
        std::vector<DrawRecord> Draws;
    };

    static constexpr uint32_t MaxStreams = 4;

    struct Capture
    {
        IDirect3DVertexDeclaration9* Decl = nullptr;
        IDirect3DVertexBuffer9* Streams[MaxStreams]{};
        UINT Offsets[MaxStreams]{};
        UINT Strides[MaxStreams]{};
        uint32_t StreamCount = 0;
        IDirect3DIndexBuffer9* Indices = nullptr;
        D3DPRIMITIVETYPE Type = D3DPT_TRIANGLELIST;
        INT BaseVertex = 0;
        UINT MinIndex = 0;
        UINT NumVertices = 0;
        UINT StartIndex = 0;
        UINT PrimitiveCount = 0;
        DWORD CullMode = D3DCULL_CCW;
        float World[16]{};
        float PrevWorld[16]{};
        float WorldViewProj[16]{};
        bool Skinned = false;
        int32_t Bones = -1;             // current bones, offset in the current bone pool
        int32_t PrevBones = -1;         // previous bones, offset in the previous bone pool
        int32_t BoneRow = -1;
    };

    static inline std::unordered_map<IDirect3DVertexDeclaration9*, DeclInfo> DeclCache;
    static inline std::unordered_map<void*, EntityRecord> EntitiesCurrent;
    static inline std::unordered_map<void*, EntityRecord> EntitiesPrevious;
    static inline std::vector<float> BonesCurrent;
    static inline std::vector<float> BonesPrevious;
    static inline std::vector<Capture> Captures;
    static inline std::vector<void*> EntityStack;
    static inline IDirect3DSurface9* SceneDepthSurface = nullptr;
    static inline bool bCapturing = false;
    static inline DWORD RenderThreadId = 0;
    static inline thread_local bool bInternalDraw = false;

    static void BeginCapture()
    {
        EndCaptureCleanup();

        bCapturing = TemporalAA::GetMode() != TemporalAA::Mode::Off && TemporalAA::CurrentCamera.Valid && ResourcesReady() && bObjectMotionVectors;
        RenderThreadId = GetCurrentThreadId();
        if (bCapturing)
            InstallDrawHook();
    }

    static void EndCaptureCleanup()
    {
        for (auto& capture : Captures)
        {
            SAFE_RELEASE(capture.Decl);
            for (auto& stream : capture.Streams)
                SAFE_RELEASE(stream);
            SAFE_RELEASE(capture.Indices);
        }
        Captures.clear();
        EntityStack.clear();
        SAFE_RELEASE(SceneDepthSurface);
    }

    static void OnEntityBegin(void* entity)
    {
        if (bCapturing)
            EntityStack.push_back(entity);
    }

    static void OnEntityEnd(void* entity)
    {
        if (bCapturing && !EntityStack.empty())
            EntityStack.pop_back();
    }

    static const DeclInfo& GetDeclInfo(IDirect3DVertexDeclaration9* decl)
    {
        auto it = DeclCache.find(decl);
        if (it != DeclCache.end())
            return it->second;

        DeclInfo info;
        D3DVERTEXELEMENT9 elements[MAXD3DDECLLENGTH]{};
        UINT count = 0;
        if (decl && decl->GetDeclaration(elements, &count) == S_OK)
        {
            for (UINT i = 0; i < count && elements[i].Stream != 0xFF; ++i)
            {
                info.Streams = std::max<uint32_t>(info.Streams, elements[i].Stream + 1u);
                if (elements[i].Usage == D3DDECLUSAGE_BLENDINDICES)
                    info.Skinned = true;
            }
        }
        return DeclCache.emplace(decl, info).first->second;
    }

    static const DrawRecord* FindPrevious(void* entity, size_t ordinal, IDirect3DIndexBuffer9* ib, IDirect3DVertexBuffer9* vb, UINT startIndex, UINT primitiveCount)
    {
        auto it = EntitiesPrevious.find(entity);
        if (it == EntitiesPrevious.end())
            return nullptr;

        auto& draws = it->second.Draws;
        auto matches = [&](const DrawRecord& d)
        {
            return d.IndexBuffer == ib && d.VertexBuffer == vb && d.StartIndex == startIndex && d.PrimitiveCount == primitiveCount;
        };

        if (ordinal < draws.size() && matches(draws[ordinal]))
            return &draws[ordinal];

        for (auto& d : draws)
            if (matches(d))
                return &d;

        return nullptr;
    }

    static void CaptureDraw(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, INT baseVertex, UINT minIndex, UINT numVertices, UINT startIndex, UINT primitiveCount)
    {
        if (type != D3DPT_TRIANGLELIST && type != D3DPT_TRIANGLESTRIP)
            return;

        auto entity = EntityStack.back();

        IDirect3DVertexDeclaration9* decl = nullptr;
        IDirect3DIndexBuffer9* indices = nullptr;
        if (device->GetVertexDeclaration(&decl) != S_OK || !decl)
            return;
        device->GetIndices(&indices);
        if (!indices)
        {
            decl->Release();
            return;
        }

        auto& declInfo = GetDeclInfo(decl);

        Capture capture;
        capture.Decl = decl;
        capture.Indices = indices;
        capture.Type = type;
        capture.BaseVertex = baseVertex;
        capture.MinIndex = minIndex;
        capture.NumVertices = numVertices;
        capture.StartIndex = startIndex;
        capture.PrimitiveCount = primitiveCount;
        capture.StreamCount = std::min(declInfo.Streams, MaxStreams);
        capture.Skinned = declInfo.Skinned;

        for (uint32_t i = 0; i < capture.StreamCount; ++i)
            device->GetStreamSource(i, &capture.Streams[i], &capture.Offsets[i], &capture.Strides[i]);

        device->GetVertexShaderConstantF(0, capture.World, 4);
        device->GetVertexShaderConstantF(8, capture.WorldViewProj, 4);
        device->GetRenderState(D3DRS_CULLMODE, &capture.CullMode);

        if (capture.Skinned)
        {
            capture.Bones = static_cast<int32_t>(BonesCurrent.size());
            BonesCurrent.resize(BonesCurrent.size() + BoneTexels * 4);
            device->GetVertexShaderConstantF(64, &BonesCurrent[capture.Bones], BoneTexels);
        }

        // Record this frame's draw for the next frame
        auto& record = EntitiesCurrent[entity];
        auto ordinal = record.Draws.size();
        DrawRecord draw;
        draw.IndexBuffer = indices;
        draw.VertexBuffer = capture.Streams[0];
        draw.StartIndex = startIndex;
        draw.PrimitiveCount = primitiveCount;
        draw.NumVertices = numVertices;
        std::memcpy(draw.World, capture.World, sizeof(draw.World));
        draw.Bones = capture.Bones;
        record.Draws.push_back(draw);

        // Only draws that moved since the previous frame need their own motion vectors, the rest is
        // covered by the camera motion reprojected from depth
        auto previous = FindPrevious(entity, ordinal, indices, capture.Streams[0], startIndex, primitiveCount);
        bool moved = false;
        if (previous)
        {
            std::memcpy(capture.PrevWorld, previous->World, sizeof(capture.PrevWorld));
            moved = std::memcmp(capture.PrevWorld, capture.World, sizeof(capture.World)) != 0;

            if (capture.Skinned && previous->Bones >= 0 && static_cast<size_t>(previous->Bones + BoneTexels * 4) <= BonesPrevious.size())
            {
                capture.PrevBones = previous->Bones;
                if (!moved)
                    moved = std::memcmp(&BonesPrevious[capture.PrevBones], &BonesCurrent[capture.Bones], BoneTexels * 4 * sizeof(float)) != 0;
            }
        }

        if (!moved)
        {
            capture.Decl = nullptr;
            capture.Indices = nullptr;
            decl->Release();
            indices->Release();
            for (auto& stream : capture.Streams)
                SAFE_RELEASE(stream);
            return;
        }

        if (!SceneDepthSurface)
            device->GetDepthStencilSurface(&SceneDepthSurface);

        Captures.push_back(capture);
    }

    // ---------------------------------------------------------------------------------------------
    // Draw hook on the real device

    static inline HRESULT(__stdcall* RealDrawIndexedPrimitive)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT) = nullptr;
    static inline void** HookedVTable = nullptr;

    static HRESULT __stdcall DrawIndexedPrimitive(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, INT baseVertex, UINT minIndex, UINT numVertices, UINT startIndex, UINT primitiveCount)
    {
        if (bCapturing && !bInternalDraw && !EntityStack.empty() && GetCurrentThreadId() == RenderThreadId)
            CaptureDraw(device, type, baseVertex, minIndex, numVertices, startIndex, primitiveCount);
        return RealDrawIndexedPrimitive(device, type, baseVertex, minIndex, numVertices, startIndex, primitiveCount);
    }

    static void InstallDrawHook()
    {
        auto device = RageDirect3DDevice9::m_pRealDevice ? *RageDirect3DDevice9::m_pRealDevice : nullptr;
        if (!device)
            return;

        auto vtable = *reinterpret_cast<void***>(device);
        constexpr auto DrawIndexedPrimitiveIndex = 82;
        if (vtable == HookedVTable || vtable[DrawIndexedPrimitiveIndex] == reinterpret_cast<void*>(&DrawIndexedPrimitive))
            return;

        RealDrawIndexedPrimitive = reinterpret_cast<decltype(RealDrawIndexedPrimitive)>(vtable[DrawIndexedPrimitiveIndex]);
        injector::WriteMemory(&vtable[DrawIndexedPrimitiveIndex], &DrawIndexedPrimitive, true);
        HookedVTable = vtable;
    }

    // ---------------------------------------------------------------------------------------------
    // Render thread: motion vectors after the G-buffer pass

    struct StateBackup
    {
        IDirect3DDevice9* device = nullptr;
        IDirect3DSurface9* rt[4]{};
        IDirect3DSurface9* ds = nullptr;
        D3DVIEWPORT9 viewport{};
        IDirect3DVertexShader9* vs = nullptr;
        IDirect3DPixelShader9* ps = nullptr;
        IDirect3DVertexDeclaration9* decl = nullptr;
        DWORD fvf = 0;
        IDirect3DVertexBuffer9* streams[MaxStreams]{};
        UINT offsets[MaxStreams]{};
        UINT strides[MaxStreams]{};
        IDirect3DIndexBuffer9* indices = nullptr;
        IDirect3DBaseTexture9* texture0 = nullptr;
        IDirect3DBaseTexture9* vertexTexture0 = nullptr;
        float vsConstants[21 * 4]{};
        float vsBones[BoneTexels * 4]{};
        float psConstants[7 * 4]{};

        static constexpr D3DRENDERSTATETYPE RenderStates[] =
        {
            D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_ZFUNC, D3DRS_ALPHABLENDENABLE, D3DRS_ALPHATESTENABLE,
            D3DRS_STENCILENABLE, D3DRS_TWOSIDEDSTENCILMODE, D3DRS_CULLMODE, D3DRS_COLORWRITEENABLE,
            D3DRS_SCISSORTESTENABLE, D3DRS_SRGBWRITEENABLE, D3DRS_FILLMODE, D3DRS_CLIPPLANEENABLE,
            D3DRS_POINTSIZE, D3DRS_POINTSCALEENABLE, D3DRS_POINTSPRITEENABLE, D3DRS_DEPTHBIAS,
            D3DRS_SLOPESCALEDEPTHBIAS, D3DRS_FOGENABLE, D3DRS_MULTISAMPLEANTIALIAS, D3DRS_SEPARATEALPHABLENDENABLE,
        };
        DWORD renderStates[std::size(RenderStates)]{};

        static constexpr D3DSAMPLERSTATETYPE SamplerStates[] =
        {
            D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER, D3DSAMP_MIPFILTER, D3DSAMP_SRGBTEXTURE,
        };
        DWORD samplerStates0[std::size(SamplerStates)]{};
        DWORD vertexSamplerStates0[std::size(SamplerStates)]{};

        explicit StateBackup(IDirect3DDevice9* dev) : device(dev)
        {
            for (DWORD i = 0; i < 4; ++i)
                device->GetRenderTarget(i, &rt[i]);
            device->GetDepthStencilSurface(&ds);
            device->GetViewport(&viewport);
            device->GetVertexShader(&vs);
            device->GetPixelShader(&ps);
            device->GetVertexDeclaration(&decl);
            device->GetFVF(&fvf);
            for (DWORD i = 0; i < MaxStreams; ++i)
                device->GetStreamSource(i, &streams[i], &offsets[i], &strides[i]);
            device->GetIndices(&indices);
            device->GetTexture(0, &texture0);
            device->GetTexture(D3DVERTEXTEXTURESAMPLER0, &vertexTexture0);
            device->GetVertexShaderConstantF(0, vsConstants, 21);
            device->GetVertexShaderConstantF(64, vsBones, BoneTexels);
            device->GetPixelShaderConstantF(0, psConstants, 7);
            for (size_t i = 0; i < std::size(RenderStates); ++i)
                device->GetRenderState(RenderStates[i], &renderStates[i]);
            for (size_t i = 0; i < std::size(SamplerStates); ++i)
            {
                device->GetSamplerState(0, SamplerStates[i], &samplerStates0[i]);
                device->GetSamplerState(D3DVERTEXTEXTURESAMPLER0, SamplerStates[i], &vertexSamplerStates0[i]);
            }
        }

        ~StateBackup()
        {
            for (DWORD i = 0; i < 4; ++i)
                device->SetRenderTarget(i, rt[i]);
            device->SetDepthStencilSurface(ds);
            device->SetViewport(&viewport);
            device->SetVertexShader(vs);
            device->SetPixelShader(ps);
            if (decl)
                device->SetVertexDeclaration(decl);
            else
                device->SetFVF(fvf);
            for (DWORD i = 0; i < MaxStreams; ++i)
                device->SetStreamSource(i, streams[i], offsets[i], strides[i]);
            device->SetIndices(indices);
            device->SetTexture(0, texture0);
            device->SetTexture(D3DVERTEXTEXTURESAMPLER0, vertexTexture0);
            device->SetVertexShaderConstantF(0, vsConstants, 21);
            device->SetVertexShaderConstantF(64, vsBones, BoneTexels);
            device->SetPixelShaderConstantF(0, psConstants, 7);
            for (size_t i = 0; i < std::size(RenderStates); ++i)
                device->SetRenderState(RenderStates[i], renderStates[i]);
            for (size_t i = 0; i < std::size(SamplerStates); ++i)
            {
                device->SetSamplerState(0, SamplerStates[i], samplerStates0[i]);
                device->SetSamplerState(D3DVERTEXTEXTURESAMPLER0, SamplerStates[i], vertexSamplerStates0[i]);
            }

            for (auto& s : rt)
                SAFE_RELEASE(s);
            SAFE_RELEASE(ds);
            SAFE_RELEASE(vs);
            SAFE_RELEASE(ps);
            SAFE_RELEASE(decl);
            for (auto& s : streams)
                SAFE_RELEASE(s);
            SAFE_RELEASE(indices);
            SAFE_RELEASE(texture0);
            SAFE_RELEASE(vertexTexture0);
        }
    };

    struct ScreenVertex
    {
        float x, y, z, rhw;
        float u, v;
    };

    static void DrawFullscreen(IDirect3DDevice9* device, float width, float height)
    {
        ScreenVertex vertices[4] =
        {
            { -0.5f,         -0.5f,          0.0f, 1.0f, 0.0f, 0.0f },
            { -0.5f,          height - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
            { width - 0.5f,  -0.5f,          0.0f, 1.0f, 1.0f, 0.0f },
            { width - 0.5f,   height - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f },
        };
        device->SetVertexShader(nullptr);
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
        device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(ScreenVertex));
    }

    // Render states of a plain fullscreen pass, StateBackup restores them
    static void SetFullscreenStates(IDirect3DDevice9* device, DWORD colorWrite = 0x0F)
    {
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, colorWrite);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    }

    static void OnSceneEnd()
    {
        using namespace TemporalAA;

        bool capturing = bCapturing;
        bCapturing = false;

        auto device = RageDirect3DDevice9::m_pRealDevice ? *RageDirect3DDevice9::m_pRealDevice : nullptr;
        if (device && GetMode() != Mode::Off && CurrentCamera.Valid && ResourcesReady())
        {
            bInternalDraw = true;
            {
                StateBackup backup(device);
                RenderCameraMotion(device);
                if (capturing && !Captures.empty())
                    RenderObjectMotion(device);
            }
            bInternalDraw = false;
            MotionFrame = SceneFrame;
        }

        EndCaptureCleanup();

        // Keep the draws of this frame for the next one
        std::swap(EntitiesPrevious, EntitiesCurrent);
        EntitiesCurrent.clear();
        std::swap(BonesPrevious, BonesCurrent);
        BonesCurrent.clear();
    }

    static inline uint32_t MotionFrame = 0;

    static void RenderCameraMotion(IDirect3DDevice9* device)
    {
        using namespace TemporalAA;

        auto depthRT = rage::grcTextureFactoryPC::GetRTByName("_DEFERRED_GBUFFER_3_");
        if (!depthRT || !depthRT->mD3DTexture)
            return;

        IDirect3DSurface9* motionSurface = nullptr;
        MotionRT->mD3DTexture->GetSurfaceLevel(0, &motionSurface);
        if (!motionSurface)
            return;

        auto& camera = CurrentCamera;
        auto& previous = PreviousCamera.Valid ? PreviousCamera : CurrentCamera;

        // current view space -> previous clip space, without jitter
        auto reproject = camera.View.Inverse() * previous.View * previous.ProjectionNoJitter;

        float constants[6 * 4]{};
        constants[0] = static_cast<float>(camera.Projection.m[0][0]);
        constants[1] = static_cast<float>(camera.Projection.m[1][1]);
        constants[2] = static_cast<float>(camera.Projection.m[2][0]);
        constants[3] = static_cast<float>(camera.Projection.m[2][1]);
        constants[4] = camera.JitterNdc[0];
        constants[5] = camera.JitterNdc[1];
        constants[6] = camera.Near;
        constants[7] = std::log2(camera.Far / camera.Near);
        reproject.To(&constants[8]);

        device->SetRenderTarget(0, motionSurface);
        for (DWORD i = 1; i < 4; ++i)
            device->SetRenderTarget(i, nullptr);
        device->SetDepthStencilSurface(nullptr);
        SetFullscreenStates(device, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN);

        device->SetTexture(0, depthRT->mD3DTexture);
        device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);

        device->SetPixelShader(CameraMotionPS);
        device->SetPixelShaderConstantF(0, constants, 6);
        DrawFullscreen(device, static_cast<float>(MotionRT->mWidth), static_cast<float>(MotionRT->mHeight));
        motionSurface->Release();

        // The same depth, as standard [0, 1] depth, for the resolve, DLAA and FSR
        IDirect3DSurface9* depthSurface = nullptr;
        DepthRT->mD3DTexture->GetSurfaceLevel(0, &depthSurface);
        if (depthSurface)
        {
            float depthConstants[4] = { camera.Near, std::log2(camera.Far / camera.Near), camera.Far, 0.0f };
            device->SetRenderTarget(0, depthSurface);
            device->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0F);
            device->SetPixelShader(DepthPS);
            device->SetPixelShaderConstantF(0, depthConstants, 1);
            DrawFullscreen(device, static_cast<float>(DepthRT->mWidth), static_cast<float>(DepthRT->mHeight));
            depthSurface->Release();
        }

        device->SetTexture(0, nullptr);
    }

    static void UploadPreviousBones(IDirect3DDevice9* device)
    {
        if (!BoneRT || !BoneRT->mD3DTexture || !bVertexTextureSupported)
            return;

        struct BoneVertex
        {
            float position[4];
            float value[4];
        };

        static std::vector<BoneVertex> vertices;
        vertices.clear();

        int32_t row = 0;
        for (auto& capture : Captures)
        {
            if (!capture.Skinned || capture.PrevBones < 0)
                continue;
            if (row >= BoneRows)
                break;

            capture.BoneRow = row;
            auto y = 1.0f - 2.0f * static_cast<float>(row) / static_cast<float>(BoneRows);
            for (int32_t i = 0; i < BoneTexels; ++i)
            {
                // D3D9 maps pixel centers to integer screen coordinates
                BoneVertex v;
                v.position[0] = 2.0f * static_cast<float>(i) / static_cast<float>(BoneTexels) - 1.0f;
                v.position[1] = y;
                v.position[2] = 0.0f;
                v.position[3] = 1.0f;
                std::memcpy(v.value, &BonesPrevious[capture.PrevBones + i * 4], sizeof(v.value));
                vertices.push_back(v);
            }
            ++row;
        }

        if (vertices.empty())
            return;

        IDirect3DSurface9* boneSurface = nullptr;
        BoneRT->mD3DTexture->GetSurfaceLevel(0, &boneSurface);
        if (!boneSurface)
            return;

        device->SetRenderTarget(0, boneSurface);
        device->SetDepthStencilSurface(nullptr);
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0F);
        device->SetRenderState(D3DRS_POINTSIZE, 0x3F800000);
        device->SetRenderState(D3DRS_POINTSCALEENABLE, FALSE);
        device->SetRenderState(D3DRS_POINTSPRITEENABLE, FALSE);
        device->SetVertexDeclaration(BoneWriteDecl);
        device->SetVertexShader(BoneWriteVS);
        device->SetPixelShader(BoneWritePS);
        device->DrawPrimitiveUP(D3DPT_POINTLIST, static_cast<UINT>(vertices.size()), vertices.data(), sizeof(BoneVertex));

        boneSurface->Release();
    }

    static void RenderObjectMotion(IDirect3DDevice9* device)
    {
        using namespace TemporalAA;

        if (!SceneDepthSurface)
            return;

        if (bSkinnedMotionVectors)
            UploadPreviousBones(device);

        IDirect3DSurface9* motionSurface = nullptr;
        MotionRT->mD3DTexture->GetSurfaceLevel(0, &motionSurface);
        if (!motionSurface)
            return;

        auto& camera = CurrentCamera;
        auto& previous = PreviousCamera.Valid ? PreviousCamera : CurrentCamera;

        float viewProjections[8 * 4];
        camera.ViewProjectionNoJitter.To(&viewProjections[0]);
        previous.ViewProjectionNoJitter.To(&viewProjections[16]);

        float depthParams[4] =
        {
            1.0f / camera.Near,
            1.0f / std::log2(camera.Far / camera.Near),
            fObjectDepthBias,
            0.0f,
        };

        device->SetRenderTarget(0, motionSurface);
        device->SetDepthStencilSurface(SceneDepthSurface);
        device->SetRenderState(D3DRS_ZENABLE, TRUE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN);
        device->SetRenderState(D3DRS_DEPTHBIAS, 0);
        device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);

        device->SetPixelShader(VelocityPS);
        device->SetPixelShaderConstantF(0, depthParams, 1);
        device->SetVertexShaderConstantF(12, viewProjections, 8);

        if (BoneRT && BoneRT->mD3DTexture)
        {
            device->SetTexture(D3DVERTEXTEXTURESAMPLER0, BoneRT->mD3DTexture);
            device->SetSamplerState(D3DVERTEXTEXTURESAMPLER0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            device->SetSamplerState(D3DVERTEXTEXTURESAMPLER0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            device->SetSamplerState(D3DVERTEXTEXTURESAMPLER0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            device->SetSamplerState(D3DVERTEXTEXTURESAMPLER0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            device->SetSamplerState(D3DVERTEXTEXTURESAMPLER0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        }

        for (auto& capture : Captures)
        {
            device->SetVertexDeclaration(capture.Decl);
            for (uint32_t i = 0; i < capture.StreamCount; ++i)
                device->SetStreamSource(i, capture.Streams[i], capture.Offsets[i], capture.Strides[i]);
            device->SetIndices(capture.Indices);
            device->SetRenderState(D3DRS_CULLMODE, capture.CullMode);

            device->SetVertexShaderConstantF(0, capture.World, 4);
            device->SetVertexShaderConstantF(4, capture.PrevWorld, 4);
            device->SetVertexShaderConstantF(8, capture.WorldViewProj, 4);

            if (capture.Skinned)
            {
                float boneParams[4] =
                {
                    (static_cast<float>(std::max(capture.BoneRow, 0)) + 0.5f) / static_cast<float>(BoneRows),
                    1.0f / static_cast<float>(BoneTexels),
                    capture.BoneRow >= 0 ? 1.0f : 0.0f,
                    0.0f,
                };
                device->SetVertexShaderConstantF(20, boneParams, 1);
                device->SetVertexShaderConstantF(64, &BonesCurrent[capture.Bones], BoneTexels);
                device->SetVertexShader(VelocitySkinnedVS);
            }
            else
            {
                device->SetVertexShader(VelocityRigidVS);
            }

            device->DrawIndexedPrimitive(capture.Type, capture.BaseVertex, capture.MinIndex, capture.NumVertices, capture.StartIndex, capture.PrimitiveCount);
        }

        device->SetTexture(D3DVERTEXTEXTURESAMPLER0, nullptr);
        motionSurface->Release();
    }

    // ---------------------------------------------------------------------------------------------
    // Render thread, right after the fog pass of the scene

    static void CaptureOpaque()
    {
        using namespace TemporalAA;

        // Once per frame, for the scene the motion vectors were rendered for
        if (!bReactiveMask || GetMode() == Mode::Off || OpaqueFrame == SceneFrame || MotionFrame != SceneFrame)
            return;
        if (!OpaqueRT || !OpaqueRT->mD3DTexture || !OpaqueLumaPS)
            return;

        auto device = RageDirect3DDevice9::m_pRealDevice ? *RageDirect3DDevice9::m_pRealDevice : nullptr;
        if (!device)
            return;

        // The fog was drawn into the scene target
        IDirect3DSurface9* sceneSurface = nullptr;
        IDirect3DTexture9* sceneTexture = nullptr;
        IDirect3DSurface9* opaqueSurface = nullptr;
        D3DSURFACE_DESC sceneDesc{};
        device->GetRenderTarget(0, &sceneSurface);
        if (sceneSurface && SUCCEEDED(sceneSurface->GetDesc(&sceneDesc)) &&
            sceneDesc.Width == static_cast<UINT>(OpaqueRT->mWidth) && sceneDesc.Height == static_cast<UINT>(OpaqueRT->mHeight))
        {
            sceneSurface->GetContainer(__uuidof(IDirect3DTexture9), reinterpret_cast<void**>(&sceneTexture));
        }
        OpaqueRT->mD3DTexture->GetSurfaceLevel(0, &opaqueSurface);

        if (sceneTexture && opaqueSurface)
        {
            bInternalDraw = true;
            {
                StateBackup backup(device);
                device->SetRenderTarget(0, opaqueSurface);
                for (DWORD i = 1; i < 4; ++i)
                    device->SetRenderTarget(i, nullptr);
                device->SetDepthStencilSurface(nullptr);
                SetFullscreenStates(device);

                device->SetTexture(0, sceneTexture);
                device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
                device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
                device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
                device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);

                device->SetPixelShader(OpaqueLumaPS);
                DrawFullscreen(device, static_cast<float>(OpaqueRT->mWidth), static_cast<float>(OpaqueRT->mHeight));
                device->SetTexture(0, nullptr);
            }
            bInternalDraw = false;
            OpaqueFrame = SceneFrame;
        }

        SAFE_RELEASE(opaqueSurface);
        SAFE_RELEASE(sceneTexture);
        SAFE_RELEASE(sceneSurface);
    }

    static bool IsOpaqueReady()
    {
        return bReactiveMask && OpaqueRT && OpaqueRT->mD3DTexture && OpaqueFrame == TemporalAA::SceneFrame;
    }

    // Reactive mask for FSR, on the device of post processing
    static IDirect3DTexture9* RenderReactive(IDirect3DDevice9* device, IDirect3DTexture9* scene)
    {
        if (!IsOpaqueReady() || !ReactiveRT || !ReactiveRT->mD3DTexture || !ReactivePS)
            return nullptr;

        IDirect3DSurface9* reactiveSurface = nullptr;
        ReactiveRT->mD3DTexture->GetSurfaceLevel(0, &reactiveSurface);
        if (!reactiveSurface)
            return nullptr;

        bInternalDraw = true;
        {
            StateBackup backup(device);

            // StateBackup keeps sampler 0, the mask also uses 1
            IDirect3DBaseTexture9* oldTextures[2]{};
            DWORD samplerStates[2][6]{};
            constexpr D3DSAMPLERSTATETYPE states[6] = { D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER, D3DSAMP_MIPFILTER, D3DSAMP_SRGBTEXTURE };
            for (DWORD s = 0; s < 2; ++s)
            {
                device->GetTexture(s, &oldTextures[s]);
                for (DWORD i = 0; i < 6; ++i)
                    device->GetSamplerState(s, states[i], &samplerStates[s][i]);
            }

            IDirect3DBaseTexture9* textures[2] = { scene, OpaqueRT->mD3DTexture };
            for (DWORD s = 0; s < 2; ++s)
            {
                device->SetTexture(s, textures[s]);
                device->SetSamplerState(s, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                device->SetSamplerState(s, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                device->SetSamplerState(s, D3DSAMP_MINFILTER, D3DTEXF_POINT);
                device->SetSamplerState(s, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
                device->SetSamplerState(s, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
                device->SetSamplerState(s, D3DSAMP_SRGBTEXTURE, FALSE);
            }

            float constants[4] = { fReactiveScale, fReactiveMax, 0.0f, 0.0f };
            device->SetRenderTarget(0, reactiveSurface);
            for (DWORD i = 1; i < 4; ++i)
                device->SetRenderTarget(i, nullptr);
            device->SetDepthStencilSurface(nullptr);
            SetFullscreenStates(device);
            device->SetPixelShader(ReactivePS);
            device->SetPixelShaderConstantF(0, constants, 1);
            DrawFullscreen(device, static_cast<float>(ReactiveRT->mWidth), static_cast<float>(ReactiveRT->mHeight));

            for (DWORD s = 0; s < 2; ++s)
            {
                device->SetTexture(s, oldTextures[s]);
                for (DWORD i = 0; i < 6; ++i)
                    device->SetSamplerState(s, states[i], samplerStates[s][i]);
                SAFE_RELEASE(oldTextures[s]);
            }
        }
        bInternalDraw = false;
        reactiveSurface->Release();
        return ReactiveRT->mD3DTexture;
    }

    // ---------------------------------------------------------------------------------------------
    // DLAA and FSR

    static inline uint32_t UpscalerFrame = 0;             // SceneFrame of the last upscaled frame
    static inline LARGE_INTEGER UpscalerTime{};

    static bool Upscale(IDirect3DDevice9* device, Upscaler::Backend backend, IDirect3DTexture9* scene, IDirect3DTexture9* output)
    {
        using namespace TemporalAA;

        LARGE_INTEGER now, frequency;
        QueryPerformanceCounter(&now);
        QueryPerformanceFrequency(&frequency);
        auto frameTime = UpscalerTime.QuadPart ? static_cast<float>(static_cast<double>(now.QuadPart - UpscalerTime.QuadPart) * 1000.0 / static_cast<double>(frequency.QuadPart)) : 16.6f;
        UpscalerTime = now;

        Upscaler::Frame frame;
        frame.Color = scene;
        frame.Depth = DepthRT->mD3DTexture;
        frame.Motion = MotionRT->mD3DTexture;
        // NVIDIA advises against a mask for DLSS
        frame.Reactive = backend == Upscaler::Backend::FSR ? RenderReactive(device, scene) : nullptr;
        frame.Output = output;
        frame.Width = static_cast<uint32_t>(HistoryWidth);
        frame.Height = static_cast<uint32_t>(HistoryHeight);
        frame.JitterX = CurrentCamera.JitterPixels[0];
        frame.JitterY = CurrentCamera.JitterPixels[1];
        frame.CameraNear = CurrentCamera.Near;
        frame.CameraFar = CurrentCamera.Far;
        frame.CameraFovY = CurrentCamera.FovY;
        frame.FrameTimeMs = std::clamp(frameTime, 1.0f, 200.0f);
        frame.Sharpness = backend == Upscaler::Backend::FSR ? fFSRSharpness : 0.0f;
        frame.DLSSPreset = static_cast<uint32_t>(nDLSSPreset);
        frame.Reset = !(UpscalerFrame != 0 && UpscalerFrame + 1 == SceneFrame && PreviousCamera.Valid && !IsCameraCut());

        if (!Upscaler::Evaluate(backend, frame))
            return false;

        UpscalerFrame = SceneFrame;
        return true;
    }

public:
    // ---------------------------------------------------------------------------------------------
    // Resolve, called by PostFX on the HDR scene before any other post processing. Writes the result
    // into output (texture and its surface) and returns true, or leaves it untouched.

    static bool Resolve(IDirect3DDevice9* device, IDirect3DTexture9* scene, IDirect3DTexture9* outputTexture, IDirect3DSurface9* output)
    {
        using namespace TemporalAA;

        if (!ResourcesReady() || !scene || !output || !CurrentCamera.Valid || MotionFrame != SceneFrame)
            return false;

        auto mode = GetMode();
        if (mode == Mode::DLAA || mode == Mode::FSR)
        {
            auto backend = mode == Mode::DLAA ? Upscaler::Backend::DLSS : Upscaler::Backend::FSR;
            if (outputTexture && Upscale(device, backend, scene, outputTexture))
            {
                // The TAA history did not follow these frames
                HistoryFrame = 0;
                ResolveFrame = SceneFrame;
                return true;
            }
        }
        UpscalerFrame = 0;

        auto previousIndex = HistoryIndex;
        auto currentIndex = HistoryIndex ^ 1u;

        IDirect3DSurface9* historySurface = nullptr;
        HistoryRT[currentIndex]->mD3DTexture->GetSurfaceLevel(0, &historySurface);
        if (!historySurface)
            return false;

        // A resolution change recreates the targets, which resets HistoryFrame
        bool historyValid = HistoryFrame != 0 && HistoryFrame + 1 == SceneFrame && PreviousCamera.Valid && !IsCameraCut();

        auto width = static_cast<float>(HistoryWidth);
        auto height = static_cast<float>(HistoryHeight);
        auto reactive = IsOpaqueReady();
        float constants[4 * 4] =
        {
            1.0f / width, 1.0f / height, width, height,
            CurrentCamera.JitterPixels[0], CurrentCamera.JitterPixels[1], historyValid ? 1.0f : 0.0f, fVarianceClipGamma,
            fCurrentWeightMin, fCurrentWeightMax, fCurrentWeightPerPixel, fLumaWeight,
            fReactiveScale, fReactiveMax, reactive ? 1.0f : 0.0f, 0.0f,
        };

        bInternalDraw = true;
        {
            // Draws with its own quad: it runs inside a draw of the game's post processing, whose vertex shader
            // depends on the pass
            StateBackup backup(device);

            // StateBackup keeps sampler 0, the resolve also uses 1-4
            constexpr DWORD Samplers = 5;
            IDirect3DBaseTexture9* oldTextures[Samplers]{};
            DWORD samplerStates[Samplers][6]{};
            constexpr D3DSAMPLERSTATETYPE states[6] = { D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER, D3DSAMP_MIPFILTER, D3DSAMP_SRGBTEXTURE };
            for (DWORD s = 0; s < Samplers; ++s)
            {
                device->GetTexture(s, &oldTextures[s]);
                for (DWORD i = 0; i < 6; ++i)
                    device->GetSamplerState(s, states[i], &samplerStates[s][i]);
            }

            auto setSampler = [&](DWORD s, IDirect3DBaseTexture9* texture, D3DTEXTUREFILTERTYPE filter)
            {
                device->SetTexture(s, texture);
                device->SetSamplerState(s, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                device->SetSamplerState(s, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                device->SetSamplerState(s, D3DSAMP_MINFILTER, filter);
                device->SetSamplerState(s, D3DSAMP_MAGFILTER, filter);
                device->SetSamplerState(s, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
                device->SetSamplerState(s, D3DSAMP_SRGBTEXTURE, FALSE);
            };

            setSampler(0, scene, D3DTEXF_POINT);
            setSampler(1, HistoryRT[previousIndex]->mD3DTexture, D3DTEXF_LINEAR);
            setSampler(2, MotionRT->mD3DTexture, D3DTEXF_POINT);
            setSampler(3, DepthRT->mD3DTexture, D3DTEXF_POINT);
            setSampler(4, reactive ? OpaqueRT->mD3DTexture : nullptr, D3DTEXF_POINT);

            device->SetRenderTarget(0, output);
            device->SetRenderTarget(1, historySurface);
            device->SetRenderTarget(2, nullptr);
            device->SetRenderTarget(3, nullptr);
            device->SetDepthStencilSurface(nullptr);
            SetFullscreenStates(device);
            device->SetPixelShader(ResolvePS);
            device->SetPixelShaderConstantF(0, constants, 4);
            DrawFullscreen(device, width, height);

            for (DWORD s = 0; s < Samplers; ++s)
            {
                device->SetTexture(s, oldTextures[s]);
                for (DWORD i = 0; i < 6; ++i)
                    device->SetSamplerState(s, states[i], samplerStates[s][i]);
                SAFE_RELEASE(oldTextures[s]);
            }
        }
        bInternalDraw = false;
        historySurface->Release();

        HistoryIndex = currentIndex;
        HistoryFrame = SceneFrame;
        ResolveFrame = SceneFrame;
        return true;
    }

    static bool IsCameraCut()
    {
        using namespace TemporalAA;

        auto currentInverse = CurrentCamera.View.Inverse();
        auto previousInverse = PreviousCamera.View.Inverse();
        auto dx = currentInverse.m[3][0] - previousInverse.m[3][0];
        auto dy = currentInverse.m[3][1] - previousInverse.m[3][1];
        auto dz = currentInverse.m[3][2] - previousInverse.m[3][2];
        if (dx * dx + dy * dy + dz * dz > fCameraCutDistance * fCameraCutDistance)
            return true;

        // forward vectors more than ~60 degrees apart
        auto dot = currentInverse.m[2][0] * previousInverse.m[2][0] + currentInverse.m[2][1] * previousInverse.m[2][1] + currentInverse.m[2][2] * previousInverse.m[2][2];
        return dot < 0.5;
    }

    Temporal()
    {
        FusionFix::onInitEventAsync() += []()
        {
            CIniReader iniReader("");
            nJitterPhases = std::clamp(iniReader.ReadInteger("TEMPORAL", "JitterPhases", 8), 2, 128);
            fCurrentWeightMin = std::clamp(iniReader.ReadFloat("TEMPORAL", "CurrentFrameWeightMin", 0.08f), 0.01f, 1.0f);
            fCurrentWeightMax = std::clamp(iniReader.ReadFloat("TEMPORAL", "CurrentFrameWeightMax", 0.2f), fCurrentWeightMin, 1.0f);
            fCurrentWeightPerPixel = std::max(iniReader.ReadFloat("TEMPORAL", "CurrentFrameWeightPerPixelOfMotion", 0.1f), 0.0f);
            fVarianceClipGamma = std::clamp(iniReader.ReadFloat("TEMPORAL", "VarianceClipGamma", 1.25f), 0.5f, 4.0f);
            fLumaWeight = std::clamp(iniReader.ReadFloat("TEMPORAL", "LumaWeight", 1.0f), 0.0f, 16.0f);
            fObjectDepthBias = std::clamp(iniReader.ReadFloat("TEMPORAL", "ObjectDepthBias", 0.00002f), 0.0f, 0.01f);
            bObjectMotionVectors = iniReader.ReadInteger("TEMPORAL", "ObjectMotionVectors", 1) != 0;
            bSkinnedMotionVectors = iniReader.ReadInteger("TEMPORAL", "SkinnedMotionVectors", 1) != 0;
            fCameraCutDistance = std::max(iniReader.ReadFloat("TEMPORAL", "CameraCutDistance", 25.0f), 1.0f);
            nDLSSPreset = std::clamp(iniReader.ReadInteger("TEMPORAL", "DLSSPreset", 0), 0, 15);
            fFSRSharpness = std::clamp(iniReader.ReadFloat("TEMPORAL", "FSRSharpness", 0.0f), 0.0f, 1.0f);
            bReactiveMask = iniReader.ReadInteger("TEMPORAL", "ReactiveMask", 1) != 0;
            fReactiveScale = std::clamp(iniReader.ReadFloat("TEMPORAL", "ReactiveMaskScale", 2.0f), 0.0f, 64.0f);
            fReactiveMax = std::clamp(iniReader.ReadFloat("TEMPORAL", "ReactiveMaskMaximum", 0.75f), 0.0f, 1.0f);

            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&Resolve, &hm);

            // Main camera perspective, CCamFinal
            auto pattern = hook::pattern("F3 0F 11 04 24 53 E8 ? ? ? ? 5F 5E 5B 8B E5");
            if (!pattern.empty())
                hbSetCameraPerspective.fun = injector::MakeCALL(pattern.get_first(6), SetCameraPerspective, true).get();
            else
            {
                pattern = hook::pattern("B9 ? ? ? ? D9 47 54 D9 5C 24 08 D9 44 24 ? D9 5C 24 04 D9 47 50 D9 1C 24 56 E8");
                if (!pattern.empty())
                    hbSetCameraPerspective.fun = injector::MakeCALL(pattern.get_first(27), SetCameraPerspective, true).get();
            }

            // Entity AddToDrawList implementations (vtable slot 35): CPed, CVehicle, CObject, CCutsceneObject, CPhysical
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

            FusionFixSettings.SetAvailability("PREF_ANTIALIASING", [](int32_t value) -> bool
            {
                switch (value)
                {
                case FusionFixSettings.AntialiasingText.eSMAA:
                    // hangs the game on D3D9on12
                    return CSettings::GetRunningGraphicsAPI() != 2;
                case FusionFixSettings.AntialiasingText.eTAA:
                    // resolved by Fusion Fix post processing
                    return GetD3DX9_43DLL() != nullptr;
                case FusionFixSettings.AntialiasingText.eDLAA:
                    return Upscaler::IsAvailable(Upscaler::Backend::DLSS);
                case FusionFixSettings.AntialiasingText.eFSR:
                    return Upscaler::IsAvailable(Upscaler::Backend::FSR);
                default:
                    return true;
                }
            });
            FusionFixSettings.RefreshAvailability("PREF_ANTIALIASING");

            // DLAA and FSR become available once the helper has started, a saved choice comes back then. The first
            // call comes after the device was created, when it's known whether SMAA can run.
            static auto refreshAvailability = []()
            {
                static uint32_t generation = UINT32_MAX;
                if (generation != Upscaler::Generation())
                {
                    generation = Upscaler::Generation();
                    FusionFixSettings.RefreshAvailability("PREF_ANTIALIASING");
                }
            };
            FusionFix::onGameProcessEvent() += []() { refreshAvailability(); };
            FusionFix::onMenuDrawingEvent() += []() { refreshAvailability(); };

            FusionFix::onShutdownEvent() += []()
            {
                Upscaler::Shutdown();
            };

            CRenderPhaseDeferredLighting_SceneToGBuffer::OnBuildRenderList() += []()
            {
                bBuildingSceneList = IsCameraScene();
                if (bBuildingSceneList)
                    Append(CTemporalDC::Type::SceneBegin);
            };

            CRenderPhaseDeferredLighting_SceneToGBuffer::OnAfterBuildRenderList() += []()
            {
                if (bBuildingSceneList)
                    Append(CTemporalDC::Type::SceneEnd);
                bBuildingSceneList = false;
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
        static auto aa = FusionFixSettings.GetRef("PREF_ANTIALIASING");
        if (!aa)
            return Mode::Off;
        switch (aa->get())
        {
        case FusionFixSettings.AntialiasingText.eTAA:
            return Mode::TAA;
        case FusionFixSettings.AntialiasingText.eDLAA:
            return Upscaler::IsAvailable(Upscaler::Backend::DLSS) ? Mode::DLAA : Mode::TAA;
        case FusionFixSettings.AntialiasingText.eFSR:
            return Upscaler::IsAvailable(Upscaler::Backend::FSR) ? Mode::FSR : Mode::TAA;
        default:
            return Mode::Off;
        }
    }

    bool IsMotionVectorsReady()
    {
        return Temporal::MotionFrame == SceneFrame && Temporal::MotionRT;
    }

    IDirect3DTexture9* GetMotionVectors()
    {
        return Temporal::MotionRT ? Temporal::MotionRT->mD3DTexture : nullptr;
    }

    bool HistoryValid()
    {
        return Temporal::HistoryFrame != 0;
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

    bool Resolve(IDirect3DDevice9* device, IDirect3DTexture9* scene, IDirect3DTexture9* outputTexture, IDirect3DSurface9* output)
    {
        return Temporal::Resolve(device, scene, outputTexture, output);
    }

    // The scene of this frame went through the resolve, DLAA or FSR
    bool IsSceneResolved()
    {
        return SceneFrame != 0 && Temporal::ResolveFrame == SceneFrame;
    }

    // Called by PostFX right after the fog pass of the scene
    void OnFogDrawn()
    {
        Temporal::CaptureOpaque();
    }
}
