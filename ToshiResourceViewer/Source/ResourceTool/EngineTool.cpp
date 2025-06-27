#include "pch.h"
#include "EngineTool.h"
#include "Resource/StreamedTexture.h"

#include <imgui.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

static TBOOL s_bVisible = TFALSE;

void EngineTool::Show()
{
	s_bVisible = TTRUE;
}

void EngineTool::Hide()
{
	s_bVisible = TFALSE;
}

void EngineTool::Toggle()
{
	s_bVisible = !s_bVisible;
}

void EngineTool::Render()
{
	if ( !s_bVisible )
		return;

	ImGui::Begin( "Engine Tool", &s_bVisible );

	ImGui::Text( "Loaded Textures" );
	if ( ImGui::BeginListBox( "##LoadedTextures", ImVec2( -FLT_MIN, 0.0f ) ) )
	{
		// List all textures
		for ( auto it = Resource::StreamedTexture_GetIteratorBegin(); it != Resource::StreamedTexture_GetIteratorEnd(); it++ )
		{
			if ( it->second.IsValid() && it->second->IsLoaded() )
				ImGui::Selectable( it->second->GetTexture().strName );
		}

		ImGui::EndListBox();
	}

	ImGui::Text( "Dummy Textures" );
	if ( ImGui::BeginListBox( "##DummyTextures", ImVec2( -FLT_MIN, 0.0f ) ) )
	{
		// List all textures
		for ( auto it = Resource::StreamedTexture_GetIteratorBegin(); it != Resource::StreamedTexture_GetIteratorEnd(); it++ )
		{
			if ( it->second.IsValid() && it->second->IsDummy() )
				ImGui::Selectable( it->second->GetTexture().strName );
		}

		ImGui::EndListBox();
	}

	ImGui::End();
}
