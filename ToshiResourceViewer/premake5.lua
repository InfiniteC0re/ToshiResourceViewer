project "ToshiResourceViewer"
	kind "ConsoleApp"
	language "C++"
	
	pchheader "pch.h"
	pchsource "Source/pch.cpp"
	
	links
	{
		"Toshi",
		"ImGui",
		"SDL2.lib",
		"opengl32.lib",
		"glew32s.lib"
	}
	
	files
	{
		"Source/**.h",
		"Source/**.cpp",
		"Source/**.c",
	}
			
	libdirs
	{
		"%{LibDir.sdl2}",
		"%{LibDir.glew}",
		"%{LibDir.assimp}",
	}

	includedirs
	{
		"Source",
		"%{IncludeDir.toshi}",
		"%{IncludeDir.sdl2}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.glew}",
		"%{IncludeDir.imgui}",
		"%{IncludeDir.assimp}",
	}

	defines
	{
		"TOSHI_CONSOLE",
		"SDL_MAIN_HANDLED"
	}
	
	filter "configurations:Debug"
		links { "assimp-vc143-mtd.lib" }

	filter "configurations:Release"
		links { "assimp-vc143-mt.lib" }

	filter "configurations:Final"
		links { "assimp-vc143-mt.lib" }

