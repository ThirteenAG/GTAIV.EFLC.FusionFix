module;

#include <common.hxx>

export module loadingdelays;

import common;
import comvars;
import framelimit;
import settings;

void WINAPI FusionSleep(DWORD dwMilliseconds)
{
    auto bMenuActive = CMenuManager::m_MenuActive && *CMenuManager::m_MenuActive;
    auto bLoadscreenActive = (CMenuManager::bLoadscreenShown && *CMenuManager::bLoadscreenShown) || bLoadingShown;

    if (!bMenuActive && bLoadscreenActive)
        return Sleep(0);

    return Sleep(dwMilliseconds);
}

namespace CRenderThreadInterface
{
    bool(__fastcall* IsLoadingRenderFunctionActive)(uint8_t* _this, void* edx);
    injector::hook_back<void(__fastcall*)(uint8_t*, void*, int)> hbFlush;
    bool bLoadingScreenAlreadyUp = false;

    // SetLoadingRenderFunction flushes the render thread before and after setting the loading screen, each flush waiting
    // for four render thread frames, also when the loading screen is already drawn. It's called every frame while the
    // screen is faded out, so these waits slow down the loading behind it.
    void __fastcall FlushBeforeLoadingScreen(uint8_t* _this, void* edx, int mode)
    {
        // Nothing changes when the active (+0xFE0) and the pending (+0xFD0) render functions are both the loading screen
        bLoadingScreenAlreadyUp = IsLoadingRenderFunctionActive(_this, edx) && std::memcmp(_this + 0xFE0, _this + 0xFD0, 16) == 0;
        if (!bLoadingScreenAlreadyUp)
            hbFlush.fun(_this, edx, mode);
    }

    void __fastcall FlushAfterLoadingScreen(uint8_t* _this, void* edx, int mode)
    {
        if (!bLoadingScreenAlreadyUp)
            hbFlush.fun(_this, edx, mode);
    }
}

class LoadingDelays
{
public:
    LoadingDelays()
    {
        FusionFix::onInitEvent() += []()
        {
            SettingsReader reader;

            // [MISC]
            auto bSkipLoadingSpinner = reader.ReadInteger("MISC", "SkipLoadingSpinner", 1) != 0;

            auto pattern = find_pattern("68 ? ? ? ? FF 15 ? ? ? ? 83 3D ? ? ? ? ? 75 ? E8 ? ? ? ? 8D 0C 24", "68 ? ? ? ? FF 15 ? ? ? ? 39 1D");
            injector::WriteMemory(pattern.get_first(1), 0, true);

            if (bSkipLoadingSpinner)
            {
                pattern = find_pattern("FF 35 ? ? ? ? EB ? FF 35 ? ? ? ? B9 ? ? ? ? E8 ? ? ? ? C6 44 24", "8B 0D ? ? ? ? 51 EB ? 8B 15 ? ? ? ? 52 B9 ? ? ? ? E8 ? ? ? ? C6 44 24");
                injector::MakeJMP(pattern.get_first(0), hook::get_pattern("6A ? 6A ? 6A ? 6A ? 6A ? 6A ? 6A ? 6A ? 6A ? 6A ? 6A ? 6A ? 6A ? 6A ? B9 ? ? ? ? C6 46"));
            }

            // 1.0.7.0 and 1.1.2.0 don't flush in SetLoadingRenderFunction
            pattern = hook::pattern("6A 00 B9 ? ? ? ? E8 ? ? ? ? 6A 04 8D 44 24 0C 50 6A 00 68 ? ? ? ? 8D 4C 24 1C");
            if (!pattern.empty())
            {
                CRenderThreadInterface::IsLoadingRenderFunctionActive = (decltype(CRenderThreadInterface::IsLoadingRenderFunctionActive))hook::get_pattern("83 EC 14 56 6A 04 8D 44 24 08 50 6A 00 8B F1 68");
                CRenderThreadInterface::hbFlush.fun = injector::MakeCALL(pattern.get_first(7), CRenderThreadInterface::FlushBeforeLoadingScreen, true).get();
                pattern = hook::pattern("6A 00 B9 ? ? ? ? E8 ? ? ? ? 5E 83 C4 28 C3");
                injector::MakeCALL(pattern.get_first(7), CRenderThreadInterface::FlushAfterLoadingScreen, true);
            }
        };

        FusionFix::onInitEvent() += []()
        {
            IATHook::Replace(GetModuleHandleA(NULL), "kernel32.DLL", std::forward_as_tuple("Sleep", FusionSleep));
        };
    }
} LoadingDelays;