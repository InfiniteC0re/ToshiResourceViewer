project "ImGui"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "on"
	characterset "ASCII"

	files
	{
		"**.cpp",
		"**.h",
	}
	
	libdirs
	{
		"%{LibDir.sdl2}"
	}

	includedirs
	{
		"%{IncludeDir.sdl2}"
	}

	filter "configurations:Debug"
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		runtime "Release"
		optimize "On"

	filter "configurations:Final"
		runtime "Release"
		optimize "On"