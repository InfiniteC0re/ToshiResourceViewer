#include "pch.h"
#include "ImGuiUtils.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

static ImGuiID s_uiID = 0;

ImGuiID ImGuiUtils::GetComponentID( ImGuiID uiOffset /*= 1 */ )
{
	ImGuiID uiID = s_uiID;
	
	s_uiID += uiOffset;

	return uiID;
}

void ImGuiUtils::ImGuiComponent::PreRender() const
{
	ImGui::PushID( uiComponentID );
}

void ImGuiUtils::ImGuiComponent::PostRender() const
{
	ImGui::PopID();
}
