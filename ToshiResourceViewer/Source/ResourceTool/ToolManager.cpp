#include "pch.h"
#include "ToolManager.h"
#include "TextureTool.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

void ToolManager::Render()
{
	TextureTool::Render();
}
