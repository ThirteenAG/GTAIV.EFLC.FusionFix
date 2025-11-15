newoption {
    trigger     = "with-version",
    value       = "STRING",
    description = "Current version",
    default     = "4.0",
}

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
   flags { "MultiProcessorCompile" }

   defines { "rsc_CompanyName=\"GTAIV.EFLC.FusionFix\"" }
   defines { "rsc_LegalCopyright=\"GTAIV.EFLC.FusionFix\""}
   defines { "rsc_InternalName=\"%{prj.name}\"", "rsc_ProductName=\"%{prj.name}\"", "rsc_OriginalFilename=\"%{prj.name}.dll\"" }
   defines { "rsc_FileDescription=\"GTAIV.EFLC.FusionFix\"" }
   defines { "rsc_UpdateUrl=\"https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix\"" }

   local major = 1
   local minor = 0
   local build = 0
   local revision = 0
   if(_OPTIONS["with-version"]) then
     local t = {}
     for i in _OPTIONS["with-version"]:gmatch("([^.]+)") do
       t[#t + 1], _ = i:gsub("%D+", "")
     end
     while #t < 4 do t[#t + 1] = 0 end
     major = math.min(tonumber(t[1]), 255)
     minor = math.min(tonumber(t[2]), 255)
     build = math.min(tonumber(t[3]), 65535)
     revision = math.min(tonumber(t[4]), 65535)
   end
   defines { "rsc_FileVersion_MAJOR=" .. major }
   defines { "rsc_FileVersion_MINOR=" .. minor }
   defines { "rsc_FileVersion_BUILD=" .. build }
   defines { "rsc_FileVersion_REVISION=" .. revision }
   defines { "rsc_FileVersion=\"" .. major .. "." .. minor .. "." .. build .. "\"" }
   defines { "rsc_ProductVersion=\"" .. major .. "." .. minor .. "." .. build .. "\"" }

   local githash = ""
   local f = io.popen("git rev-parse --short HEAD")
   if f then
      githash = f:read("*a"):gsub("%s+", "")
      f:close()
   end

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
   links { "LogitechLEDLib.lib" }

   includedirs { "external/injector/safetyhook/include" }
   includedirs { "external/injector/zydis" }
   includedirs { "external/hooking" }
   includedirs { "external/injector/include" }
   includedirs { "external/inireader" }
   includedirs { "external/modupdater/dist" }
   includedirs { "source/gxt/src" }
   libdirs { "external/modupdater/dist" }
   files { "external/hooking/Hooking.Patterns.h", "external/hooking/Hooking.Patterns.cpp" }
   files { "external/injector/safetyhook/include/**.hpp", "external/injector/safetyhook/src/**.cpp" }
   files { "external/injector/zydis/**.h", "external/injector/zydis/**.c" }
   files { "source/gxt/src/**.h", "source/gxt/src/**.cpp" }

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
   }

   pbcommands = {
      "setlocal EnableDelayedExpansion",
      --"set \"path=" .. (gamepath) .. "\"",
      "set file=$(TargetPath)",
      "FOR %%i IN (\"%file%\") DO (",
      "set filename=%%~ni",
      "set fileextension=%%~xi",
      "set target=!path!!filename!!fileextension!",
      "if exist \"!target!\" copy /y \"!file!\" \"!target!\"",
      ")" }

   function setpaths (gamepath, exepath, scriptspath)
      scriptspath = scriptspath or "scripts/"
      if (gamepath) then
         cmdcopy = { "set \"path=" .. gamepath .. scriptspath .. "\"" }
         table.insert(cmdcopy, pbcommands)
         postbuildcommands (cmdcopy)
         debugdir (gamepath)
         if (exepath) then
            debugcommand (gamepath .. exepath)
            dir, file = exepath:match'(.*/)(.*)'
            debugdir (gamepath .. (dir or ""))
         end
      end
      targetdir ("bin")
   end

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"
      links { "libmodupdater_debug_win32.lib" }

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"
      links { "libmodupdater_release_win32.lib" }

project "GTAIV.EFLC.FusionFix"
   setpaths("H:/SteamLibrary/steamapps/common/Grand Theft Auto IV/GTAIV/", "GTAIV.exe", "plugins/")

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
