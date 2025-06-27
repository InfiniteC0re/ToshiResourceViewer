#include "pch.h"
#include "TextureLoader.h"
#include "Application.h"

#include "SOIL2/SOIL2.h"

#include "CTLib/Utilities.hpp"
#include "CTLib/Image.hpp"

#include "stb/stb_image_write.h"

#include <filesystem>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TBOOL ResourceLoader::TTL_Load( void* pData, Endianess eEndianess, TBOOL bCreateTextures, TBOOL bForcePlatform, Textures& rOutVector, Toshi::TString8* pOutName /*= TNULL */ )
{
	if ( !bForcePlatform )
	{
		// Do not force platform to the current settings and guess it from the file structure
		TTL_Win* pTTL         = TSTATICCAST( TTL_Win, pData );
		TINT     iNumTextures = CONVERTENDIANESS( eEndianess, pTTL->iNumTextures );

		if ( iNumTextures == 0 )
			return TFALSE;

		TTEXTURE_FORMAT eFormat = CONVERTENDIANESS( eEndianess, pTTL->pTextureInfos[ 0 ].eFormat );

		if ( eFormat > TTEX_FMT_WIN_BASE && eFormat <= TTEX_FMT_WIN_DDS )
			return ResourceLoader::TTL_Load_Barnyard_Windows( pData, eEndianess, TTRUE, rOutVector, pOutName );
		
		if ( eFormat > TTEX_FMT_REV_BASE && eFormat <= TTEX_FMT_REV_CI8_IA8 )
			return ResourceLoader::TTL_Load_Barnyard_Rev( pData, eEndianess, TTRUE, rOutVector, pOutName );
		
		TERROR( "ResourceLoader::TTL_Load: Invalid texture format!\n" );
		return TFALSE;
	}
	
	// Force platform to the current settings
	TOSHISKU ePlatform = g_oTheApp.GetSelectedPlatform();

	switch ( g_oTheApp.GetSelectedGame() )
	{
		case TOSHIGAME_BARNYARD:
			if ( ePlatform == TOSHISKU_WINDOWS )
				return ResourceLoader::TTL_Load_Barnyard_Windows( pData, eEndianess, TTRUE, rOutVector, pOutName );
			
			if ( ePlatform == TOSHISKU_REV )
				return ResourceLoader::TTL_Load_Barnyard_Rev( pData, eEndianess, TTRUE, rOutVector, pOutName );

			break;
	}

	return TFALSE;
}

TBOOL ResourceLoader::TTL_Load_Barnyard_Windows( void* pData, Endianess eEndianess, TBOOL bCreateTextures, Textures& rOutVector, TString8* pOutName /*= TNULL */ )
{
	TTL_Win* pTTL = TSTATICCAST( TTL_Win, pData );

	if ( !pTTL )
		return TFALSE;

	TINT iNumTextures = CONVERTENDIANESS( eEndianess, pTTL->iNumTextures );

	if ( pOutName )
		*pOutName = pTTL->szPackName;

	rOutVector.Reserve( iNumTextures );

	for ( TINT i = 0; i < iNumTextures; i++ )
	{
		TINT   iWidth, iHeight, iNumComponents;
		TBYTE* pData      = TSTATICCAST( TBYTE, pTTL->pTextureInfos[ i ].pData );
		TUINT  uiDataSize = CONVERTENDIANESS( eEndianess, pTTL->pTextureInfos[ i ].uiDataSize );

		// Load, decode image from the buffer
		TBYTE* pSoilData = SOIL_load_image_from_memory( pData, uiDataSize, &iWidth, &iHeight, &iNumComponents, 4 );

		if ( !pSoilData )
		{
			TERROR( "Couldn't load texture '%s'\n", pTTL->pTextureInfos[ i ].szFileName );
			continue;
		}

		// Store data in a buffer we own
		TBYTE* pImgData = (TBYTE*)TMalloc( iWidth * iHeight * iNumComponents );
		TUtil::MemCopy( pImgData, pSoilData, iWidth * iHeight * iNumComponents );

		SOIL_free_image_data( pSoilData );
		pSoilData = TNULL;

		rOutVector.EmplaceBack(
		    Resource::StreamedTexture_Create( TPS8D( pTTL->pTextureInfos[ i ].szFileName ), iWidth, iHeight, pImgData, bCreateTextures )
		);
	}

	return TTRUE;
}

