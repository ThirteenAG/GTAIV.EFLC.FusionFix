workspace "gxt"
	configurations
	{
		"Debug",
		"Release",
	}

	location "build"

project "gxt"
	kind "ConsoleApp"
	language "C++"
	targetname "gxt"
	targetdir "bin/%{cfg.buildcfg}"
	cppdialect "C++17"
	targetextension ".exe"

	includedirs { "src" }

	files { "src/common.h" }
	files { "src/ContainerBase.h" }
	files { "src/ConversionTable.cpp" }
	files { "src/ConversionTable.h" }
	files { "src/DataContainer.h" }
	files { "src/Hash.cpp" }
	files { "src/Hash.h" }
	files { "src/Hash_CRC32.cpp" }
	files { "src/Hash_CRC32.h" }
	files { "src/Hash_Jenkins.cpp" }
	files { "src/Hash_Jenkins.h" }
	files { "src/KeyChar.h" }
	files { "src/KeyContainer.h" }
	files { "src/main.cpp" }
	files { "src/StringContainer.cpp" }
	files { "src/StringContainer.h" }
	files { "src/TableData.h" }
	files { "src/umap.h" }
	files { "src/utils.h" }
	files { "src/utils_win32.cpp" }

	characterset ("Unicode")
	toolset ("v141_xp")

	links { "legacy_stdio_definitions" }

	filter "configurations:Debug"
		defines { "_DEBUG" }
		symbols "full"
		optimize "off"
		runtime "debug"
		editAndContinue "off"
		flags { "NoIncrementalLink" }
		staticruntime "on"

	filter "configurations:Release"
		defines { "NDEBUG" }
		symbols "on"
		optimize "speed"
		runtime "release"
		staticruntime "on"
		flags { "LinkTimeOptimization" }
		linkoptions { "/OPT:NOICF" }

