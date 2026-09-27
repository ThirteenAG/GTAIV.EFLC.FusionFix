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

workspace "GTAIV.EFLC.FusionFix"
   configurations { "Release", "Debug" }
   architecture "x86"
   location "build"
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
   includedirs { "source/includes" }
   includedirs { "source/ledsdk" }
   includedirs { "source/dxsdk" }
   libdirs { "source/ledsdk" }
   libdirs { "source/dxsdk" }
   files { "source/**.h", "source/*.hpp", "source/*.cpp", "source/*.hxx", "source/**.ixx" }
   files { "source/resources/Versioninfo.rc" }
   files { "source/resources/Shaders.rc" }
   files { "source/resources/LODLights.rc" }
   files { "source/resources/snow/*.rc" }
   files { "source/resources/temporal/*.rc" }
   files { "source/resources/hdr/*.rc" }
   links { "LogitechLEDLib.lib" }

   includedirs { "external/injector/safetyhook/include" }
   includedirs { "external/injector/zydis" }
   includedirs { "external/hooking" }
   includedirs { "external/injector/include" }
   includedirs { "external/inireader" }
   includedirs { "external/Vulkan-Headers/include" }
   includedirs { "external/modupdater/dist" }
   includedirs { "source/gxt/src" }
   libdirs { "external/modupdater/dist" }
   files { "external/hooking/Hooking.Patterns.h", "external/hooking/Hooking.Patterns.cpp" }
   files { "external/injector/safetyhook/include/**.hpp", "external/injector/safetyhook/src/**.cpp" }
   files { "external/injector/zydis/**.h", "external/injector/zydis/**.c" }
   files { "source/gxt/src/**.h", "source/gxt/src/**.cpp" }
   files { "data/plugins/*.ini" }

   os.mkdir("shaders/external/gamma/asm")
   os.mkdir("source/resources/shaders/win32_30")

   -- Compile hlsl shaders to .fxo (Output to "../source/resources/shaders/win32_30"). Assembly variants are still output for convenience to "../shaders/external/%shadername%/asm".
   prebuildcommands {
      -- ConsoleGamma
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T fx_2_0 /nologo /Fo \"../source/resources/shaders/win32_30/ConsoleGamma.fxo\" /Fc \"../shaders/external/gamma/asm/ConsoleGamma.asm\" \"../shaders/external/gamma/fx/ConsoleGamma.fx\"",
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T fx_2_0 /nologo /Fo \"../source/resources/shaders/win32_30/ConsoleGamma_Dither.fxo\" /Fc \"../shaders/external/gamma/asm/ConsoleGamma_Dither.asm\" \"../shaders/external/gamma/fx/ConsoleGamma_Dither.fx\"",
    }

   prebuildcommands {
      "for /R \"../source/resources/snow/\" %%f in (*.ps) do (\"../source/dxsdk/lib/x86/fxc.exe\" /T ps_3_0 /nologo /E main /Fo \"../source/resources/snow/%%~nfps.pso\" %%f)",
      "for /R \"../source/resources/snow/\" %%f in (*.vs) do (\"../source/dxsdk/lib/x86/fxc.exe\" /T vs_3_0 /nologo /E main /Fo \"../source/resources/snow/%%~nfvs.vso\" %%f)",

      -- Compile HLSL shaders to .pso/.vso (output to ../source/resources/)
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T ps_3_0 /nologo /E SSDraw /Fo \"../source/resources/SSDraw_PS.pso\" \"../source/resources/SunShafts_PS.hlsl\"",
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T ps_3_0 /nologo /E SSPrepass /Fo \"../source/resources/SSPrepass_PS.pso\" \"../source/resources/SunShafts_PS.hlsl\"",
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T ps_3_0 /nologo /E SSAdd /Fo \"../source/resources/SSAdd_PS.pso\" \"../source/resources/SunShafts_PS.hlsl\"",
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T ps_3_0 /nologo /E ApplyFXAA /Fo \"../source/resources/FxaaPS.pso\" \"../source/resources/FXAA3_11.h\"",
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T ps_3_0 /nologo /E DX9_SMAALumaEdgeDetectionPS /Fo \"../source/resources/SMAA_EdgeDetection.pso\" \"../source/resources/SMAA.hlsl\"",
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T ps_3_0 /nologo /E DX9_SMAABlendingWeightCalculationPS /Fo \"../source/resources/SMAA_BlendingWeightsCalculation.pso\" \"../source/resources/SMAA.hlsl\"",
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T ps_3_0 /nologo /E DX9_SMAANeighborhoodBlendingPS /Fo \"../source/resources/SMAA_NeighborhoodBlending.pso\" \"../source/resources/SMAA.hlsl\"",
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T vs_3_0 /nologo /E DX9_SMAAEdgeDetectionVS /Fo \"../source/resources/SMAA_EdgeDetectionVS.vso\" \"../source/resources/SMAA.hlsl\"",
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T vs_3_0 /nologo /E DX9_SMAABlendingWeightCalculationVS /Fo \"../source/resources/SMAA_BlendingWeightsCalculationVS.vso\" \"../source/resources/SMAA.hlsl\"",
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T vs_3_0 /nologo /E DX9_SMAANeighborhoodBlendingVS /Fo \"../source/resources/SMAA_NeighborhoodBlendingVS.vso\" \"../source/resources/SMAA.hlsl\"",

      -- Temporal anti-aliasing
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T vs_3_0 /nologo /E VS_VelocityRigid /Fo \"../source/resources/temporal/VelocityRigid.vso\" \"../source/resources/temporal/Temporal.hlsl\"",
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T vs_3_0 /nologo /E VS_VelocitySkinned /Fo \"../source/resources/temporal/VelocitySkinned.vso\" \"../source/resources/temporal/Temporal.hlsl\"",
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T ps_3_0 /nologo /E PS_Velocity /Fo \"../source/resources/temporal/Velocity.pso\" \"../source/resources/temporal/Temporal.hlsl\"",
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T vs_3_0 /nologo /E VS_BoneWrite /Fo \"../source/resources/temporal/BoneWrite.vso\" \"../source/resources/temporal/Temporal.hlsl\"",
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T ps_3_0 /nologo /E PS_BoneWrite /Fo \"../source/resources/temporal/BoneWrite.pso\" \"../source/resources/temporal/Temporal.hlsl\"",
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T ps_3_0 /nologo /E PS_CameraMotion /Fo \"../source/resources/temporal/CameraMotion.pso\" \"../source/resources/temporal/Temporal.hlsl\"",
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T ps_3_0 /nologo /E PS_TemporalResolve /Fo \"../source/resources/temporal/TemporalResolve.pso\" \"../source/resources/temporal/Temporal.hlsl\"",
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T ps_3_0 /nologo /E PS_UpscalerDepth /Fo \"../source/resources/temporal/UpscalerDepth.pso\" \"../source/resources/temporal/Temporal.hlsl\"",
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T ps_3_0 /nologo /E PS_OpaqueLuma /Fo \"../source/resources/temporal/OpaqueLuma.pso\" \"../source/resources/temporal/Temporal.hlsl\"",
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T ps_3_0 /nologo /E PS_Reactive /Fo \"../source/resources/temporal/Reactive.pso\" \"../source/resources/temporal/Temporal.hlsl\"",

      -- HDR output
      "\"../source/dxsdk/lib/x86/fxc.exe\" /T ps_3_0 /nologo /E PS_HDROutput /Fo \"../source/resources/hdr/HDROutput.pso\" \"../source/resources/hdr/HDR.hlsl\"",
   }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"
      links { "libmodupdater_debug_win32.lib" }

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"
      links { "libmodupdater_release_win32.lib" }

