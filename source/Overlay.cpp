#include <common.hxx>
#include <Commctrl.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <Xinput.h>
#include "OverlayInput.hpp"
#include <overlay/OverlayD3D9.hpp>
#include <upscaler/graphics_api.h>

import common;
import settings;

// In-game settings overlay (Ctrl+F10 / Back + Start), the hub-style editor of every loaded plugin's ASI Settings, including
// Advanced settings. The game's own Fusion Fix menu stays the regular menu. Library: the overlay of FusionFixUtils
// (external/FusionFixUtils/overlay); adapter structure as in Max Payne 3 Fusion Fix (same RAGE input paths).
namespace GTAIVOverlay
{
    namespace
    {
        asi_overlay::D3D9Overlay overlay;
        HMODULE host = nullptr;
        HWND attachedWindow = nullptr;
        constexpr const char* languages[] = { "en", "fr", "de", "it", "es", "ru", "ja" }; // PREF_CURRENT_LANGUAGE
        constexpr UINT_PTR subclassId = 0x46464F56;
        std::recursive_mutex inputMutex;
        // Device implementations can share methods. Hook each implementation
        // once and retain its trampoline; never alter or retain a COM object's vtable.
        std::map<void*, SafetyHookInline> createDeviceHooks, stateHooks, dataHooks;
        std::set<void*> mice;
        bool bufferedMouse = false;
        using CreateDI = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
        CreateDI createDI = nullptr;
        using GetPad = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
        std::atomic<GetPad> getPad = nullptr;
        std::atomic<DWORD> activePad = 0;
        void FeedPad(const XINPUT_GAMEPAD& pad);
        decltype(&GetProcAddress) resolveProc = GetProcAddress;
        decltype(&GetKeyboardState) keyboardState = GetKeyboardState;

        void* Method(void* object, size_t slot) { return (*static_cast<void***>(object))[slot]; }

        HRESULT WINAPI DeviceState(IDirectInputDevice8W* device, DWORD size, void* data)
        {
            std::lock_guard lock(inputMutex);
            auto result = stateHooks.at(Method(device, 9)).stdcall<HRESULT>(device, size, data);
            // The game reads the mouse through DirectInput: window messages may carry no clicks, so feed the overlay here.
            if (SUCCEEDED(result) && data && overlay.IsOpen() && (size == sizeof(DIMOUSESTATE) || size == sizeof(DIMOUSESTATE2)))
            {
                auto mouse = static_cast<const DIMOUSESTATE*>(data);
                for (int i = 0; i < 3; ++i) overlay.MouseButton(i, (mouse->rgbButtons[i] & 0x80) != 0);
                if (!bufferedMouse) overlay.MouseWheel(float(mouse->lZ) / WHEEL_DELTA);
            }
            if (SUCCEEDED(result) && data && BlocksInput() && (size == 256 || size == sizeof(DIMOUSESTATE) || size == sizeof(DIMOUSESTATE2))) memset(data, 0, size);
            return result;
        }

        HRESULT WINAPI DeviceData(IDirectInputDevice8W* device, DWORD size, DIDEVICEOBJECTDATA* data, DWORD* count, DWORD flags)
        {
            std::lock_guard lock(inputMutex);
            if (BlocksInput()) flags &= ~DIGDD_PEEK;
            auto result = dataHooks.at(Method(device, 10)).stdcall<HRESULT>(device, size, data, count, flags);
            if (SUCCEEDED(result) && data && count && overlay.IsOpen() && mice.contains(device))
            {
                bufferedMouse = true;
                for (DWORD i = 0; i < *count; ++i)
                {
                    auto& event = *reinterpret_cast<const DIDEVICEOBJECTDATA*>(reinterpret_cast<const char*>(data) + i * size);
                    if (event.dwOfs >= DIMOFS_BUTTON0 && event.dwOfs <= DIMOFS_BUTTON2) overlay.MouseButton(int(event.dwOfs - DIMOFS_BUTTON0), (event.dwData & 0x80) != 0);
                    else if (event.dwOfs == DIMOFS_Z) overlay.MouseWheel(float(LONG(event.dwData)) / WHEEL_DELTA);
                }
            }
            // Drain the original queue while open, so closing cannot replay clicks.
            if (SUCCEEDED(result) && count && BlocksInput()) *count = 0;
            return result;
        }

