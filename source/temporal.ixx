module;

#include <common.hxx>
#include <aa/AntiAliasing.hpp>
#include "includes/TemporalRenderStates.hpp"

export module temporal;

import common;
import comvars;
import d3dx9_43;
import settings;

#define IDR_TEMPORAL_VS_RIGID           3001
#define IDR_TEMPORAL_VS_SKINNED         3002
#define IDR_TEMPORAL_PS_VELOCITY        3003
#define IDR_TEMPORAL_VS_BONEWRITE       3004
#define IDR_TEMPORAL_PS_BONEWRITE       3005

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p)=NULL; } }
#endif

// Temporal anti-aliasing for the scene: TAA, DLAA and FSR of the shared module (FusionFixUtils, aa/AntiAliasing.hpp),
// with what is particular to this game here.
//
// - The projection of the main camera viewport is offset by the module's sub-pixel Halton sequence every frame. Render
//   phases copy the whole grcViewport, so the jitter reaches every scene pass, and the render thread reads it back from
//   the projection it actually rendered with (the module's BeginFrame).
// - Motion vectors: the module reprojects the camera's motion from the depth (_DEFERRED_GBUFFER_3_, logarithmic).
//   Moving entities (peds, vehicles, objects) are wrapped with draw list markers while the G-buffer render list is
//   built; their draws are captured on the render thread and drawn again after the G-buffer pass with the world matrix
//   and bones of the previous frame, into the module's motion vectors.
// - Motion vectors and the depth used by the resolve, DLAA and FSR are taken right after the G-buffer pass, before glass
//   and other transparent geometry is drawn: through a window they describe what is behind it, which is also what the
//   color mostly shows. Depth taken later would describe the glass, and reprojecting the scene behind it with that depth
//   smears it.
// - Particles, glass and other transparent geometry have no motion vectors of their own. The luminance of the scene is
//   copied right after the fog pass, which comes after water and before transparent geometry and visual effects; where
//   the final scene differs from it, the resolve and FSR follow the current frame more (reactive mask).
// - The resolve runs on the HDR scene before the game's post processing downsamples it for bloom and exposure (bloom
//   from the jittered scene would jitter), and so before depth of field and tone mapping. DLAA and FSR replace it when
//   they are available, with the same jitter and motion vectors. Native FSR also stabilizes its reconstructed output.
// - What the scene phase draws after the post processing isn't resolved: it gets the projection without the jitter.

export namespace TemporalAA
{
    using Mode = WFPAA::Mode;

    struct FrameCamera
    {
        WFPAA::Matrix View;
        WFPAA::Matrix ProjectionNoJitter;
        WFPAA::Matrix ViewProjectionNoJitter;
        WFPAA::Jitter JitterPixels;             // how far the rendered content moved, in pixels (y down)
        float Near = 0.0f;
        float Far = 0.0f;
        int32_t Width = 0;
        int32_t Height = 0;
        bool Valid = false;
    };

    // Filled on the render thread when the G-buffer pass starts
    FrameCamera CurrentCamera;
    FrameCamera PreviousCamera;

    bool IsTemporalAntialiasing(int32_t value);
    Mode GetMode();
}

class Temporal
{
public:
    // Settings from the ini
    static inline bool bObjectMotionVectors = true;
    static inline bool bSkinnedMotionVectors = true;
    static inline float fObjectDepthBias = 0.00002f;

    static inline HMODULE hm = NULL;

    // ---------------------------------------------------------------------------------------------
    // Object motion: shaders and the previous bones (the module owns everything else)

    static inline rage::grcRenderTargetPC* BoneRT = nullptr;
    static inline bool bVertexTextureSupported = false;

    static constexpr int32_t BoneTexels = 144;        // 48 bones, 3 rows each
    static constexpr int32_t BoneRows = 512;          // skinned draws per frame with previous bones