project "GTAIV.EFLC.FusionFix"
   targetdir "bin"

   local gamedir = envdir("GTAIV_DIR")
   if gamedir then
      local plugins = gamedir .. "\\plugins"
      -- Only the built .asi is deployed: the contents of `data` are put in place by the
      -- installer, and a build copying them over a game folder would overwrite the files
      -- of an installed mod with the ones of the working tree. A plugin that is already
      -- installed is replaced, a folder that does not have one is left alone.
      postbuildcommands {
         "if exist \"" .. plugins .. "\\$(TargetFileName)\" copy /y \"$(TargetPath)\" \"" .. plugins .. "\\\"",
      }
      debugdir (gamedir)
      debugcommand (gamedir .. "\\GTAIV.exe")
   end

project "GTAIV.EFLC.FusionFixInstaller"
   kind "WindowedApp"
   language "C++"
   targetdir "bin/%{cfg.buildcfg}"
   targetextension ".exe"
   staticruntime "On"

   files { "installer/*.rc" }
   files { "installer/main.cpp" }
   removefiles { "source/**" }
   removefiles { "external/**" }
   files { "source/resources/Versioninfo.rc" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

project "d3d9"
   language "C++"
   targetdir "bin"
   targetextension ".dll"
   staticruntime "On"

   removefiles { "source/**" }
   removefiles { "external/**" }
   files { "source/d3d9/d3d9.def" }
   files { "source/d3d9/d3d9.cpp" }
   files { "source/resources/Versioninfo.rc" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

-- 64-bit helper running NVIDIA DLSS and AMD FSR (GTAIV.EFLC.FusionFix.exe, next to the plugin).
-- Visual Studio solutions cannot mix platforms, so it has a solution of its own:
-- build/GTAIV.EFLC.FusionFix.Helper.slnx
workspace "GTAIV.EFLC.FusionFix.Helper"
   configurations { "Release", "Debug" }
   architecture "x86_64"
   location "build"
   cppdialect "C++latest"
   language "C++"
   characterset ("Unicode")
   staticruntime "On"
   -- The NGX static loader is built against the release runtime
   runtime "Release"
   multiprocessorcompile ("On")
   startproject "GTAIV.EFLC.FusionFix.Helper"

   defines { "rsc_CompanyName=\"GTAIV.EFLC.FusionFix\"" }
   defines { "rsc_LegalCopyright=\"GPL-3.0 license\""}
   defines { "rsc_InternalName=\"%{prj.name}\"", "rsc_ProductName=\"%{prj.name}\"", "rsc_OriginalFilename=\"%{cfg.buildtarget.name}\"" }
   defines { "rsc_FileDescription=\"GTAIV.EFLC.FusionFix upscaler helper\"" }
   defines { "rsc_UpdateUrl=\"https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix\"" }
   defines { "rsc_FileVersion_MAJOR=" .. major }
   defines { "rsc_FileVersion_MINOR=" .. minor }
   defines { "rsc_FileVersion_BUILD=" .. build }
   defines { "rsc_FileVersion_REVISION=" .. revision }
   defines { "rsc_FileVersion=\"" .. major .. "." .. minor .. "." .. build .. "\"" }
   defines { "rsc_ProductVersion=\"" .. productVersion .. "\"" }
   defines { "rsc_GitSHA1=\"" .. githash .. "\"" }
   defines { "rsc_GitSHA1W=L\"" .. githash .. "\"" }
   defines { "_CRT_SECURE_NO_WARNINGS", "_WINDOWS" }

project "GTAIV.EFLC.FusionFix.Helper"
   kind "WindowedApp"
   targetdir "bin"
   targetname "GTAIV.EFLC.FusionFix"
   targetextension ".exe"
   -- Must not overwrite the plugin's GTAIV.EFLC.FusionFix.pdb in the same folder
   symbolspath "$(OutDir)GTAIV.EFLC.FusionFix.Helper.pdb"

   files { "source/helper/*.cpp" }
   files { "source/includes/upscaler_protocol.hpp" }
   files { "source/resources/Versioninfo.rc" }

   includedirs { "source/includes" }
   includedirs { "external/dlss/include" }
   includedirs { "external/fidelityfx/api/include" }
   includedirs { "external/fidelityfx/upscalers/include" }
   libdirs { "external/dlss/lib/Windows_x86_64/x64" }
   links { "d3d12", "dxgi", "shell32", "nvsdk_ngx_s" }

   local gamedir = envdir("GTAIV_DIR")
   if gamedir then
      local plugins = gamedir .. "\\plugins"
      -- Deployed next to an installed plugin, like the plugin itself
      postbuildcommands {
         "if exist \"" .. plugins .. "\\GTAIV.EFLC.FusionFix.asi\" copy /y \"$(TargetPath)\" \"" .. plugins .. "\\\"",
      }
   end

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"
      symbols "On"
