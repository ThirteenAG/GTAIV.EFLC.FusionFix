newoption {
    trigger     = "with-version",
    value       = "STRING",
    description = "Current version",
    default     = "1.0",
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
   buildoptions { "/dxifcInlineFunctions-" }
   
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

   defines { "_CRT_SECURE_NO_WARNINGS" }
   defines { "ZYDIS_STATIC_BUILD" }

   includedirs { "source" }
   includedirs { "source/includes" }
   includedirs { "source/ledsdk" }
   includedirs { "source/dxsdk" }
   includedirs { "source/includes/safetyhook" }
   libdirs { "source/ledsdk" }
   libdirs { "source/dxsdk" }
   files { "source/*.h", "source/*.hpp", "source/*.cpp", "source/*.hxx", "source/*.ixx" }
   files { "source/includes/safetyhook/*.h", "source/includes/safetyhook/*.hpp" }
   files { "source/includes/safetyhook/*.c", "source/includes/safetyhook/*.cpp" }
   files { "source/resources/Versioninfo.rc" }
   links { "LogitechLEDLib.lib" }
   
   includedirs { "external/hooking" }
   includedirs { "external/injector/include" }
   includedirs { "external/inireader" }
   includedirs { "external/minhook/include" }
   files { "external/minhook/include/*.*", "external/minhook/src/*.*", "external/minhook/src/**/*.*" }
   files { "external/hooking/Hooking.Patterns.h", "external/hooking/Hooking.Patterns.cpp" }
   
   characterset ("Unicode")
   
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

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"
      staticruntime "On"
      
project "GTAIV.EFLC.FusionFix"
   setpaths("H:/SteamLibrary/steamapps/common/Grand Theft Auto IV/GTAIV/", "GTAIV.exe", "plugins/")