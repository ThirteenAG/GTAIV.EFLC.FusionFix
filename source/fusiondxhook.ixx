module;

#include <common.hxx>

export module fusiondxhook;
import common;

#pragma message ("FusionDxHook is disabled")

import <map>;
import <functional>;
import <numeric>;
import <algorithm>;
import <functional>;
import <typeindex>;
import <typeinfo>;

class VTBL
{
private:
    virtual std::vector<const char*>& GetMethods() = 0;
public:
    const auto GetNumberOfMethods() {
        return GetMethods().size();
    }
    const auto GetIndex(const char* name) {
        auto iter = std::find(std::begin(GetMethods()), std::end(GetMethods()), name);
        if (iter != std::end(GetMethods()))
            return (int32_t)std::distance(std::begin(GetMethods()), iter);
        else
            return (int32_t)-1;
    }
    const auto GetMethod(int32_t index) {
        if (index < static_cast<int32_t>(GetNumberOfMethods()))
            return GetMethods()[index];
        else
            return "";
    }
};

class IDirect3D9VTBL : public VTBL
{
private:
    std::vector<const char*>& GetMethods() override {
        static std::vector<const char*> methodsNames{
            "QueryInterface",
            "AddRef",
            "Release",
            "RegisterSoftwareDevice",
            "GetAdapterCount",
            "GetAdapterIdentifier",
            "GetAdapterModeCount",
            "EnumAdapterModes",
            "GetAdapterDisplayMode",
            "CheckDeviceType",
            "CheckDeviceFormat",
            "CheckDeviceMultiSampleType",
            "CheckDepthStencilMatch",
            "CheckDeviceFormatConversion",
            "GetDeviceCaps",
            "GetAdapterMonitor",
            "CreateDevice",
        };
        return methodsNames;
    }
};

class IDirect3DDevice9VTBL : public VTBL
{
private:
    std::vector<const char*>& GetMethods() override {
        static std::vector<const char*> methodsNames{
            "QueryInterface",
            "AddRef",
            "Release",
            "TestCooperativeLevel",
            "GetAvailableTextureMem",
            "EvictManagedResources",
            "GetDirect3D",
            "GetDeviceCaps",
            "GetDisplayMode",
            "GetCreationParameters",
            "SetCursorProperties",
            "SetCursorPosition",
            "ShowCursor",
            "CreateAdditionalSwapChain",
            "GetSwapChain",
            "GetNumberOfSwapChains",
            "Reset",
            "Present",
            "GetBackBuffer",
            "GetRasterStatus",
            "SetDialogBoxMode",
            "SetGammaRamp",
            "GetGammaRamp",
            "CreateTexture",
            "CreateVolumeTexture",
            "CreateCubeTexture",
            "CreateVertexBuffer",
            "CreateIndexBuffer",
            "CreateRenderTarget",
            "CreateDepthStencilSurface",
            "UpdateSurface",
            "UpdateTexture",
            "GetRenderTargetData",
            "GetFrontBufferData",
            "StretchRect",
            "ColorFill",
            "CreateOffscreenPlainSurface",
            "SetRenderTarget",
            "GetRenderTarget",
            "SetDepthStencilSurface",
            "GetDepthStencilSurface",
            "BeginScene",
            "EndScene",
            "Clear",
            "SetTransform",
            "GetTransform",
            "MultiplyTransform",
            "SetViewport",
            "GetViewport",
            "SetMaterial",
            "GetMaterial",
            "SetLight",
            "GetLight",
            "LightEnable",
            "GetLightEnable",
            "SetClipPlane",
            "GetClipPlane",
            "SetRenderState",
            "GetRenderState",
            "CreateStateBlock",
            "BeginStateBlock",
            "EndStateBlock",
            "SetClipStatus",
            "GetClipStatus",
            "GetTexture",
            "SetTexture",
            "GetTextureStageState",
            "SetTextureStageState",
            "GetSamplerState",
            "SetSamplerState",
            "ValidateDevice",
            "SetPaletteEntries",
            "GetPaletteEntries",
            "SetCurrentTexturePalette",
            "GetCurrentTexturePalette",
            "SetScissorRect",
            "GetScissorRect",
            "SetSoftwareVertexProcessing",
            "GetSoftwareVertexProcessing",
            "SetNPatchMode",
            "GetNPatchMode",
            "DrawPrimitive",
            "DrawIndexedPrimitive",
            "DrawPrimitiveUP",
            "DrawIndexedPrimitiveUP",
            "ProcessVertices",
            "CreateVertexDeclaration",
            "SetVertexDeclaration",
            "GetVertexDeclaration",
            "SetFVF",
            "GetFVF",
            "CreateVertexShader",
            "SetVertexShader",
            "GetVertexShader",
            "SetVertexShaderConstantF",
            "GetVertexShaderConstantF",
            "SetVertexShaderConstantI",
            "GetVertexShaderConstantI",
            "SetVertexShaderConstantB",
            "GetVertexShaderConstantB",
            "SetStreamSource",
            "GetStreamSource",
            "SetStreamSourceFreq",
            "GetStreamSourceFreq",
            "SetIndices",
            "GetIndices",
            "CreatePixelShader",
            "SetPixelShader",
            "GetPixelShader",
            "SetPixelShaderConstantF",
            "GetPixelShaderConstantF",
            "SetPixelShaderConstantI",
            "GetPixelShaderConstantI",
            "SetPixelShaderConstantB",
            "GetPixelShaderConstantB",
            "DrawRectPatch",
            "DrawTriPatch",
            "DeletePatch",
            "CreateQuery",
            "SetConvolutionMonoKernel",
            "ComposeRects",
            "PresentEx",
            "GetGPUThreadPriority",
            "SetGPUThreadPriority",
            "WaitForVBlank",
            "CheckResourceResidency",
            "SetMaximumFrameLatency",
            "GetMaximumFrameLatency",
            "CheckDeviceState",
            "CreateRenderTargetEx",
            "CreateOffscreenPlainSurfaceEx",
            "CreateDepthStencilSurfaceEx",
            "ResetEx",
            "GetDisplayModeEx",
        };
        return methodsNames;
    }
};

