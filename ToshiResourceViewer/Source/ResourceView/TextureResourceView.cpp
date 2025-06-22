#include "pch.h"
#include "TextureResourceView.h"

#include "SOIL2/SOIL2.h"
#include <imgui_internal.h>

#include <CTLib/Utilities.hpp>
#include <CTLib/Image.hpp>

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

namespace TRBHeader
{

struct TTL
{
	struct TexInfo
	{
		BOOL         m_bIsT2Texture;
		const TCHAR* m_szFileName;
		TUINT        m_uiDataSize;
		void*        m_pData;
	};

	TINT         m_iNumTextures;
	TexInfo*     m_pTextureInfos;
	const TCHAR* m_szPackName;
};

} // namespace TRBHeader

TBOOL TextureResourceView::OnCreate()
{
	TRBResourceView::OnCreate();

	// Create unique IDs
	m_strTexturesId.Format( "##Textures%u", GetImGuiID() );
	m_strPreviewId.Format( "##Preview%u", GetImGuiID() );

	if ( m_strSymbolName == "TTL" )
		return LoadTTL_Windows();

	return TFALSE;
}

TBOOL TextureResourceView::CanSave()
{
	return TFALSE;
}

TBOOL TextureResourceView::OnSave( PTRB* pOutTRB )
{
	//CTLib::Buffer buffer = CTLib::IO::readFile( "C:\\Users\\InfC0re\\Desktop\\test_texture" );
	//CTLib::Image  image  = CTLib::ImageCoder::decode( buffer, 512, 256, CTLib::ImageFormat::RGBA8 );
	//
	//CTLib::ImageIO::write( "C:\\Users\\InfC0re\\Desktop\\test_texture.png", image );

	return TFALSE;
}

void TextureResourceView::OnDestroy()
{
	T2_FOREACH( m_vecTextures, it )
	{
		it->oTexture.Destroy();
	}

	m_vecTextures.Clear();
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
				if ( ImGui::Selectable( it->strName.GetString(), &bSelected ) )
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
			TFLOAT fWidth  = (TFLOAT)texInfo.iWidth * m_fScale;
			TFLOAT fHeight = (TFLOAT)texInfo.iHeight * m_fScale;

			ImGui::SetCursorPos( ImVec2( oRegion.x / 2 - fWidth / 2 + m_fOffsetX, oRegion.y / 2 - fHeight / 2 + m_fOffsetY ) );
			ImGui::Image( texInfo.oTexture.GetHandle(), ImVec2( fWidth, fHeight ) );

			// Draw info
			ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 1.0f, 1.0f, 0.5f ) );
			ImGui::SetCursorPos( ImVec2( 12.0f, oRegion.y - ImGui::GetFontSize() * 2 - 8.0f ) );
			ImGui::Text( "Scale: %.2f", m_fScale );
			ImGui::SetCursorPos( ImVec2( 12.0f, oRegion.y - ImGui::GetFontSize() - 8.0f ) );
			ImGui::Text( "Width: %d, Height: %d", texInfo.iWidth, texInfo.iHeight );
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

TBOOL TextureResourceView::LoadTTL_Windows()
{
	TRBHeader::TTL* pTTL = TSTATICCAST( TRBHeader::TTL, m_pData );

	if ( !pTTL )
		return TFALSE;

	m_strPackName = pTTL->m_szPackName;
	m_vecTextures.Reserve( pTTL->m_iNumTextures );

	for ( TINT i = 0; i < pTTL->m_iNumTextures; i++ )
	{
		TINT iWidth, iHeight, iNumComponents;
		TBYTE* pData = TSTATICCAST( TBYTE, pTTL->m_pTextureInfos[ i ].m_pData );
		TUINT  uiDataSize = pTTL->m_pTextureInfos[ i ].m_uiDataSize;

		// Load, decode image from the buffer
		TBYTE* pImgData = SOIL_load_image_from_memory( pData, uiDataSize, &iWidth, &iHeight, &iNumComponents, 4 );

		if ( !pImgData )
		{
			TERROR( "Couldn't load texture '%s'\n", pTTL->m_pTextureInfos[ i ].m_szFileName );
			continue;
		}

		Texture* pInsertedTexture = m_vecTextures.EmplaceBack();
		pInsertedTexture->strName = pTTL->m_pTextureInfos[ i ].m_szFileName;
		pInsertedTexture->iWidth  = iWidth;
		pInsertedTexture->iHeight = iHeight;

		// Create texture
		pInsertedTexture->oTexture.Create( TEXTURE_FORMAT_R8G8B8A8_UNORM, iWidth, iHeight, pImgData );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );

		// Free image data
		SOIL_free_image_data( pImgData );
	}

	return TTRUE;
}
