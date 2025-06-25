#include "pch.h"
#include "TextureResourceView.h"
#include "Application.h"

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

struct TTL_Win
{
	struct TexInfo
	{
		TTEXTURE_FORMAT eFormat;
		const TCHAR*    szFileName;
		TUINT           uiDataSize;
		TBYTE*          pData;
	};

	TINT         iNumTextures;
	TexInfo*     pTextureInfos;
	const TCHAR* szPackName;
}; // struct TTL_Win

struct TTL_Rev
{
	struct TexInfo
	{
		TTEXTURE_FORMAT eFormat;
		const TCHAR*    szFileName;
		TUINT           uiWidth;
		TUINT           uiHeight;
		
		TINT            iUnk1;
		TBYTE*          pData;
		TINT            iUnk2;

		void*           pLUT1;
		void*           pLUT2;
		TINT            iUnk3;
		TINT            iUnk4;

		TUINT uiNumMipMaps;
		TUINT uiDataSize;
	};

	TINT         iNumTextures;
	TexInfo*     pTextureInfos;
	void*        pZero1;
	void*        pZero2;

}; // struct TTL_Rev

} // namespace TRBHeader

// libogc gx defines
#define _GX_TF_CTF   0x20
#define _GX_TF_ZTF   0x10
#define GX_TF_Z16    ( 0x3 | _GX_TF_ZTF )
#define GX_TF_Z24X8  ( 0x6 | _GX_TF_ZTF )
#define GX_TF_Z8     ( 0x1 | _GX_TF_ZTF )
#define GX_CTF_A8    ( 0x7 | _GX_TF_CTF )
#define GX_CTF_B8    ( 0xA | _GX_TF_CTF )
#define GX_CTF_G8    ( 0x9 | _GX_TF_CTF )
#define GX_CTF_GB8   ( 0xC | _GX_TF_CTF )
#define GX_CTF_R4    ( 0x0 | _GX_TF_CTF )
#define GX_CTF_R8    ( 0x8 | _GX_TF_CTF )
#define GX_CTF_RA4   ( 0x2 | _GX_TF_CTF )
#define GX_CTF_RA8   ( 0x3 | _GX_TF_CTF )
#define GX_CTF_RG8   ( 0xB | _GX_TF_CTF )
#define GX_CTF_YUVA8 ( 0x6 | _GX_TF_CTF )
#define GX_CTF_Z16L  ( 0xC | _GX_TF_ZTF | _GX_TF_CTF )
#define GX_CTF_Z4    ( 0x0 | _GX_TF_ZTF | _GX_TF_CTF )
#define GX_CTF_Z8L   ( 0xA | _GX_TF_ZTF | _GX_TF_CTF )
#define GX_CTF_Z8M   ( 0x9 | _GX_TF_ZTF | _GX_TF_CTF )
#define GX_TF_A8     GX_CTF_A8
#define GX_TF_CI14   0xa
#define GX_TF_CI4    0x8
#define GX_TF_CI8    0x9
#define GX_TF_CMPR   0xE
#define GX_TF_I4     0x0
#define GX_TF_I8     0x1
#define GX_TF_IA4    0x2
#define GX_TF_IA8    0x3
#define GX_TF_RGB565 0x4
#define GX_TF_RGB5A3 0x5
#define GX_TF_RGBA8  0x6
#define GX_TL_IA8    0x00
#define GX_TL_RGB565 0x01
#define GX_TL_RGB5A3 0x02

