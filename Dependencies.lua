IncludeDir = {}
IncludeDir.fmod = "%{wks.location}/OpenToshi/Toshi/Vendor/fmod/include"
IncludeDir.trbf = "%{wks.location}/OpenToshi/Tools/TRBF/Include"
IncludeDir.libogg = "%{wks.location}/OpenToshi/Toshi/Vendor/libogg/include"
IncludeDir.libvorbis = "%{wks.location}/OpenToshi/Toshi/Vendor/libvorbis/include"
IncludeDir.libtheora = "%{wks.location}/OpenToshi/Toshi/Vendor/libtheora/include"
IncludeDir.theoraplay = "%{wks.location}/OpenToshi/Toshi/Vendor/theoraplay/include"
IncludeDir.stb = "%{wks.location}/OpenToshi/Toshi/Vendor/stb"
IncludeDir.dx8 = "%{wks.location}/OpenToshi/Toshi/Vendor/DX81/include"
IncludeDir.bink = "%{wks.location}/OpenToshi/Toshi/Vendor/bink/include"
IncludeDir.detours = "%{wks.location}/OpenToshi/SDK/Vendor/Detours/include"
IncludeDir.glm = "%%{wks.location}/OpenToshi/Toshi/Vendor/glm/include"
IncludeDir.glew = "%{wks.location}/OpenToshi/Toshi/Vendor/glew/include"
IncludeDir.sdl2 = "%{wks.location}/OpenToshi/Toshi/Vendor/sdl2/include"
IncludeDir.toshi = "%{wks.location}/OpenToshi/Toshi/Source"
IncludeDir.imgui = "%{wks.location}/ThirdParty/ImGui"
IncludeDir.assimp = "%{wks.location}/ToshiResourceViewer/Vendor/assimp/include"

LibDir = {}
LibDir.fmod = "%{wks.location}/OpenToshi/Toshi/Vendor/fmod/lib"
LibDir.dx8 = "%{wks.location}/OpenToshi/Toshi/Vendor/DX81/lib"
LibDir.bink = "%{wks.location}/OpenToshi/Toshi/Vendor/bink/lib"
LibDir.detours = "%{wks.location}/OpenToshi/SDK/Vendor/Detours/lib"
LibDir.glew = "%{wks.location}/OpenToshi/Toshi/Vendor/glew/lib"
LibDir.sdl2 = "%{wks.location}/OpenToshi/Toshi/Vendor/sdl2/lib"
LibDir.assimp = "%{wks.location}/ToshiResourceViewer/Vendor/assimp/lib"

-- content of these folders should be copied to any client application
ClientContentCommon = "%{wks.location}/OpenToshiContent/Common/"
ClientContentArch   = "%{wks.location}/OpenToshiContent"

-- modify paths based on architecture
LibDir.fmod = LibDir.fmod .. "/x86/"
LibDir.bink = LibDir.bink .. "/x86/"
LibDir.detours = LibDir.detours .. "/x86/"
ClientContentArch = ClientContentArch .. "/x86/"