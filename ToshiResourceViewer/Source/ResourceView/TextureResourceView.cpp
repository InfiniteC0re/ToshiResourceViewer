#include "pch.h"
#include "TextureResourceView.h"
#include "Application.h"
#include "ResourceLoader/TextureLoader.h"

#include <imgui_internal.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TextureResourceView::TextureResourceView()
{
	m_strName = "Texture Library";
}

TextureResourceView::~TextureResourceView()
{
}

TBOOL TextureResourceView::OnCreate( Toshi::T2StringView pchFilePath )
{
	TRBResourceView::OnCreate( pchFilePath );

	// Create unique IDs
	m_strTexturesId.Format( "##Textures%u", GetImGuiID() );
	m_strPreviewId.Format( "##Preview%u", GetImGuiID() );

	if ( m_strSymbolName == "TTL" )
		return ResourceLoader::TTL_Load( m_pData, m_pTRB->GetEndianess(), TTRUE, TFALSE, m_vecTextures, &m_strName );

	return TFALSE;
}

TBOOL TextureResourceView::CanSave()
{
	return TFALSE;
}

TBOOL TextureResourceView::OnSave( PTRB* pOutTRB )
{
	return TFALSE;
}

void TextureResourceView::OnDestroy()
{
}

void TextureResourceView::OnRender( TFLOAT flDeltaTime )
{
	ImVec2 vInitialPos = ImGui::GetCursorPos();

	ImGui::Text( "Textures" );
	ImGui::BeginChild( m_strTexturesId.Get(), ImVec2(200, -1), ImGuiChildFlags_ResizeX );
	{
		ImGui::PushStyleColor( ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0) );
		if ( ImGui::BeginListBox( "Textures", ImVec2(-1, -1) ) )
		{
			T2_FOREACH( m_vecTextures, it )
			{
				TBOOL bSelected = m_iSelectedTexture == it.Index();
				if ( ImGui::Selectable( it->Get()->GetTexture().strName.GetString(), &bSelected ) )
					m_iSelectedTexture = it.Index();
			}

			ImGui::EndListBox();
		}
		ImGui::PopStyleColor();

		ImGui::EndChild();
	}

	ImGui::SameLine();
	ImVec2 vPreviewPos = ImGui::GetCursorPos();
	ImGui::SetCursorPos( ImVec2( vPreviewPos.x, vInitialPos.y ) );
	ImGui::Text( "Preview" );

	ImGui::SameLine();
	ImGui::SetCursorPos( ImVec2( ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize( "Reset View" ).x - ImGui::GetStyle().FramePadding.x * 2, vInitialPos.y ) );
	if ( ImGui::SmallButton( "Reset View" ) )
	{
		m_fOffsetX = 0.0f;
		m_fOffsetY = 0.0f;
		m_fScale   = 1.0f;
	}

	ImGui::SetCursorPos( vPreviewPos );
	ImGui::BeginChild( m_strPreviewId.Get(), ImVec2( -1, -1 ), 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
	{
		if ( m_iSelectedTexture >= 0 && m_iSelectedTexture < m_vecTextures.Size() )
		{
			auto& texInfo = m_vecTextures[ m_iSelectedTexture ];

			ImVec2 oRegion = ImGui::GetContentRegionAvail();
			TFLOAT fWidth  = (TFLOAT)texInfo->GetTexture().iWidth * m_fScale;
			TFLOAT fHeight = (TFLOAT)texInfo->GetTexture().iHeight * m_fScale;

			ImGui::SetCursorPos( ImVec2( oRegion.x / 2 - fWidth / 2 + m_fOffsetX, oRegion.y / 2 - fHeight / 2 + m_fOffsetY ) );
			ImGui::Image( texInfo->GetHandle(), ImVec2( fWidth, fHeight ) );

			// Draw info
			ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 1.0f, 1.0f, 0.5f ) );
			ImGui::SetCursorPos( ImVec2( 12.0f, oRegion.y - ImGui::GetFontSize() * 2 - 8.0f ) );
			ImGui::Text( "Scale: %.2f", m_fScale );
			ImGui::SetCursorPos( ImVec2( 12.0f, oRegion.y - ImGui::GetFontSize() - 8.0f ) );
			ImGui::Text( "Width: %d, Height: %d", texInfo->GetTexture().iWidth, texInfo->GetTexture().iHeight );
			ImGui::PopStyleColor();

			// Control scale and offset
			if ( ImGui::IsWindowHovered() )
			{
				m_fScale += TINT( ImGui::GetIO().MouseWheel ) * 0.1f;
				TMath::Clip( m_fScale, 0.1f, 5.0f );

				static TBOOL s_bWasDragging = TFALSE;
				TBOOL        bIsDragging    = ImGui::IsMouseDown( ImGuiMouseButton_Left );

				if ( ImGui::IsMouseDown( ImGuiMouseButton_Left ) )
				{
					static ImVec2 s_vLastPos  = ImGui::GetMousePos();
					ImVec2        vCurrentPos = ImGui::GetMousePos();
					ImVec2        vDrag       = ImVec2( s_vLastPos.x - vCurrentPos.x, s_vLastPos.y - vCurrentPos.y );

					if ( s_bWasDragging )
					{
						m_fOffsetX -= vDrag.x;
						m_fOffsetY -= vDrag.y;
					}

					// Don't let the event go further
					ImGui::SetActiveID( ImGui::GetID( GetImGuiID() ), ImGui::GetCurrentWindow() );

					// Save current pos for the next frame
					s_vLastPos = vCurrentPos;
				}

				s_bWasDragging = bIsDragging;
			}
		}

		ImGui::EndChild();
	}
}
