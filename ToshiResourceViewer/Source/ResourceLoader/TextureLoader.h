#pragma once
#include "Resource/StreamedTexture.h"

#include <Toshi/TString8.h>
#include <Toshi/Endianness.h>
#include <Render/TTexture.h>
#include <ToshiTools/T2DynamicVector.h>

#include <Platform/GL/T2GLTexture_GL.h>

namespace ResourceLoader
{

using Textures = Toshi::T2DynamicVector<Toshi::T2SharedPtr<Resource::StreamedTexture>>;

TBOOL TTL_Load( void* pData, Endianess eEndianess, TBOOL bCreateTextures, TBOOL bForcePlatform, Textures& rOutVector, Toshi::TString8* pOutName = TNULL );
TBOOL TTL_Load_Barnyard_Windows( void* pData, Endianess eEndianess, TBOOL bCreateTextures, Textures& rOutVector, Toshi::TString8* pOutName = TNULL );
TBOOL TTL_Load_Barnyard_Rev( void* pData, Endianess eEndianess, TBOOL bCreateTextures, Textures& rOutVector, Toshi::TString8* pOutName = TNULL );

TBOOL TTL_UnpackTextures( Textures& rTextures, const TCHAR* szOutDir );

struct TTL_Win
{
	struct TexInfo
	{
		Toshi::TTEXTURE_FORMAT eFormat;
		const TCHAR*           szFileName;
		TUINT                  uiDataSize;
		TBYTE*                 pData;
	};

	TINT         iNumTextures;
	TexInfo*     pTextureInfos;
	const TCHAR* szPackName;
}; // struct TTL_Win

struct TTL_Rev
{
	struct TexInfo
	{
		Toshi::TTEXTURE_FORMAT eFormat;
		const TCHAR*    szFileName;
		TUINT           uiWidth;
		TUINT           uiHeight;

		TINT   iUnk1;
		TBYTE* pData;
		TINT   iUnk2;

		TBYTE* pLUTColor;
		TBYTE* pLUTAlpha;
		TINT   iUnk3;
		TINT   iUnk4;

		TUINT uiNumMipMaps;
		TUINT uiDataSize;
	};

	TINT     iNumTextures;
	TexInfo* pTextureInfos;
	void*    pZero1;
	void*    pZero2;

}; // struct TTL_Rev

} // namespace ResourceLoader
