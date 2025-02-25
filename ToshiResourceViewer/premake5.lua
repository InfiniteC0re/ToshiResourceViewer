project "ToshiResourceViewer"
	kind "ConsoleApp"
	language "C++"
	staticruntime "on"
	
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
	}
			
	libdirs
	{
		"%{LibDir.sdl2}",
		"%{LibDir.glew}"
	}

	includedirs
	{
		"Source",
		"%{IncludeDir.toshi}",
		"%{IncludeDir.sdl2}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.glew}",
		"%{IncludeDir.imgui}"
	}

	defines
	{
		"TOSHI_CONSOLE",
		"SDL_MAIN_HANDLED"
	}