        HRESULT WINAPI CreateDevice(IDirectInput8W* input, REFGUID guid, IDirectInputDevice8W** device, IUnknown* outer)
        {
            std::lock_guard lock(inputMutex);
            // REFGUID must stay a pointer: the variadic trampoline call copies arguments by value.
            auto result = createDeviceHooks.at(Method(input, 3)).stdcall<HRESULT>(input, &guid, device, outer);
            if (SUCCEEDED(result) && device && *device && (guid == GUID_SysKeyboard || guid == GUID_SysMouse))
            {
                // Keyboard and relative mouse packets have a zero neutral state, unlike absolute joystick axes.
                if (guid == GUID_SysMouse) mice.insert(*device);
                auto state = Method(*device, 9), data = Method(*device, 10);
                if (!stateHooks.contains(state)) stateHooks.emplace(state, safetyhook::create_inline(state, DeviceState));
                if (!dataHooks.contains(data)) dataHooks.emplace(data, safetyhook::create_inline(data, DeviceData));
            }
            return result;
        }

        HRESULT WINAPI DirectInputCreate(HINSTANCE instance, DWORD version, REFIID iid, void** input, IUnknown* outer)
        {
            auto result = createDI(instance, version, iid, input, outer);
            if (SUCCEEDED(result) && input && *input)
            {
                std::lock_guard lock(inputMutex);
                auto method = Method(*input, 3);
                if (!createDeviceHooks.contains(method)) createDeviceHooks.emplace(method, safetyhook::create_inline(method, CreateDevice));
            }
            return result;
        }

        DWORD WINAPI PadState(DWORD index, XINPUT_STATE* state)
        {
            auto original = getPad.load();
            auto result = original ? original(index, state) : ERROR_DEVICE_NOT_CONNECTED;
            if (result == ERROR_SUCCESS && state && index == activePad) FeedPad(state->Gamepad);
            if (result == ERROR_SUCCESS && state && BlocksInput()) state->Gamepad = {};
            return result;
        }

        FARPROC WINAPI ResolveProc(HMODULE module, LPCSTR name)
        {
            auto proc = resolveProc(module, name);
            if (reinterpret_cast<uintptr_t>(name) > 0xFFFF && strcmp(name, "XInputGetState") == 0 && proc)
            {
                getPad = reinterpret_cast<GetPad>(proc);
                return reinterpret_cast<FARPROC>(PadState);
            }
            return proc;
        }

        BOOL WINAPI KeyboardState(PBYTE keys)
        {
            auto result = keyboardState(keys);
            if (result && BlocksInput()) memset(keys, 0, 256);
            return result;
        }

        LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
        {
            LRESULT result = 0;
            if (overlay.Message(hwnd, message, wp, lp, result)) return result;
            if (message == WM_NCDESTROY)
            {
                overlay.SetOpen(false);
                RemoveWindowSubclass(hwnd, WindowProc, subclassId);
                attachedWindow = nullptr;
            }
            return DefSubclassProc(hwnd, message, wp, lp);
        }

        void FeedPad(const XINPUT_GAMEPAD& pad)
        {
            auto b = pad.wButtons;
            uint16_t mapped = 0;
            const WORD from[] = { XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_START, XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_RIGHT,
                XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER,
                XINPUT_GAMEPAD_Y, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_X };
            const uint16_t to[] = { 1, 8, 0x10, 0x20, 0x40, 0x80, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000 };
            for (size_t i = 0; i < std::size(from); ++i) if (b & from[i]) mapped |= to[i];
            overlay.Gamepad(mapped, int8_t(pad.sThumbLX / 256), int8_t(-int(pad.sThumbLY) / 258));
        }

        void PollPad()
        {
            auto original = getPad.load();
            if (!original)
            {
                if (auto module = GetModuleHandleW(L"xinput1_3.dll"))
                    getPad = original = reinterpret_cast<GetPad>(GetProcAddress(module, "XInputGetState"));
            }
            XINPUT_STATE state{};
            if (original) for (DWORD i = 0; i < 4; ++i) if (original(i, &state) == ERROR_SUCCESS) { activePad = i; break; }
            FeedPad(state.Gamepad);
        }
    }

    bool BlocksInput() { return overlay.BlocksInput(); }

    void AttachWindow(HWND window)
    {
        if (window && attachedWindow != window && SetWindowSubclass(window, WindowProc, subclassId, 0)) attachedWindow = window;
    }

    namespace
    {
        IDirect3DDevice9* lastDevice = nullptr; // the device of the frame being drawn, for the Debug pages