export SafetyHookInline CreateDeviceOriginal = {};
export SafetyHookInline BeginSceneOriginal = {};
export SafetyHookInline EndSceneOriginal = {};
export SafetyHookInline ResetOriginal = {};
export SafetyHookInline SetVertexShaderConstantFOriginal = {};
export SafetyHookInline SetPixelShaderConstantFOriginal = {};
export SafetyHookInline CreateTextureOriginal = {};
export SafetyHookInline SetTextureOriginal = {};
export SafetyHookInline DrawPrimitiveOriginal = {};
export SafetyHookInline SetPixelShaderOriginal = {};
export SafetyHookInline SetVertexShaderOriginal = {};
export SafetyHookInline CreatePixelShaderOriginal = {};
export SafetyHookInline CreateVertexShaderOriginal = {};

class FusionDxHook
{
private:
    static inline std::map<const HMODULE, std::map<std::type_index, std::vector<uintptr_t*>>> deviceMethods;

    class HookWindow
    {
    public:
        HookWindow(std::wstring_view className = L"FusionDxHook", std::wstring windowName = L"FusionDxHook")
        {
            windowClass.cbSize = sizeof(WNDCLASSEX);
            windowClass.style = CS_HREDRAW | CS_VREDRAW;
            windowClass.lpfnWndProc = DefWindowProc;
            windowClass.cbClsExtra = 0;
            windowClass.cbWndExtra = 0;
            windowClass.hInstance = GetModuleHandle(NULL);
            windowClass.hIcon = NULL;
            windowClass.hCursor = NULL;
            windowClass.hbrBackground = NULL;
            windowClass.lpszMenuName = NULL;
            windowClass.lpszClassName = className.data();
            windowClass.hIconSm = NULL;
            RegisterClassExW(&windowClass);
            window = CreateWindowW(windowClass.lpszClassName, windowName.data(), WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, windowClass.hInstance, NULL);
        }
        ~HookWindow()
        {
            DestroyWindow(window);
            UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
        }
        static HWND GetHookWindow(HookWindow& hw)
        {
            return hw.window;
        }
    private:
        WNDCLASSEX windowClass{};
        HWND window{};
    };

