project "ToshiResourceViewer"
	kind "ConsoleApp"
	language "C++"
	staticruntime "on"
	
	pchheader "pch.h"
	pchsource "Source/pch.cpp"
	
	links
	{
		"Toshi"
	}
	
	files
	{
		"Source/**.h",
		"Source/**.cpp",
	}

	includedirs
	{
		"Source",
		"%{IncludeDir.toshi}",
	}

	defines
	{
		"TOSHI_CONSOLE"
	}
