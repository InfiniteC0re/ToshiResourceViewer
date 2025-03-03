#pragma once
#include <imgui.h>

namespace ImGuiUtils
{

// Returns an unique ID
ImGuiID GetComponentID( ImGuiID uiOffset = 1 );

struct ImGuiComponent
{
	ImGuiID uiComponentID = GetComponentID();

	ImGuiID GetImGuiID() const { return uiComponentID; }
	void    PreRender() const;
	void    PostRender() const;
};

}
