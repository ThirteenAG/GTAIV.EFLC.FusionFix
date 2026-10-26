newoption {
    trigger     = "with-version",
    value       = "STRING",
    description = "Current version",
}

-- The folder a project is deployed to, and the game it is started from when debugging,
-- is the path of one machine and does not belong in the repository. It is read from a
-- `.env` file next to this script, which is not tracked by git and holds one
-- `<KEY>=<folder>` line per game (quotes and a trailing slash are optional), see the
-- readme. A project whose key is missing is not deployed at all.
local envkeys = nil
function envdir(key)
   if not envkeys then
      envkeys = {}
      local text = io.readfile(path.join(_SCRIPT_DIR, ".env")) or ""
      for line in text:gmatch("[^\r\n]+") do
         local k, v = line:match("^%s*([%w_]+)%s*=%s*(.-)%s*$")
         if k and v ~= "" then
            v = v:gsub('^"', ""):gsub('"$', ""):gsub("^'", ""):gsub("'$", "")
            envkeys[k] = v
         end
      end
   end

   local value = envkeys[key]
   if not value then return nil end

   value = value:gsub("[%s\\/]+$", "")
   if value == "" then return nil end

   return path.translate(value)
end

-- The pre-build step writing BuildTime.h of a project that embeds Versioninfo.rc: a new time only when one of `inputs`
-- (files or folders, relative to the repository) is newer than the project's target, so a build without changes
-- doesn't link again
function buildtime(inputs)
   local args = '"$(MSBuildProjectFullPath)"'
   for _, i in ipairs(inputs) do args = args .. ' "../' .. i .. '"' end
   prebuildcommands { 'powershell -NoProfile -ExecutionPolicy Bypass -File "../tools/buildtime.ps1" -Out "../build/generated/%{prj.name}/%{cfg.buildcfg}/BuildTime.h" -Target "$(TargetPath)" ' .. args }
end

