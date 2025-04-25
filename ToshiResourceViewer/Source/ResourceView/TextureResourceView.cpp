#include "pch.h"
#include "TextureResourceView.h"

#include "SOIL2/SOIL2.h"
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
		return LoadTTL();

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
	ImGui::DockSpace( m_uiDockspace.GetImGuiID(), ImVec2( 0, 0 ), ImGuiDockNodeFlags_NoTabBar );
	
	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );
	ImGui::Begin( m_strTexturesId.Get(), TNULL, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration );
	{
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

		ImGui::End();
	}

	ImGui::Begin( m_strPreviewId.Get(), TNULL, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration );
	{
		if ( m_iSelectedTexture >= 0 && m_iSelectedTexture < m_vecTextures.Size() )
		{
			auto& texInfo = m_vecTextures[ m_iSelectedTexture ];

			ImGui::Image( texInfo.oTexture.GetHandle(), ImVec2( (TFLOAT)texInfo.iWidth, (TFLOAT)texInfo.iHeight ) );
		}

		ImGui::End();
	}
	ImGui::PopStyleVar();

	// Dock the windows
	if ( !m_bDocked )
	{
		ImGuiID dockspace_main_id = m_uiDockspace.GetImGuiID();
		ImGui::DockBuilderRemoveNode( dockspace_main_id );
		ImGui::DockBuilderAddNode( dockspace_main_id );
		ImGui::DockBuilderSetNodeSize( dockspace_main_id, ImGui::GetMainViewport()->Size );

		ImGuiID right;
		ImGuiID left = ImGui::DockBuilderSplitNode( dockspace_main_id, ImGuiDir_Left, 0.4f, nullptr, &right );

		ImGui::DockBuilderDockWindow( m_strTexturesId.Get(), left );
		ImGui::DockBuilderDockWindow( m_strPreviewId.Get(), right );
		ImGui::DockBuilderFinish( dockspace_main_id );

		m_bDocked = TTRUE;
	}
}

TBOOL TextureResourceView::LoadTTL()
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

		// Free image data
		SOIL_free_image_data( pImgData );
	}

	return TTRUE;
}