    static inline IDirect3DVertexShader9* VelocityRigidVS = nullptr;
    static inline IDirect3DVertexShader9* VelocitySkinnedVS = nullptr;
    static inline IDirect3DPixelShader9* VelocityPS = nullptr;
    static inline IDirect3DVertexShader9* BoneWriteVS = nullptr;
    static inline IDirect3DPixelShader9* BoneWritePS = nullptr;
    static inline IDirect3DVertexDeclaration9* BoneWriteDecl = nullptr;

    static bool ShadersLoaded()
    {
        return VelocityRigidVS && VelocitySkinnedVS && VelocityPS && BoneWriteVS && BoneWritePS && BoneWriteDecl;
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

    // The previous bones' target, created with a temporal mode outside the scene passes (EndScene); the module creates
    // its own targets when it needs them
    static void EnsureBones()
    {
        if (BoneRT || !bVertexTextureSupported || !bSkinnedMotionVectors || !bObjectMotionVectors || TemporalAA::GetMode() == TemporalAA::Mode::Off)
            return;

        rage::grcRenderTargetDesc desc{};
        desc.mMultisampleCount = 0;
        desc.field_0 = 1;
        desc.field_12 = 1;
        desc.mDepthRT = nullptr;
        desc.field_8 = 1;
        desc.field_10 = 1;
        desc.field_11 = 1;
        desc.field_24 = false;
        desc.mFormat = rage::GRCFMT_A32B32G32R32F;

        BoneRT = rage::grcTextureFactory::GetInstance()->CreateRenderTarget("TemporalBones", 3, BoneTexels, BoneRows, 128, &desc);
        if (BoneRT)
        {
            rage::grcDevice::grcResolveFlags resolveFlags{};
            rage::grcTextureFactoryPC::GetInstance()->LockRenderTarget(0, BoneRT, nullptr);
            rage::grcTextureFactoryPC::GetInstance()->UnlockRenderTarget(0, &resolveFlags);
        }
    }

    static void ReleaseResources()
    {
        if (BoneRT)
        {
            BoneRT->Destroy();
            BoneRT = nullptr;
        }
        WFPAA::TemporalAA::OnLostDevice();
    }

    // ---------------------------------------------------------------------------------------------
    // Jitter, main thread

    static inline uint32_t JitterCounter = 0;

    // CCamFinal sets up two camera viewports every frame: the game camera's and a second one (square, far clip 800)
    // that the scene is not rendered with. Only the one the G-buffer phase copies is jittered: a jittered viewport
    // that no resolve follows would show the jitter as it is.
    static inline std::array<rage::grcViewport*, 4> CameraViewports{};
    static inline rage::grcViewport* SceneViewport = nullptr;

    static bool SameCamera(const rage::grcViewport* a, const rage::grcViewport* b)
    {
        // P[0][0] and P[1][1] hold the field of view and the aspect ratio, the jitter only changes P[2][0] and P[2][1]
        return a->mWidth == b->mWidth && a->mHeight == b->mHeight && a->mFov == b->mFov && a->mNearClip == b->mNearClip &&
            a->mFarClip == b->mFarClip && a->mProjectionMatrix[0][0] == b->mProjectionMatrix[0][0] &&
            a->mProjectionMatrix[1][1] == b->mProjectionMatrix[1][1];
    }

    // The G-buffer phase of the game viewport, not of the phone camera or of front end scenes: its viewport
    // is a copy of the camera viewport, which becomes the one that is jittered
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

        TemporalAA::GetMode();
        if (!viewport || viewport != SceneViewport || !WFPAA::TemporalAA::IsJitterActive() || viewport->mWidth <= 0 || viewport->mHeight <= 0)
            return hbSetCameraPerspective.fun(_this, edx, viewport, fov, aspect, nearClip, farClip);

        // One step of the sequence per game frame. A counter of the calls advanced twice per frame (both camera
        // viewports): every frame got an odd index, and Halton(odd, 2) >= 0.5, so all samples were in the same half
        // of the pixel horizontally.
        auto frame = CTimer::m_frameCount ? *CTimer::m_frameCount : JitterCounter++;
        auto jitter = WFPAA::TemporalAA::JitterOf(frame);

        // Perspective() copies these projection offsets into P[2][0] and P[2][1]. With P[2][3] = -1 the rendered
        // content moves by -offset in NDC, so this shifts the image right/down by jitter.X/jitter.Y pixels.
        auto shiftX = viewport->field_2D8;
        auto shiftY = viewport->field_2DC;
        viewport->field_2D8 = shiftX - 2.0f * jitter.X / static_cast<float>(viewport->mWidth);
        viewport->field_2DC = shiftY + 2.0f * jitter.Y / static_cast<float>(viewport->mHeight);
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

    // ---------------------------------------------------------------------------------------------
    // Render thread: scene camera

    static IDirect3DDevice9* RealDevice()
    {
        return RageDirect3DDevice9::m_pRealDevice ? *RageDirect3DDevice9::m_pRealDevice : nullptr;
    }

    static void OnSceneBegin()
    {
        using namespace TemporalAA;

        auto viewport = rage::GetCurrentViewport();
        if (!viewport)
            return;

        if (CurrentCamera.Valid)
            PreviousCamera = CurrentCamera;

        FrameCamera camera;
        camera.View = WFPAA::Matrix::From(&viewport->mViewMatrix[0][0]);
        camera.ProjectionNoJitter = WFPAA::Matrix::From(&viewport->mProjectionMatrix[0][0]);
        camera.Width = viewport->mWidth;
        camera.Height = viewport->mHeight;
        camera.Near = viewport->mNearClip;
        camera.Far = viewport->mFarClip;

        if (viewport->mIsPerspective)
        {
            float jitterNdc[2] = { viewport->mProjectionMatrix[2][0] - viewport->field_2D8, viewport->mProjectionMatrix[2][1] - viewport->field_2DC };
            camera.ProjectionNoJitter.m[2][0] = viewport->field_2D8;
            camera.ProjectionNoJitter.m[2][1] = viewport->field_2DC;
            camera.JitterPixels.X = -jitterNdc[0] * 0.5f * static_cast<float>(camera.Width);
            camera.JitterPixels.Y = jitterNdc[1] * 0.5f * static_cast<float>(camera.Height);
        }

        camera.ViewProjectionNoJitter = camera.View * camera.ProjectionNoJitter;
        camera.Valid = viewport->mIsPerspective && camera.Width > 0 && camera.Height > 0 && camera.Near > 0.0f && camera.Far > camera.Near;
        CurrentCamera = camera;

        GetMode();
        WFPAA::TemporalAA::BeginFrame(RealDevice(), camera.View, camera.ProjectionNoJitter, camera.JitterPixels, camera.Valid ? camera.Width : 0,
            camera.Height, camera.Near, camera.Far);
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

        bCapturing = TemporalAA::GetMode() != TemporalAA::Mode::Off && TemporalAA::CurrentCamera.Valid && ShadersLoaded() && bObjectMotionVectors;
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
        auto device = RealDevice();
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
        TemporalRenderStates::ColorWriteState colorWrite;
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
            D3DRS_STENCILENABLE, D3DRS_TWOSIDEDSTENCILMODE, D3DRS_CULLMODE,
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

        explicit StateBackup(IDirect3DDevice9* dev) : device(dev), colorWrite(dev)
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

    static void OnSceneEnd()
    {
        using namespace TemporalAA;

        bool capturing = bCapturing;
        bCapturing = false;

        auto device = RealDevice();
        auto depthRT = rage::grcTextureFactoryPC::GetRTByName("_DEFERRED_GBUFFER_3_");
        if (device && depthRT && depthRT->mD3DTexture && GetMode() != Mode::Off && CurrentCamera.Valid &&
            WFPAA::TemporalAA::RenderMotion(device, depthRT->mD3DTexture))
        {
            if (capturing && !Captures.empty())
            {
                bInternalDraw = true;
                {
                    StateBackup backup(device);
                    RenderObjectMotion(device);
                }
                bInternalDraw = false;
            }
        }

        EndCaptureCleanup();

        // Keep the draws of this frame for the next one
        std::swap(EntitiesPrevious, EntitiesCurrent);
        EntitiesCurrent.clear();
        std::swap(BonesPrevious, BonesCurrent);
        BonesCurrent.clear();
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

    // The moved entities drawn again with their previous world matrix and bones, into the module's motion vectors
    static void RenderObjectMotion(IDirect3DDevice9* device)
    {
        using namespace TemporalAA;

        auto motion = WFPAA::TemporalAA::MotionTexture();
        if (!SceneDepthSurface || !motion)
            return;

        // The game's states of its G-buffer pass are still set here (alpha test, blending, sRGB writes): they would
        // drop or change the previous bones' texels and the motion written below. StateBackup restores them.
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE);
        TemporalRenderStates::ColorWriteState::Set(device, 0x0F);

        if (bSkinnedMotionVectors)
            UploadPreviousBones(device);

        IDirect3DSurface9* motionSurface = nullptr;
        motion->GetSurfaceLevel(0, &motionSurface);
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

        D3DSURFACE_DESC desc{};
        motionSurface->GetDesc(&desc);
        D3DVIEWPORT9 viewport{ 0, 0, desc.Width, desc.Height, 0.0f, 1.0f };

        device->SetRenderTarget(0, motionSurface);
        for (DWORD i = 1; i < 4; ++i)
            device->SetRenderTarget(i, nullptr);
        device->SetDepthStencilSurface(SceneDepthSurface);
        device->SetViewport(&viewport);
        device->SetRenderState(D3DRS_ZENABLE, TRUE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN);
        device->SetRenderState(D3DRS_DEPTHBIAS, 0);
        device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);

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
        if (TemporalAA::GetMode() == TemporalAA::Mode::Off)
            return;
        auto device = RealDevice();
        if (!device)
            return;

        // The fog was drawn into the scene target
        IDirect3DSurface9* sceneSurface = nullptr;
        IDirect3DTexture9* sceneTexture = nullptr;
        device->GetRenderTarget(0, &sceneSurface);
        if (sceneSurface)
            sceneSurface->GetContainer(__uuidof(IDirect3DTexture9), reinterpret_cast<void**>(&sceneTexture));
        if (sceneTexture)
        {
            bInternalDraw = true;
            WFPAA::TemporalAA::CaptureOpaque(device, sceneTexture);
            bInternalDraw = false;
        }
        SAFE_RELEASE(sceneTexture);
        SAFE_RELEASE(sceneSurface);
    }

public:
    // ---------------------------------------------------------------------------------------------
    // Resolve, called by PostFX on the HDR scene before any other post processing. Writes the result
    // into output and returns true, or leaves it untouched.

    static bool Resolve(IDirect3DDevice9* device, IDirect3DTexture9* scene, IDirect3DSurface9* output)
    {
        bInternalDraw = true;
        auto resolved = WFPAA::TemporalAA::Resolve(device, scene, output);
        bInternalDraw = false;
        return resolved;
    }

    // ---------------------------------------------------------------------------------------------
    // After the post processing

    static inline void(__fastcall* RegenerateMatrices)(rage::grcViewport*, void*) = nullptr;

    // CRenderPhaseDrawScene keeps drawing after the post processing, into the resolved image and with the viewport
    // of the scene: CoronasCB (time cycle, ped impostors), render list 10, Coronas::Render, 3D markers. Nothing
    // resolves them, so the jitter would show as it is: they get the projection without it.
    static void RemoveJitterAfterResolve()
    {
        using namespace TemporalAA;

        if (!RegenerateMatrices || !WFPAA::TemporalAA::IsSceneResolved())
            return;

        auto viewport = rage::GetCurrentViewport();
        if (!viewport || !viewport->mIsPerspective || viewport->mWidth != CurrentCamera.Width || viewport->mHeight != CurrentCamera.Height)
            return;

        // P[2][0] and P[2][1] are the projection offsets without jitter, see SetCameraPerspective
        if (viewport->mProjectionMatrix[2][0] == viewport->field_2D8 && viewport->mProjectionMatrix[2][1] == viewport->field_2DC)
            return;

        viewport->mProjectionMatrix[2][0] = viewport->field_2D8;
        viewport->mProjectionMatrix[2][1] = viewport->field_2DC;
        RegenerateMatrices(viewport, nullptr);
    }

    Temporal()
    {
        FusionFix::onInitEventAsync() += []()
        {
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&Resolve, &hm);

            SettingsReader reader;
            WFPAA::Options options;
            options.BridgeName = L"Local\\GTAIV.EFLC.FusionFix.Upscaler";
            options.ClientName = "GTAIV.EFLC.FusionFix";
            options.Log = GetThisModulePath() / L"GTAIV.EFLC.FusionFix.AntiAliasing.log";
            options.HubLog = GetThisModulePath() / L"GTAIV.EFLC.FusionFix.Upscaler.log";
            options.LowDynamicRange = false;
            options.LogarithmicDepth = true;
            // The game draws the motion of its moving entities itself (RenderObjectMotion)
            options.ObjectMotion = false;
            options.JitterPhases = static_cast<uint32_t>(std::clamp(reader.ReadInteger("TEMPORAL", "JitterPhases", 8), 2, 128));
            // Native-resolution FSR uses an eight-phase cycle (8 * display/render ratio squared)
            options.FSRJitterPhases = 8;
            options.CurrentWeightMin = std::clamp(reader.ReadFloat("TEMPORAL", "CurrentFrameWeightMin", 0.05f), 0.01f, 1.0f);
            options.CurrentWeightMax = std::clamp(reader.ReadFloat("TEMPORAL", "CurrentFrameWeightMax", 0.2f), options.CurrentWeightMin, 1.0f);
            options.CurrentWeightPerPixel = std::max(reader.ReadFloat("TEMPORAL", "CurrentFrameWeightPerPixelOfMotion", 0.1f), 0.0f);
            options.VarianceClipGamma = std::clamp(reader.ReadFloat("TEMPORAL", "VarianceClipGamma", 1.25f), 0.5f, 4.0f);
            options.StaticHistoryTrust = std::clamp(reader.ReadFloat("TEMPORAL", "StaticHistoryTrust", 0.5f), 0.0f, 1.0f);
            options.LumaWeight = std::clamp(reader.ReadFloat("TEMPORAL", "LumaWeight", 1.0f), 0.0f, 16.0f);
            options.CameraCutDistance = std::max(reader.ReadFloat("TEMPORAL", "CameraCutDistance", 25.0f), 1.0f);
            options.DLSSPreset = static_cast<uint32_t>(std::clamp(reader.ReadInteger("TEMPORAL", "DLSSPreset", 0), 0, 15));
            options.FSRSharpness = std::clamp(reader.ReadFloat("TEMPORAL", "FSRSharpness", 0.0f), 0.0f, 1.0f);
            // Hidden: DLAA and FSR on Windows' D3D9on12 (Graphics API DirectX 12), which is too slow to be offered
            options.D3D9On12Interop = reader.ReadInteger("TEMPORAL", "D3D9on12Interop", 0) != 0;
            options.ReactiveMask = reader.ReadInteger("TEMPORAL", "ReactiveMask", 1) != 0;
            options.ReactiveScale = std::clamp(reader.ReadFloat("TEMPORAL", "ReactiveMaskScale", 2.0f), 0.0f, 64.0f);
            options.ReactiveMax = std::clamp(reader.ReadFloat("TEMPORAL", "ReactiveMaskMaximum", 0.75f), 0.0f, 1.0f);
            options.StabilizeFSR = true;
            WFPAA::TemporalAA::Configure(options);

            fObjectDepthBias = std::clamp(reader.ReadFloat("TEMPORAL", "ObjectDepthBias", 0.00002f), 0.0f, 0.01f);
            bObjectMotionVectors = reader.ReadInteger("TEMPORAL", "ObjectMotionVectors", 1) != 0;
            bSkinnedMotionVectors = reader.ReadInteger("TEMPORAL", "SkinnedMotionVectors", 1) != 0;

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

            // Rebuilds the composite matrices of a viewport and uploads them when it's the current one
            pattern = find_pattern("55 8B EC 83 E4 F8 51 56 8B F1 8D 86 80 00 00 00 50 8D 86 80 01 00 00 50 8D 8E C0 00 00 00 E8",
                "56 8B F1 8D 86 80 00 00 00 50 8D 86 80 01 00 00 50 8D 8E C0 00 00 00 E8");
            if (!pattern.empty())
                RegenerateMatrices = reinterpret_cast<decltype(RegenerateMatrices)>(pattern.get_first(0));

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
                case FusionFixSettings.AntialiasingText.eTAA:
                    // resolved by Fusion Fix post processing
                    return GetD3DX9_43DLL() != nullptr;
                case FusionFixSettings.AntialiasingText.eDLAA:
                    return WFPAA::TemporalAA::BridgeAvailable(RealDevice()) && WFPAA::TemporalAA::IsAvailable(WFPAA::Backend::DLSS);
                case FusionFixSettings.AntialiasingText.eFSR:
                    return WFPAA::TemporalAA::BridgeAvailable(RealDevice()) && WFPAA::TemporalAA::IsAvailable(WFPAA::Backend::FSR);
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
                if (generation != WFPAA::TemporalAA::Generation())
                {
                    generation = WFPAA::TemporalAA::Generation();
                    FusionFixSettings.RefreshAvailability("PREF_ANTIALIASING");
                }
            };
            FusionFix::onGameProcessEvent() += []() { refreshAvailability(); };
            FusionFix::onMenuDrawingEvent() += []() { refreshAvailability(); };

            FusionFix::onShutdownEvent() += []()
            {
                WFPAA::TemporalAA::Shutdown();
            };
            FusionFix::onEndScene() += []()
            {
                EnsureBones();
            };

            CRenderPhaseDeferredLighting_SceneToGBuffer::OnBuildRenderList() += []()
            {
                bBuildingSceneList = IsCameraScene();
                bMarkingEntities = bBuildingSceneList && bObjectMotionVectors && TemporalAA::GetMode() != TemporalAA::Mode::Off;
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

    // The chosen mode, given to the module; DLAA and FSR are TAA while the bridge can't run them
    Mode GetMode()
    {
        static auto aa = FusionFixSettings.GetRef("PREF_ANTIALIASING");
        auto requested = Mode::Off;
        if (aa)
        {
            switch (aa->get())
            {
            case FusionFixSettings.AntialiasingText.eTAA: requested = Mode::TAA; break;
            case FusionFixSettings.AntialiasingText.eDLAA: requested = Mode::DLAA; break;
            case FusionFixSettings.AntialiasingText.eFSR: requested = Mode::FSR; break;
            default: break;
            }
        }
        WFPAA::TemporalAA::SetMode(requested, 0.0f);
        return WFPAA::TemporalAA::EffectiveMode();
    }

    void LoadShaders(IDirect3DDevice9* device)
    {
        Temporal::LoadShaders(device);
    }

    // The module creates its targets when it needs them, the previous bones' target comes at the end of a scene
    void CreateResources(uint32_t, uint32_t)
    {
    }

    void ReleaseResources()
    {
        Temporal::ReleaseResources();
    }

    bool Resolve(IDirect3DDevice9* device, IDirect3DTexture9* scene, IDirect3DTexture9*, IDirect3DSurface9* output)
    {
        return Temporal::Resolve(device, scene, output);
    }

    // The scene of this frame went through the resolve, DLAA or FSR
    bool IsSceneResolved()
    {
        return WFPAA::TemporalAA::IsSceneResolved();
    }

    // Called by PostFX right after the fog pass of the scene
    void OnFogDrawn()
    {
        Temporal::CaptureOpaque();
    }

    // Called by PostFX once the post processing of the scene is drawn
    void OnPostFXDrawn()
    {
        Temporal::RemoveJitterAfterResolve();
    }
}