        // A sample extension page (FusionFixUtils overlay/readme.md, "Extending the overlay"): Debug > Info.
        // Delete it or add more pages, rows and menus the same way.
        void RegisterDebugPages()
        {
            asi_overlay::Overlay::AddPage("Debug", "Info", [](asi_overlay::OverlayUI& ui)
            {
                ui.Value("Fusion Fix version", rsc_ProductVersion);
                ui.Value("Build", __DATE__ " " __TIME__);
                // What the plugin applied (graphics_api.h): the API asked for, the one running, why they differ
                auto graphics = FusionFixGraphicsApi::State();
                ui.Value("Graphics API", FusionFixGraphicsApi::Name(graphics.effective));
                if (graphics.requested != graphics.effective)
                    ui.Value("Graphics API asked for", std::string(FusionFixGraphicsApi::Name(graphics.requested)) + ": " + graphics.why);
                if (graphics.dxvk == FusionFixGraphicsApi::DxvkFile::Known || graphics.dxvk == FusionFixGraphicsApi::DxvkFile::Custom)
                    ui.Value("DXVK (vulkan.dll)", graphics.dxvk == FusionFixGraphicsApi::DxvkFile::Known ? "FusionFixHub's build" : "own build");
                if (graphics.driverLoaded)
                {
                    char caps[16];
                    sprintf_s(caps, "0x%X", graphics.driverCaps);
                    ui.Value("D3D9On12 capabilities", caps);
                }
                if (!graphics.wrapper.empty())
                    ui.Value("Third-party d3d9.dll", FusionFixGraphicsApi::detail::Narrow(graphics.wrapper));
                if (graphics.oldProxy)
                    ui.Value("Old Fusion Fix d3d9.dll", "bypassed");
                std::string resolution = "-";
                IDirect3DSurface9* backbuffer = nullptr;
                if (lastDevice && SUCCEEDED(lastDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer)))
                {
                    D3DSURFACE_DESC desc{};
                    backbuffer->GetDesc(&desc);
                    backbuffer->Release();
                    resolution = std::to_string(desc.Width) + " x " + std::to_string(desc.Height);
                }
                ui.Value("Resolution", resolution, "The back buffer's size");
            });
        }
    }

    void Present(IDirect3DDevice9* device)
    {
        if (!device) return;
        lastDevice = device;
        overlay.hotkey = SettingsPlugin().Int("MAIN.OverlayHotkey");
        auto language = FusionFixSettings.Get("PREF_CURRENT_LANGUAGE");
        overlay.fallbackLanguage = languages[language >= 0 && language < int32_t(std::size(languages)) ? language : 0];
        overlay.pretransformed = true; // GTA IV: fixed-function T&L outputs black, see OverlayD3D9.hpp
        PollPad();
        overlay.Present(device, host);
    }

    class Adapter
    {
    public:
        Adapter()
        {
            FusionFix::onInitEvent() += []
            {
                RegisterDebugPages();
                GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(&Present), &host);
                auto originals = IATHook::Replace(GetModuleHandleW(nullptr), "DINPUT8.DLL", std::forward_as_tuple("DirectInput8Create", DirectInputCreate));
                if (originals.contains("DirectInput8Create")) createDI = reinterpret_cast<CreateDI>(originals.at("DirectInput8Create").get());
                // Input devices may predate the callback that initializes the fix. Probe the system implementations
                // as well as intercepting future game devices. No acquisition or cooperative-mode changes needed.
                // GTA IV creates its (ANSI) devices before this runs, and the A and W interfaces can use different
                // method implementations: probe both.
                if (createDI)
                {
                    for (auto iid : { &IID_IDirectInput8A, &IID_IDirectInput8W })
                    {
                        IDirectInput8W* probe = nullptr; // A and W share the CreateDevice/Release layout
                        if (FAILED(DirectInputCreate(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, *iid, reinterpret_cast<void**>(&probe), nullptr)))
                            continue;
                        for (const auto& guid : { GUID_SysKeyboard, GUID_SysMouse })
                        {
                            IDirectInputDevice8W* device = nullptr;
                            if (SUCCEEDED(probe->CreateDevice(guid, &device, nullptr)))
                            {
                                mice.erase(device); // probe devices are released at once
                                device->Release();
                            }
                        }
                        probe->Release();
                    }
                }
                auto kernel = IATHook::Replace(GetModuleHandleW(nullptr), "KERNEL32.DLL", std::forward_as_tuple("GetProcAddress", ResolveProc));
                if (kernel.contains("GetProcAddress")) resolveProc = reinterpret_cast<decltype(resolveProc)>(kernel.at("GetProcAddress").get());
                auto user = IATHook::Replace(GetModuleHandleW(nullptr), "USER32.DLL", std::forward_as_tuple("GetKeyboardState", KeyboardState));
                if (user.contains("GetKeyboardState")) keyboardState = reinterpret_cast<decltype(keyboardState)>(user.at("GetKeyboardState").get());
            };

            // ImGui's default-pool resources go before every device Reset, also while the overlay is closed
            FusionFix::onBeforeReset() += []
            {
                overlay.BeforeReset();
            };

            FusionFix::onShutdownEvent() += []
            {
                if (attachedWindow) RemoveWindowSubclass(attachedWindow, WindowProc, subclassId);
            };
        }
    } adapter;
}
