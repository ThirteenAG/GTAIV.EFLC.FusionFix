module;

#include <common.hxx>
#include <asi_settings/asi_settings.hpp>
#include <upscaler/graphics_api.h>

export module graphicsapi;

import common;
import settings;

// The graphics API, MAIN.GraphicsAPI: 0 DirectX 9, 1 Vulkan (DXVK as vulkan.dll next to the exe), 2 DirectX 12 (Fusion
// Fix's D3D9On12 driver next to the exe). Fusion Fix no longer has a d3d9.dll proxy for it and reads no d3d9.cfg: the
// plugin applies it itself with source/upscaler/graphics_api.h (which explains how), from Init() in
// dllmain.cpp, the first thing it does. That runs when the Ultimate ASI Loader starts the plugins at the game's entry
// point (or, for exes that unpack themselves, as soon as the code is unpacked), long before WinMain creates the window
// and calls Direct3DCreate9 (rage's device setup). The d3d9.dll in the package is a pass-through placeholder that only
// replaces the old proxy of earlier releases (source/d3d9/d3d9.cpp); an old proxy still in the folder is bypassed, a
// third-party d3d9.dll (ReShade, ENB, dxwrapper) stays on top of the API.
//
// The crash guard's marker (GTAIV.EFLC.FusionFix.graphics-api-start) and the log (GTAIV.EFLC.FusionFix.graphics.log)
// are next to the settings file. When the guard falls back, MAIN.GraphicsAPI is set back to 0 through the settings
// (ASI Settings Set, which saves the cfg) and a message box says so before the device is created.

std::ofstream& LogFile()
{
    static std::ofstream file;
    return file;
}

export void InstallGraphicsApi()
{
    static std::once_flag once;
    std::call_once(once, []()
    {
        auto& plugin = SettingsPlugin();
        FusionFixGraphicsApi::Options options;
        // The player's choice as stored: State() tells why another API runs
        int32_t wanted = 0;
        asi_settings::detail::ParseInt(plugin.Wanted("MAIN.GraphicsAPI"), wanted);
        options.api = static_cast<FusionFixGraphicsApi::Api>(std::clamp(wanted, 0, 2));
        if (!plugin.CfgPath().empty())
        {
            options.markerFolder = plugin.CfgPath().parent_path();
            options.markerName = L"GTAIV.EFLC.FusionFix";
            LogFile().open(options.markerFolder / L"GTAIV.EFLC.FusionFix.graphics.log", std::ios::trunc);
            // The markers of the graphics-API proxy of development builds (before the plugin did it): such a proxy still in
            // the folder is bypassed, so its marker would never be cleared and its next start would set the API back
            std::error_code ec;
            for (auto name : { L"GTAIV.EFLC.FusionFix.vulkan-start", L"GTAIV.EFLC.FusionFix.d3d9on12-start" })
                std::filesystem::remove(options.markerFolder / name, ec);
        }
        options.callbacks.log = [](const char* line)
        {
            // also written from threads other than the render thread (the upscaler's fence watch)
            static std::mutex lock;
            std::lock_guard<std::mutex> guard(lock);
            auto& log = LogFile();
            if (!log.is_open())
                return;
            SYSTEMTIME now;
            GetLocalTime(&now);
            char stamp[32];
            sprintf_s(stamp, "%02u:%02u:%02u.%03u ", now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
            log << stamp << line << std::endl;
        };
        options.callbacks.setApiSetting = [](FusionFixGraphicsApi::Api api)
        {
            SettingsPlugin().Set("MAIN.GraphicsAPI", std::to_string(static_cast<int>(api)), true);
            // The game starts on this value: the menu doesn't ask for a restart
            CSettings::RestartBaseline("MAIN.GraphicsAPI");
        };
        FusionFixGraphicsApi::Install(options);
    });
}