workspace "GTAIV.EFLC.FusionFix"
   configurations { "Release", "Debug" }
   architecture "x86"
   location "build"
   -- BuildTimeUtc (Versioninfo.rc): the build's own time in UTC, written by tools/buildtime.ps1 before a build that
   -- links (see buildtime() below); the in-game update check compares it with the release asset's time (FileVersion is
   -- only shown)
   includedirs { "build/generated/%{prj.name}/%{cfg.buildcfg}" }
   resincludedirs { "build/generated/%{prj.name}/%{cfg.buildcfg}" }
   cppdialect "C++latest"
   kind "SharedLib"
   language "C++"
   targetdir "bin/%{cfg.buildcfg}"
   targetextension ".asi"
   buildoptions { "/dxifcInlineFunctions- /Zc:__cplusplus /utf-8" }
   staticruntime "On"
   characterset ("Unicode")
   multiprocessorcompile ("On")
   startproject "GTAIV.EFLC.FusionFix"

   defines { "rsc_CompanyName=\"GTAIV.EFLC.FusionFix\"" }
   defines { "rsc_LegalCopyright=\"GPL-3.0 license\""}
   defines { "rsc_InternalName=\"%{prj.name}\"", "rsc_ProductName=\"%{prj.name}\"", "rsc_OriginalFilename=\"%{cfg.buildtarget.name}\"" }
   defines { "rsc_FileDescription=\"GTAIV.EFLC.FusionFix\"" }
   defines { "rsc_UpdateUrl=\"https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix\"" }

   local major = os.date("%d")
   local minor = os.date("%m")
   local build = os.date("%Y")
   local revision = os.date("%H") .. os.date("%M")

   if _OPTIONS["with-version"] then
      local t = {}
      for i in _OPTIONS["with-version"]:gmatch("([^.]+)") do
         t[#t + 1], _ = i:gsub("%D+", "")
      end
      while #t < 4 do t[#t + 1] = 0 end
      major    = math.min(tonumber(t[1]), 255)
      minor    = math.min(tonumber(t[2]), 255)
      build    = math.min(tonumber(t[3]), 65535)
      revision = math.min(tonumber(t[4]), 65535)
   end

   local githash = ""
   local f = io.popen("git rev-parse --short HEAD")
   if f then
      githash = f:read("*a"):gsub("%s+", "")
      f:close()
   end

   local productVersion = major .. "." .. minor .. "." .. build .. "." .. revision
   if githash ~= "" then
      productVersion = productVersion .. "-" .. githash
   end

   defines { "rsc_FileVersion_MAJOR=" .. major }
   defines { "rsc_FileVersion_MINOR=" .. minor }
   defines { "rsc_FileVersion_BUILD=" .. build }
   defines { "rsc_FileVersion_REVISION=" .. revision }
   defines { "rsc_FileVersion=\"" .. major .. "." .. minor .. "." .. build .. "\"" }
   defines { "rsc_ProductVersion=\"" .. productVersion .. "\"" }
   defines { "rsc_GitSHA1=\"" .. githash .. "\"" }
   defines { "rsc_GitSHA1W=L\"" .. githash .. "\"" }

   defines { "_CRT_SECURE_NO_WARNINGS" }

   includedirs { "source" }
   -- Shared headers, ASI Settings, overlay, automation and the upscaler bridge's client: FusionFixUtils, a submodule
   -- (change them there)
   includedirs { "external/FusionFixUtils/include", "external/FusionFixUtils" }
   includedirs { "source/ledsdk" }
   includedirs { "source/dxsdk" }
   -- In-game settings overlay: the overlay of FusionFixUtils; ImGui is the imgui submodule of FusionFixUtils.
   includedirs { "external/FusionFixUtils/imgui" }
   files { "external/FusionFixUtils/overlay/*.hpp", "external/FusionFixUtils/imgui/*.h" }
   files { "external/FusionFixUtils/imgui/imgui.cpp", "external/FusionFixUtils/imgui/imgui_draw.cpp", "external/FusionFixUtils/imgui/imgui_tables.cpp", "external/FusionFixUtils/imgui/imgui_widgets.cpp" }
   files { "external/FusionFixUtils/imgui/backends/imgui_impl_win32.cpp", "external/FusionFixUtils/imgui/backends/imgui_impl_dx9.cpp" }
   links { "comctl32", "dxguid" }
   libdirs { "source/ledsdk" }
   libdirs { "source/dxsdk" }
   files { "source/**.h", "source/*.hpp", "source/*.cpp", "source/*.hxx", "source/**.ixx" }
   files { "source/resources/Versioninfo.rc" }
   files { "source/resources/Shaders.rc" }
   files { "source/resources/snow/*.rc" }
   files { "source/resources/temporal/*.rc" }
   files { "external/FusionFixUtils/aa/AntiAliasing.rc", "external/FusionFixUtils/aa/*.hpp" }
   files { "source/resources/hdr/*.rc" }
   -- ASI Settings: the description and translations of the settings, embedded by source/resources/Settings.rc
   files { "source/resources/Settings.rc" }
   files { "settings/*.json" }
   -- The fonts' character tables, for text of the descriptions shown in the menu
   files { "source/resources/GameFont.rc" }
   links { "LogitechLEDLib.lib" }

   includedirs { "external/injector/safetyhook/include" }
   includedirs { "external/injector/zydis" }
   includedirs { "external/hooking" }
   includedirs { "external/injector/include" }
   includedirs { "external/inireader" }
   includedirs { "external/Vulkan-Headers/include" }
   includedirs { "source/gxt/src" }
   files { "external/hooking/Hooking.Patterns.h", "external/hooking/Hooking.Patterns.cpp" }
   files { "external/injector/safetyhook/include/**.hpp", "external/injector/safetyhook/src/**.cpp" }
   files { "external/injector/zydis/**.h", "external/injector/zydis/**.c" }
   files { "source/gxt/src/**.h", "source/gxt/src/**.cpp" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

project "GTAIV.EFLC.FusionFix"
   targetdir "bin"

   -- A broken settings description fails the build, before compiling and in the built plugin
   dependson { "AsiSettingsValidator" }
   prebuildcommands { '"../bin/tools/asi-settings-validate.exe" "../settings/GTAIV.EFLC.FusionFix.json" "../settings/GTAIV.EFLC.FusionFix.en.json" "../settings/GTAIV.EFLC.FusionFix.fr.json" "../settings/GTAIV.EFLC.FusionFix.de.json" "../settings/GTAIV.EFLC.FusionFix.it.json" "../settings/GTAIV.EFLC.FusionFix.es.json" "../settings/GTAIV.EFLC.FusionFix.ru.json" "../settings/GTAIV.EFLC.FusionFix.ja.json"' }
   postbuildcommands { '"../bin/tools/asi-settings-validate.exe" "$(TargetPath)"' }

   -- HLSL shaders compiled with fxc: the table of them is in tools/compileshaders.ps1. The compiled shaders are
   -- committed with source/resources/shaders.lock (those of FusionFixUtils in its aa/shaders.lock); only the ones whose
   -- source, includes or fxc arguments changed compile, in parallel
   prebuildcommands { 'powershell -NoProfile -ExecutionPolicy Bypass -File "../tools/compileshaders.ps1"' }
   buildtime { "source", "external", "settings" }

   local gamedir = envdir("GTAIV_DIR")
   if gamedir then
      local plugins = gamedir .. "\\plugins"
      -- Only the built .asi is deployed: the contents of `data` are put in place by the
      -- hub installer, and a build copying them over a game folder would overwrite the files
      -- of an installed mod with the ones of the working tree. A plugin that is already
      -- installed is replaced, a folder that does not have one is left alone.
      postbuildcommands {
         "if exist \"" .. plugins .. "\\$(TargetFileName)\" copy /y \"$(TargetPath)\" \"" .. plugins .. "\\\"",
      }
      debugdir (gamedir)
      debugcommand (gamedir .. "\\GTAIV.exe")
   end

-- Checks the ASI Settings description of the plugin (settings/*.json, external/FusionFixUtils/asi_settings)
project "AsiSettingsValidator"
   kind "ConsoleApp"
   targetdir "bin/tools"
   targetname "asi-settings-validate"
   targetextension ".exe"

   removefiles { "source/**" }
   removefiles { "external/**" }
   removefiles { "settings/**" }
   files { "external/FusionFixUtils/asi_settings/tools/validator.cpp" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"


-- d3d9.dll: a pass-through to the system's d3d9.dll, shipped only so that unpacking a release over an old one replaces
-- the old graphics-API proxy (the plugin applies the graphics API itself, FusionFixUtils upscaler/graphics_api.h)
project "d3d9"
   language "C++"
   targetdir "bin"
   targetextension ".dll"
   staticruntime "On"
   removedefines { 'rsc_FileDescription="GTAIV.EFLC.FusionFix"' }
   defines { 'rsc_FileDescription="Fusion Fix pass-through d3d9.dll"' }

   removefiles { "source/**" }
   removefiles { "external/**" }
   files { "source/d3d9/d3d9.def" }
   files { "source/d3d9/d3d9.cpp" }
   files { "source/resources/Versioninfo.rc" }
   buildtime { "source/d3d9", "source/resources/Versioninfo.rc", "source/resources/VersionInfo.h" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

-- The dev-only automation adapters (zz_Automation.asi) live in FusionFixHelpers, next to this checkout; they use the
-- frame-stage export of this plugin (asi_automation_frames.h), which is inert without them.
