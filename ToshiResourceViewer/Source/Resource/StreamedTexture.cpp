#include "pch.h"
#include "StreamedTexture.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

Resource::StreamedTexture::StreamedTexture()
    : m_bCreated( TFALSE )
{
}

Resource::StreamedTexture::~StreamedTexture()
{
	Destroy();
}

void Resource::StreamedTexture::Setup( const Toshi::TPString8& strName, TINT iWidth, TINT iHeight, const void* pData )
{
	m_oTexture.strName = strName;
	m_oTexture.iWidth  = iWidth;
	m_oTexture.iHeight = iHeight;
	m_oTexture.pData   = pData;
}

TBOOL Resource::StreamedTexture::Create()
{
	if ( !m_oTexture.pData || m_oTexture.iWidth <= 0 || m_oTexture.iHeight <= 0 )
		return TFALSE;

	if ( m_bCreated )
		Destroy();

	m_oTexture.pHandle.Create( TEXTURE_FORMAT_R8G8B8A8_UNORM, m_oTexture.iWidth, m_oTexture.iHeight, m_oTexture.pData );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
	m_bCreated = TTRUE;

	return TTRUE;
}

void Resource::StreamedTexture::Destroy()
{
	if ( m_bCreated )
	{
		m_oTexture.pHandle.Destroy();

		if ( m_oTexture.pData )
			delete[] m_oTexture.pData;

		m_oTexture.strName  = TPString8();
		m_oTexture.pData    = TNULL;
		m_oTexture.iWidth   = 0;
		m_oTexture.iHeight  = 0;

		m_bCreated = TFALSE;
	}
}

GLuint Resource::StreamedTexture::GetHandle()
{
	return ( m_bCreated ) ? m_oTexture.pHandle.GetHandle() : g_pTextureManager->GetInvalidTexture()->GetHandle();
}

Resource::StreamedTextureMap g_mapStreamedTextures;

void Resource::StreamedTexture_DestroyUnused()
{
	for ( auto it = g_mapStreamedTextures.Begin(); it != g_mapStreamedTextures.End(); )
	{
		auto next = it.Next();

		// Destroy the weak reference if no strong references are left
		if ( !it->second.IsValid() )
			g_mapStreamedTextures.Remove( it );

		it = next;
	}
}

TINT Resource::StreamedTexture_GetTotalNumber()
{
	return g_mapStreamedTextures.Size();
}

Resource::StreamedTextureMap::Iterator Resource::StreamedTexture_GetIteratorBegin()
{
	return g_mapStreamedTextures.Begin();
}

Resource::StreamedTextureMap::Iterator Resource::StreamedTexture_GetIteratorEnd()
{
	return g_mapStreamedTextures.End();
}

T2SharedPtr<Resource::StreamedTexture> Resource::StreamedTexture_Create( const TPString8& strName, TINT iWidth, TINT iHeight, const void* pData, TBOOL bCreateTexture )
{
	// See if the texture is already created
	auto it = g_mapStreamedTextures.Find( strName );

	if ( it != g_mapStreamedTextures.End() )
	{
		if ( !it->second.IsValid() )
			Resource::StreamedTexture_DestroyUnused();
		else
		{
			if ( it->second->IsDummy() )
			{
				// Complete the texture with real data
				it->second->Setup( strName, iWidth, iHeight, pData );

				if ( bCreateTexture )
					it->second->Create();
			}

			return it->second;
		}
	}

	// Create new real texture
	auto pTexture = T2SharedPtr<Resource::StreamedTexture>::New();
	
	pTexture->Setup( strName, iWidth, iHeight, pData );

	if ( bCreateTexture )
		pTexture->Create();

	g_mapStreamedTextures.Insert( strName, T2WeakPtr<Resource::StreamedTexture>( pTexture ) );

	return pTexture;
}

T2SharedPtr<Resource::StreamedTexture> Resource::StreamedTexture_FindOrCreateDummy( const TPString8& strName )
{
	// See if the texture is already created
	auto it = g_mapStreamedTextures.Find( strName );

	if ( it != g_mapStreamedTextures.End() )
	{
		if ( !it->second.IsValid() )
			Resource::StreamedTexture_DestroyUnused();
		else
			return it->second;
	}

	// Create new dummy texture
	auto pTexture = T2SharedPtr<Resource::StreamedTexture>::New();

	pTexture->GetTexture().strName = strName;
	g_mapStreamedTextures.Insert( strName, T2WeakPtr<Resource::StreamedTexture>( pTexture ) );

	return pTexture;
}