    template <class... Ts>
    static inline void copyMethods(HMODULE mod, Ts&& ... inputs)
    {
        ([&] {
            auto vtbl_info = std::get<0>(inputs);
            auto ptr = std::get<1>(inputs);
            deviceMethods[mod][typeid(vtbl_info)].resize(vtbl_info.GetNumberOfMethods());
            std::memcpy(deviceMethods.at(mod).at(typeid(vtbl_info)).data(), ptr, vtbl_info.GetNumberOfMethods() * sizeof(uintptr_t));
        } (), ...);
    }

public:
    FusionDxHook()
    {
        FusionFix::onInitEventAsync() += []()
        {
            return;

            auto hD3D9 = GetModuleHandleW(L"d3d9.dll");
            if (!hD3D9) return;
            auto Direct3DCreate9 = GetProcAddress(hD3D9, "Direct3DCreate9");
            if (!Direct3DCreate9) return;
            auto Direct3D9 = ((LPDIRECT3D9(WINAPI*)(uint32_t))(Direct3DCreate9))(D3D_SDK_VERSION);
            if (!Direct3D9) return;

            auto hookWindowCtor = HookWindow(L"FusionDxHookD3D9", L"FusionDxHookD3D9");
            auto hWnd = HookWindow::GetHookWindow(hookWindowCtor);

            D3DPRESENT_PARAMETERS params;
            params.BackBufferWidth = 0;
            params.BackBufferHeight = 0;
            params.BackBufferFormat = D3DFMT_UNKNOWN;
            params.BackBufferCount = 0;
            params.MultiSampleType = D3DMULTISAMPLE_NONE;
            params.MultiSampleQuality = NULL;
            params.SwapEffect = D3DSWAPEFFECT_DISCARD;
            params.hDeviceWindow = hWnd;
            params.Windowed = 1;
            params.EnableAutoDepthStencil = 0;
            params.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
            params.Flags = NULL;
            params.FullScreen_RefreshRateInHz = 0;
            params.PresentationInterval = 0;

            LPDIRECT3DDEVICE9 Device;
            if (Direct3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT, &params, &Device) >= 0)
            {
                copyMethods(hD3D9,
                    std::forward_as_tuple(IDirect3DDevice9VTBL(), *(uintptr_t**)Device),
                    std::forward_as_tuple(IDirect3D9VTBL(), *(uintptr_t**)Direct3D9)
                );

                auto D3D9CreateDevice = [](LPDIRECT3D9 pDirect3D9, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface) -> HRESULT
                {
                    FusionFix::D3D9::onBeforeCreateDevice().executeAll(pDirect3D9, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
                    auto res = CreateDeviceOriginal.unsafe_stdcall<HRESULT>(pDirect3D9, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
                    FusionFix::D3D9::onAfterCreateDevice().executeAll(pDirect3D9, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
                    return res;
                };

                auto D3D9BeginScene = [](LPDIRECT3DDEVICE9 pDevice) -> HRESULT
                {
                    FusionFix::D3D9::onBeginScene().executeAll(pDevice);
                    return BeginSceneOriginal.unsafe_stdcall<HRESULT>(pDevice);
                };

                auto D3D9EndScene = [](LPDIRECT3DDEVICE9 pDevice) -> HRESULT
                {
                    FusionFix::D3D9::onEndScene().executeAll(pDevice);
                    return EndSceneOriginal.unsafe_stdcall<HRESULT>(pDevice);
                };

                auto D3D9Reset = [](LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters) -> HRESULT
                {
                    FusionFix::D3D9::onBeforeReset().executeAll(pDevice, pPresentationParameters);
                    auto res = ResetOriginal.unsafe_stdcall<HRESULT>(pDevice, pPresentationParameters);
                    FusionFix::D3D9::onAfterReset().executeAll(pDevice, pPresentationParameters);
                    return res;
                };

                auto D3D9SetVertexShaderConstantF = [](LPDIRECT3DDEVICE9 pDevice, UINT StartRegister, float* pConstantData, UINT Vector4fCount) -> HRESULT
                {
                    FusionFix::D3D9::onSetVertexShaderConstantF().executeAll(pDevice, StartRegister, pConstantData, Vector4fCount);
                    return SetVertexShaderConstantFOriginal.unsafe_stdcall<HRESULT>(pDevice, StartRegister, pConstantData, Vector4fCount);
                };

                auto D3D9SetPixelShaderConstantF = [](LPDIRECT3DDEVICE9 pDevice, UINT StartRegister, float* pConstantData, UINT Vector4fCount) -> HRESULT
                {
                    FusionFix::D3D9::onSetPixelShaderConstantF().executeAll(pDevice, StartRegister, pConstantData, Vector4fCount);
                    return SetPixelShaderConstantFOriginal.unsafe_stdcall<HRESULT>(pDevice, StartRegister, pConstantData, Vector4fCount);
                };

                auto D3D9CreateTexture = [](LPDIRECT3DDEVICE9 pDevice, UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) ->HRESULT
                {
                    FusionFix::D3D9::onBeforeCreateTexture().executeAll(pDevice, Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);
                    auto res = CreateTextureOriginal.unsafe_stdcall<HRESULT>(pDevice, Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);
                    FusionFix::D3D9::onAfterCreateTexture().executeAll(pDevice, Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);
                    return res;
                };

                auto D3D9SetTexture = [](LPDIRECT3DDEVICE9 pDevice, DWORD Stage, IDirect3DBaseTexture9* pTexture) -> HRESULT
                {
                    FusionFix::D3D9::onSetTexture().executeAll(pDevice, Stage, pTexture);
                    return SetTextureOriginal.unsafe_stdcall<HRESULT>(pDevice, Stage, pTexture);
                };

                auto D3D9DrawPrimitive = [](LPDIRECT3DDEVICE9 pDevice, D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) -> HRESULT
                {
                    FusionFix::D3D9::onBeforeDrawPrimitive().executeAll(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                    auto hr = S_OK;
                    if (!FusionFix::D3D9::isInsteadDrawPrimitive())
                        hr = DrawPrimitiveOriginal.unsafe_stdcall<HRESULT>(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                    FusionFix::D3D9::setInsteadDrawPrimitive(false);
                    FusionFix::D3D9::onAfterDrawPrimitive().executeAll(pDevice, PrimitiveType, StartVertex, PrimitiveCount);
                    return hr;
                };

                auto D3D9SetPixelShader = [](LPDIRECT3DDEVICE9 pDevice, IDirect3DPixelShader9* pShader) -> HRESULT
                {
                    FusionFix::D3D9::onBeforeSetPixelShader().executeAll(pDevice, pShader);
                    auto hr = SetPixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, pShader);
                    FusionFix::D3D9::onAfterSetPixelShader().executeAll(pDevice, pShader);
                    return hr;
                };

                auto D3D9SetVertexShader = [](LPDIRECT3DDEVICE9 pDevice, IDirect3DVertexShader9* pShader) -> HRESULT
                {
                    FusionFix::D3D9::onBeforeSetVertexShader().executeAll(pDevice, pShader);
                    auto hr = SetVertexShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, pShader);
                    FusionFix::D3D9::onAfterSetVertexShader().executeAll(pDevice, pShader);
                    return hr;
                };

                auto D3D9CreatePixelShader = [](LPDIRECT3DDEVICE9 pDevice, DWORD* pFunction, IDirect3DPixelShader9** ppShader) -> HRESULT
                {
                    FusionFix::D3D9::onBeforeCreatePixelShader().executeAll(pDevice, pFunction, ppShader);
                    auto hr = CreatePixelShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, pFunction, ppShader);
                    FusionFix::D3D9::onAfterCreatePixelShader().executeAll(pDevice, pFunction, ppShader);
                    return hr;
                };

                auto D3D9CreateVertexShader = [](LPDIRECT3DDEVICE9 pDevice, DWORD* pFunction, IDirect3DVertexShader9** ppShader) -> HRESULT
                {
                    FusionFix::D3D9::onBeforeCreateVertexShader().executeAll(pDevice, pFunction, ppShader);
                    auto hr = CreateVertexShaderOriginal.unsafe_stdcall<HRESULT>(pDevice, pFunction, ppShader);
                    FusionFix::D3D9::onAfterCreateVertexShader().executeAll(pDevice, pFunction, ppShader);
                    return hr;
                };

                static HRESULT(WINAPI* CreateDevice)(LPDIRECT3D9, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**) = D3D9CreateDevice;
                static HRESULT(WINAPI* BeginScene)(LPDIRECT3DDEVICE9) = D3D9BeginScene;
                static HRESULT(WINAPI* EndScene)(LPDIRECT3DDEVICE9) = D3D9EndScene;
                static HRESULT(WINAPI* Reset)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*) = D3D9Reset;
                static HRESULT(WINAPI* SetVertexShaderConstantF)(LPDIRECT3DDEVICE9, UINT, float*, UINT) = D3D9SetVertexShaderConstantF;
                static HRESULT(WINAPI* SetPixelShaderConstantF)(LPDIRECT3DDEVICE9, UINT, float*, UINT) = D3D9SetPixelShaderConstantF;
                static HRESULT(WINAPI* CreateTexture)(LPDIRECT3DDEVICE9, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*) = D3D9CreateTexture;
                static HRESULT(WINAPI* SetTexture)(LPDIRECT3DDEVICE9, DWORD, IDirect3DBaseTexture9*) = D3D9SetTexture;
                static HRESULT(WINAPI* DrawPrimitive)(LPDIRECT3DDEVICE9, D3DPRIMITIVETYPE, UINT, UINT) = D3D9DrawPrimitive;
                static HRESULT(WINAPI* SetPixelShader)(LPDIRECT3DDEVICE9, IDirect3DPixelShader9*) = D3D9SetPixelShader;
                static HRESULT(WINAPI* SetVertexShader)(LPDIRECT3DDEVICE9, IDirect3DVertexShader9*) = D3D9SetVertexShader;
                static HRESULT(WINAPI* CreatePixelShader)(LPDIRECT3DDEVICE9, DWORD*, IDirect3DPixelShader9**) = D3D9CreatePixelShader;
                static HRESULT(WINAPI* CreateVertexShader)(LPDIRECT3DDEVICE9, DWORD*, IDirect3DVertexShader9**) = D3D9CreateVertexShader;

                bind(hD3D9, typeid(IDirect3D9VTBL), IDirect3D9VTBL().GetIndex("CreateDevice"), CreateDevice, CreateDeviceOriginal);
                bind(hD3D9, typeid(IDirect3DDevice9VTBL), IDirect3DDevice9VTBL().GetIndex("BeginScene"), BeginScene, BeginSceneOriginal);
                bind(hD3D9, typeid(IDirect3DDevice9VTBL), IDirect3DDevice9VTBL().GetIndex("EndScene"), EndScene, EndSceneOriginal);
                bind(hD3D9, typeid(IDirect3DDevice9VTBL), IDirect3DDevice9VTBL().GetIndex("Reset"), Reset, ResetOriginal);
                bind(hD3D9, typeid(IDirect3DDevice9VTBL), IDirect3DDevice9VTBL().GetIndex("SetVertexShaderConstantF"), SetVertexShaderConstantF, SetVertexShaderConstantFOriginal);
                bind(hD3D9, typeid(IDirect3DDevice9VTBL), IDirect3DDevice9VTBL().GetIndex("SetPixelShaderConstantF"), SetPixelShaderConstantF, SetPixelShaderConstantFOriginal);
                bind(hD3D9, typeid(IDirect3DDevice9VTBL), IDirect3DDevice9VTBL().GetIndex("CreateTexture"), CreateTexture, CreateTextureOriginal);
                bind(hD3D9, typeid(IDirect3DDevice9VTBL), IDirect3DDevice9VTBL().GetIndex("SetTexture"), SetTexture, SetTextureOriginal);
                bind(hD3D9, typeid(IDirect3DDevice9VTBL), IDirect3DDevice9VTBL().GetIndex("DrawPrimitive"), DrawPrimitive, DrawPrimitiveOriginal);
                bind(hD3D9, typeid(IDirect3DDevice9VTBL), IDirect3DDevice9VTBL().GetIndex("SetPixelShader"), SetPixelShader, SetPixelShaderOriginal);
                bind(hD3D9, typeid(IDirect3DDevice9VTBL), IDirect3DDevice9VTBL().GetIndex("SetVertexShader"), SetVertexShader, SetVertexShaderOriginal);
                bind(hD3D9, typeid(IDirect3DDevice9VTBL), IDirect3DDevice9VTBL().GetIndex("CreatePixelShader"), CreatePixelShader, CreatePixelShaderOriginal);
                bind(hD3D9, typeid(IDirect3DDevice9VTBL), IDirect3DDevice9VTBL().GetIndex("CreateVertexShader"), CreateVertexShader, CreateVertexShaderOriginal);

                Device->Release();
            }
            Direct3D9->Release();
        };
    }

public:
    static inline void bind(HMODULE module, std::type_index type_index, uint16_t func_index, void* function, SafetyHookInline& hook)
    {
        auto target = deviceMethods.at(module).at(type_index).at(func_index);
        hook = safetyhook::create_inline(target, function);
    }
    static inline void unbind(SafetyHookInline& hook)
    {
        hook.reset();
    }
} FusionDxHook;