TBOOL TextureResourceView::OnCreate()
{
	TRBResourceView::OnCreate();

	// Create unique IDs
	m_strTexturesId.Format( "##Textures%u", GetImGuiID() );
	m_strPreviewId.Format( "##Preview%u", GetImGuiID() );

	if ( m_strSymbolName == "TTL" )
	{
		TOSHISKU ePlatform = g_oTheApp.GetSelectedPlatform();

		switch ( g_oTheApp.GetSelectedGame() )
		{
			case TOSHIGAME_BARNYARD:
				if ( ePlatform == TOSHISKU_WINDOWS )
					return LoadTTL_Barnyard_Windows();
				else if ( ePlatform == TOSHISKU_REV )
					return LoadTTL_Barnyard_Rev();

				break;
		}
		return TFALSE;
	}

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

TBOOL TextureResourceView::LoadTTL_Barnyard_Windows()
{
	TRBHeader::TTL_Win* pTTL = TSTATICCAST( TRBHeader::TTL_Win, m_pData );

	if ( !pTTL )
		return TFALSE;

	TINT iNumTextures = ConvertEndianess( pTTL->iNumTextures );

	m_strPackName = pTTL->szPackName;
	m_vecTextures.Reserve( iNumTextures );

	for ( TINT i = 0; i < iNumTextures; i++ )
	{
		TINT iWidth, iHeight, iNumComponents;
		TBYTE* pData = TSTATICCAST( TBYTE, pTTL->pTextureInfos[ i ].pData );
		TUINT  uiDataSize = ConvertEndianess( pTTL->pTextureInfos[ i ].uiDataSize );

		// Load, decode image from the buffer
		TBYTE* pImgData = SOIL_load_image_from_memory( pData, uiDataSize, &iWidth, &iHeight, &iNumComponents, 4 );

		if ( !pImgData )
		{
			TERROR( "Couldn't load texture '%s'\n", pTTL->pTextureInfos[ i ].szFileName );
			continue;
		}

		Texture* pInsertedTexture = m_vecTextures.EmplaceBack();
		pInsertedTexture->strName = pTTL->pTextureInfos[ i ].szFileName;
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

TBOOL TextureResourceView::LoadTTL_Barnyard_Rev()
{
	TRBHeader::TTL_Rev* pTTL = TSTATICCAST( TRBHeader::TTL_Rev, m_pData );

	// Some asserts to make sure they are always zero...
	TASSERT( pTTL->pZero1 == TNULL );
	TASSERT( pTTL->pZero2 == TNULL );

	if ( !pTTL )
		return TFALSE;

	m_vecTextures.Reserve( ConvertEndianess( pTTL->iNumTextures ) );

	for ( TINT i = 0; i < ConvertEndianess( pTTL->iNumTextures ); i++ )
	{
		TRBHeader::TTL_Rev::TexInfo* pTexInfo = &pTTL->pTextureInfos[ i ];

		TBYTE*          pData      = TSTATICCAST( TBYTE, pTexInfo->pData );
		TUINT           uiWidth    = ConvertEndianess( pTexInfo->uiWidth );
		TUINT           uiHeight   = ConvertEndianess( pTexInfo->uiHeight );
		TUINT           uiDataSize = ConvertEndianess( pTexInfo->uiDataSize );
		TTEXTURE_FORMAT eFormat    = ConvertEndianess( pTexInfo->eFormat );

		// Copy the data into a buffer
		CTLib::Buffer dataBuffer( uiDataSize );
		dataBuffer.putArray( pTexInfo->pData, uiDataSize );
		dataBuffer.rewind();

		CTLib::ImageFormat eCTLibImgFmt;
		switch ( eFormat )
		{
			case TTEX_FMT_REV_I4:
				eCTLibImgFmt = CTLib::ImageFormat::I4;
				break;
			case TTEX_FMT_REV_IA4:
				eCTLibImgFmt = CTLib::ImageFormat::IA4;
				break;
			case TTEX_FMT_REV_I8:
				eCTLibImgFmt = CTLib::ImageFormat::I8;
				break;
			case TTEX_FMT_REV_IA8:
				eCTLibImgFmt = CTLib::ImageFormat::IA8;
				break;
			case TTEX_FMT_REV_RGB565:
				eCTLibImgFmt = CTLib::ImageFormat::RGB565;
				break;
			case TTEX_FMT_REV_RGB5A3:
				eCTLibImgFmt = CTLib::ImageFormat::RGB5A3;
				break;
			case TTEX_FMT_REV_RGBA8:
				eCTLibImgFmt = CTLib::ImageFormat::RGBA8;
				break;
			case TTEX_FMT_REV_CMPR:
				eCTLibImgFmt = CTLib::ImageFormat::CMPR;
				break;
			default:
				TERROR( "Unsupported TTEXTURE_FORMAT format for the '%s' texture!\n", pTexInfo->szFileName );
				continue;
		}

		// Decode image
		try
		{
			CTLib::Image image = CTLib::ImageCoder::decode( dataBuffer, uiWidth, uiHeight, eCTLibImgFmt );

			Texture* pInsertedTexture = m_vecTextures.EmplaceBack();
			pInsertedTexture->strName = pTexInfo->szFileName;
			pInsertedTexture->iWidth  = TINT( uiWidth );
			pInsertedTexture->iHeight = TINT( uiHeight );

			// Create texture
			pInsertedTexture->oTexture.Create( TEXTURE_FORMAT_R8G8B8A8_UNORM, pInsertedTexture->iWidth, pInsertedTexture->iHeight, *image.getData() );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
		}
		catch ( CTLib::ImageError error )
		{
			TERROR( "An error has occured while decoding '%s' texture!\n", pTexInfo->szFileName );
		}
	}

	return TTRUE;

	//CTLib::Buffer buffer = CTLib::IO::readFile( "C:\\Users\\InfC0re\\Desktop\\test_texture" );
	//CTLib::Image  image  = CTLib::ImageCoder::decode( buffer, 512, 256, CTLib::ImageFormat::RGBA8 );
	//
	//CTLib::ImageIO::write( "C:\\Users\\InfC0re\\Desktop\\test_texture.png", image );
}
