#pragma once
#include <Toshi/TString8.h>
#include <Toshi/Endianness.h>
#include <Render/TTexture.h>
#include <ToshiTools/T2DynamicVector.h>

#include <Platform/GL/T2GLTexture_GL.h>

namespace ResourceLoader
{

struct Texture
{
	Toshi::TString8    strName;
	Toshi::T2GLTexture oTexture;
	TINT               iWidth;
	TINT               iHeight;
	TBYTE*             pData;
};

using Textures = Toshi::T2DynamicVector<ResourceLoader::Texture>;

TBOOL TTL_Load( void* pData, Endianess eEndianess, TBOOL bCreateTextures, TBOOL bForcePlatform, Textures& rOutVector, Toshi::TString8* pOutName = TNULL );
TBOOL TTL_Load_Barnyard_Windows( void* pData, Endianess eEndianess, TBOOL bCreateTextures, Textures& rOutVector, Toshi::TString8* pOutName = TNULL );
TBOOL TTL_Load_Barnyard_Rev( void* pData, Endianess eEndianess, TBOOL bCreateTextures, Textures& rOutVector, Toshi::TString8* pOutName = TNULL );
void  TTL_Destroy( Textures& rTextures );

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