TBOOL ResourceLoader::TTL_Load_Barnyard_Rev( void* pData, Endianess eEndianess, TBOOL bCreateTextures, Textures& rOutVector, TString8* pOutName /*= TNULL */ )
{
	TTL_Rev* pTTL = TSTATICCAST( TTL_Rev, pData );

	// Some asserts to make sure they are always zero...
	TASSERT( pTTL->pZero1 == TNULL );
	TASSERT( pTTL->pZero2 == TNULL );

	if ( !pTTL )
		return TFALSE;

	rOutVector.Reserve( CONVERTENDIANESS( eEndianess, pTTL->iNumTextures ) );

	for ( TINT i = 0; i < CONVERTENDIANESS( eEndianess, pTTL->iNumTextures ); i++ )
	{
		TTL_Rev::TexInfo* pTexInfo = &pTTL->pTextureInfos[ i ];

		TBYTE*          pData      = TSTATICCAST( TBYTE, pTexInfo->pData );
		TUINT           uiWidth    = CONVERTENDIANESS( eEndianess, pTexInfo->uiWidth );
		TUINT           uiHeight   = CONVERTENDIANESS( eEndianess, pTexInfo->uiHeight );
		TUINT           uiDataSize = CONVERTENDIANESS( eEndianess, pTexInfo->uiDataSize );
		TTEXTURE_FORMAT eFormat    = CONVERTENDIANESS( eEndianess, pTexInfo->eFormat );

		// Copy the data into a buffer
		CTLib::Buffer dataBuffer( uiDataSize );
		dataBuffer.putArray( pTexInfo->pData, uiDataSize );
		dataBuffer.rewind();

		CTLib::ImageFormat eCTLibImgFmt;
		CTLib::ImageFormat eCTLibPalFmt = CTLib::ImageFormat::NONE;
		TBYTE*             pLUT         = NULL;

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
			case TTEX_FMT_REV_CI4_RGB5A3:
				eCTLibImgFmt = CTLib::ImageFormat::C4;
				eCTLibPalFmt = CTLib::ImageFormat::RGB5A3;
				break;
			case TTEX_FMT_REV_CI4_RGB565:
				eCTLibImgFmt = CTLib::ImageFormat::C4;
				eCTLibPalFmt = CTLib::ImageFormat::RGB565;
				break;
			case TTEX_FMT_REV_CI8_IA8:
				eCTLibImgFmt = CTLib::ImageFormat::C8;
				eCTLibPalFmt = CTLib::ImageFormat::IA8;
				break;
			case TTEX_FMT_REV_CI8_RGB565:
				eCTLibImgFmt = CTLib::ImageFormat::C8;
				eCTLibPalFmt = CTLib::ImageFormat::RGB565;
				break;
			case TTEX_FMT_REV_CI8_RGB5A3:
				eCTLibImgFmt = CTLib::ImageFormat::C8;
				eCTLibPalFmt = CTLib::ImageFormat::RGB5A3;
				break;
			default:
				TERROR( "Unsupported TTEXTURE_FORMAT format for the '%s' texture!\n", pTexInfo->szFileName );
				continue;
		}

		// Fill palette buffer if it presents
		CTLib::Buffer paletteBuffer;
		if ( eCTLibPalFmt != CTLib::ImageFormat::NONE )
		{
			if ( eCTLibImgFmt == CTLib::ImageFormat::C4 )
			{
				paletteBuffer.create( 16 * 2 );
				paletteBuffer.putArray( pLUT ? pLUT : pTexInfo->pLUTColor, 16 * 2 );
			}
			else if ( eCTLibImgFmt == CTLib::ImageFormat::C8 )
			{
				paletteBuffer.create( 256 * 2 );
				paletteBuffer.putArray( pLUT ? pLUT : pTexInfo->pLUTColor, 256 * 2 );
			}
			else if ( eCTLibImgFmt == CTLib::ImageFormat::C14X2 )
			{
				paletteBuffer.create( 16384 * 2 );
				paletteBuffer.putArray( pLUT ? pLUT : pTexInfo->pLUTColor, 16384 * 2 );
			}
		}

		// Decode image
		try
		{
			CTLib::Image image = CTLib::ImageCoder::decode( dataBuffer, paletteBuffer, uiWidth, uiHeight, eCTLibImgFmt, eCTLibPalFmt );

			// Store data in a buffer we own
			TBYTE* pData = (TBYTE*)TMalloc( image.getData().capacity() );
			TUtil::MemCopy( pData, *image.getData(), image.getData().capacity() );

			rOutVector.EmplaceBack(
			    Resource::StreamedTexture_Create( TPS8D( pTTL->pTextureInfos[ i ].szFileName ), TINT( uiWidth ), TINT( uiHeight ), pData, bCreateTextures )
			);
		}
		catch ( CTLib::ImageError error )
		{
			TERROR( "An error has occured while decoding '%s' texture!\n", pTexInfo->szFileName );
		}
	}

	return TTRUE;
}

void ResourceLoader::TTL_Destroy( Textures& rTextures )
{
	rTextures.FreeMemory();
}

TBOOL ResourceLoader::TTL_UnpackTextures( Textures& rTextures, const TCHAR* szOutDir )
{
	TString8 strOutPath;
	T2_FOREACH( rTextures, it )
	{
		TString8 strFileName = it->Get()->GetTexture().strName.GetString();

		// Fix name of the file
		if ( strFileName.EndsWithNoCase( ".tga" ) )
		{
			strFileName[ strFileName.Length() - 3 ] = 'p';
			strFileName[ strFileName.Length() - 2 ] = 'n';
			strFileName[ strFileName.Length() - 1 ] = 'g';
		}
		else
		{
			strFileName += ".png";
		}

		// Skip all '..\' sequences
		const TCHAR* pchFileName = strFileName;

		TINT iPos = 0;
		while ( iPos = strFileName.Find( "..\\", iPos ), iPos != -1 )
		{
			iPos += 3;
			pchFileName = &strFileName[ iPos ];
		}

		strOutPath.Format( "%s\\%s", szOutDir, pchFileName );
		Toshi::FixPathSlashes( strOutPath );

		// Create directories
		std::filesystem::path path( strOutPath.GetString() );
		std::filesystem::create_directories( path.parent_path() );

		stbi_write_png( strOutPath, it->Get()->GetTexture().iWidth, it->Get()->GetTexture().iHeight, 4, it->Get()->GetTexture().pData, 0 );
	}

	return TTRUE;
}
