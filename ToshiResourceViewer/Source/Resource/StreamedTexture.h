#pragma once
#include <Toshi/T2Map.h>
#include <Toshi/TString8.h>
#include <Toshi/TPString8.h>
#include <Toshi/T2SharedPtr.h>

#include <Platform/GL/T2GLTexture_GL.h>

namespace Resource
{

struct Texture
{
	Toshi::TPString8   strName;
	Toshi::T2GLTexture pHandle;
	TINT               iWidth  = 0;
	TINT               iHeight = 0;
	const void*        pData   = TNULL;
};

class StreamedTexture
{
public:
	StreamedTexture();
	~StreamedTexture();

	void  Setup( const Toshi::TPString8& strName, TINT iWidth, TINT iHeight, const void* pData );
	TBOOL Create();
	void  Destroy();

	GLuint              GetHandle();
	TBOOL               IsLoaded() const { return m_bCreated; }
	TBOOL               IsDummy() const { return !m_bCreated; }
	Texture&            GetTexture() { return m_oTexture; }
	const Texture&      GetTexture() const { return m_oTexture; }

private:
	Texture            m_oTexture;
	TBOOL              m_bCreated;
};

using StreamedTextureMap = Toshi::T2Map<Toshi::TPString8, Toshi::T2WeakPtr<Resource::StreamedTexture>, Toshi::TPString8::Comparator>;

void                         StreamedTexture_DestroyUnused();
TINT                         StreamedTexture_GetTotalNumber();
StreamedTextureMap::Iterator StreamedTexture_GetIteratorBegin();
StreamedTextureMap::Iterator StreamedTexture_GetIteratorEnd();

Toshi::T2SharedPtr<StreamedTexture> StreamedTexture_Create( const Toshi::TPString8& strName, TINT iWidth, TINT iHeight, const void* pData, TBOOL bCreateTexture );
Toshi::T2SharedPtr<StreamedTexture> StreamedTexture_FindOrCreateDummy( const Toshi::TPString8& strName );

} // namespace Resource
