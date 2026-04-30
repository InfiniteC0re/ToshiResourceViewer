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
	m_strDockspaceId.Format( "##Dockspace%u", GetImGuiID() );
	m_strTexturesId.Format( "Textures##Textures%u", GetImGuiID() );
	m_strPreviewId.Format( "Preview##Preview%u", GetImGuiID() );

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

	const ImGuiID dockSpaceID = ImGui::GetID( m_strDockspaceId.Get() );

	ImGuiWindowClass windowClass;
	windowClass.ClassId = GetImGuiID();

	// Create dockspace for the resource view window
	ImGui::SetNextWindowClass( &windowClass );
	ImGui::DockSpace( dockSpaceID );

	constexpr TFLOAT WINDOW_PADDING = 0.0f;

	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( WINDOW_PADDING, WINDOW_PADDING ) );

	if ( !m_bDockingSetUp )
	{
		// Clear just in case
		ImGui::DockBuilderRemoveNode( dockSpaceID );

		// Create root node and set initial size
		ImGuiID dockRoot = ImGui::DockBuilderAddNode( dockSpaceID, ImGuiDockNodeFlags_DockSpace );
		ImGui::DockBuilderSetNodeSize( dockSpaceID, ImGui::GetWindowSize() );

		// Start splitting the UI
		m_DockLeft = ImGui::DockBuilderSplitNode( dockSpaceID, ImGuiDir_Left, 0.5f, TNULL, &m_DockRight );

		// Finally, dock the windows
		ImGui::DockBuilderDockWindow( m_strTexturesId.Get(), m_DockLeft );
		ImGui::DockBuilderDockWindow( m_strPreviewId.Get(), m_DockRight );

		ImGui::DockBuilderFinish( dockSpaceID );
		m_bDockingSetUp = TTRUE;
	}

	ImGui::SetNextWindowClass( &windowClass );
	ImGui::Begin( m_strTexturesId.Get() );
	{
		ImGui::PushStyleColor( ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0) );
		if ( ImGui::BeginListBox( "##Textures", ImVec2(-1, -1) ) )
		{
			T2_FOREACH( m_vecTextures, it )
			{
				TBOOL bSelected = m_iSelectedTexture == it.Index();
				if ( ImGui::Selectable( it->Get()->GetTexture().strName.GetString(), &bSelected ) )
				{
					m_iSelectedTexture = it.Index();
					
					// Reset view
					m_fOffsetX = 0.0f;
					m_fOffsetY = 0.0f;
				}
			}

			ImGui::EndListBox();
		}
		ImGui::PopStyleColor();

		ImGui::End();
	}

	ImGui::SetNextWindowSize( ImVec2( 0, 0 ), ImGuiCond_Appearing );

	ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.18f, 0.185f, 0.20f, 1.0f ) );
	ImGui::Begin( m_strPreviewId.Get(), TNULL, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
	{
		ImGui::PopStyleColor();

		if ( m_iSelectedTexture >= 0 && m_iSelectedTexture < m_vecTextures.Size() )
		{
			auto& texInfo = m_vecTextures[ m_iSelectedTexture ];

			TFLOAT fWidth     = (TFLOAT)texInfo->GetTexture().iWidth * m_fScale;
			TFLOAT fHeight    = (TFLOAT)texInfo->GetTexture().iHeight * m_fScale;
			TFLOAT fBarHeight = ImGui::GetFrameHeight();

			ImVec2 oRegion = ImGui::GetContentRegionAvail();
			oRegion.x += WINDOW_PADDING * 2.0f;
			oRegion.y += WINDOW_PADDING * 2.0f;

			ImGui::SetCursorPos( ImVec2( oRegion.x / 2 - fWidth / 2 + m_fOffsetX, fBarHeight + oRegion.y / 2 - fHeight / 2 + m_fOffsetY ) );
			ImGui::Image( texInfo->GetHandle(), ImVec2( fWidth, fHeight ) );

			// Draw info
			ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 1.0f, 1.0f, 0.5f ) );
			ImGui::SetCursorPos( ImVec2( 12.0f, fBarHeight + oRegion.y - ImGui::GetFontSize() * 3 - 8.0f ) );
			ImGui::Text( "Scale: %.2f", m_fScale );
			ImGui::SetCursorPos( ImVec2( 12.0f, fBarHeight + oRegion.y - ImGui::GetFontSize() * 2 - 8.0f ) );
			ImGui::Text( "Width: %d, Height: %d", texInfo->GetTexture().iWidth, texInfo->GetTexture().iHeight );
			ImGui::SetCursorPos( ImVec2( 12.0f, fBarHeight + oRegion.y - ImGui::GetFontSize() * 1 - 8.0f ) );
			ImGui::Text( "Format: %s", TTEXTURE_FORMAT_TO_STRING( texInfo->GetFormat() ) );
			ImGui::PopStyleColor();

			// Control scale and offset
			if ( ImGui::GetMousePos().y > ImGui::GetWindowPos().y + fBarHeight && ImGui::IsWindowHovered( ImGuiHoveredFlags_ChildWindows ) )
			{
				m_fScale += TINT( ImGui::GetIO().MouseWheel ) * 0.1f;
				TMath::Clip( m_fScale, 0.1f, 5.0f );

				static TBOOL s_bWasDragging = TFALSE;
				TBOOL        bIsDragging    = ImGui::IsMouseDown( ImGuiMouseButton_Left );

				if ( bIsDragging )
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

		ImGui::End();
	}

	ImGui::PopStyleVar();
}
