#include "pch.h"
#include "TextureLoader.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

static TFORCEINLINE TUINT8 Expand5To8( TUINT16 value )
{
	return TUINT8( ( value << 3 ) | ( value >> 2 ) );
}

static TUINT DecodePS2ClutIndexForLayout( TUINT8 uiIndex, TUINT uiClutWidth )
{
	switch (uiClutWidth)
	{
		case 0x20:
		case 0x40:
			return TUINT8( ( uiIndex & 0xE7u ) | ( ( uiIndex & 0x08u ) << 1 ) | ( ( uiIndex & 0x10u ) >> 1 ) );

		case 0x80:
			// source bits 01234567 -> destination bits 01253674
			return TUINT(
			    ( uiIndex & 0x07u ) |
			    ( ( uiIndex & 0x08u ) << 2 ) |
			    ( ( uiIndex & 0x10u ) >> 1 ) |
			    ( ( uiIndex & 0x20u ) << 1 ) |
			    ( ( uiIndex & 0x40u ) << 1 ) |
			    ( ( uiIndex & 0x80u ) >> 3 )
			);

		case 0x100:
			// source bits 01234567 -> destination bits 01253678, with destination bit 9 set
			return TUINT(
			    0x200u |
			    ( uiIndex & 0x07u ) |
			    ( ( uiIndex & 0x08u ) << 2 ) |
			    ( ( uiIndex & 0x10u ) >> 1 ) |
			    ( ( uiIndex & 0x20u ) << 1 ) |
			    ( ( uiIndex & 0x40u ) << 1 ) |
			    ( ( uiIndex & 0x80u ) << 1 )
			);
	}

	return uiIndex;
}

static TBOOL InferPS2TextureDimensions( TUINT uiFormat, TUINT uiPixelDataSize, TUINT uiGSWidth, TUINT uiGSHeight, TUINT& uiOutWidth, TUINT& uiOutHeight )
{
	if ( uiFormat == TTEX_FMT_PS2_PSMT8_RGB888 )
	{
		uiOutWidth  = uiGSWidth;
		uiOutHeight = uiGSHeight;
	}
	else
	{
		uiOutWidth  = uiGSWidth >> 1;
		uiOutHeight = uiGSHeight >> 1;
	}

	return TTRUE;
}

static void ReorderClut16Block( TUINT16* pDstWords, TUINT uiRowWords, const TUINT16* pSrcWords, TUINT uiSourceStrideWords, TUINT uiTotalBytes )
{
	// Barnyard (PS2 Korean): 0x00486388
	while ( uiTotalBytes != 0 )
	{
		uiTotalBytes -= 0x20;

		for ( TUINT i = 0; i < 8; i++ )
		{
			pDstWords[ i ] = *pSrcWords;
			pSrcWords += uiSourceStrideWords;
		}

		for ( TUINT i = 0; i < 8; i++ )
		{
			pDstWords[ uiRowWords + i ] = *pSrcWords;
			pSrcWords += uiSourceStrideWords;
		}

		for ( TUINT i = 0; i < 8; i++ )
		{
			pDstWords[ 8 + i ] = *pSrcWords;
			pSrcWords += uiSourceStrideWords;
		}

		for ( TUINT i = 0; i < 8; i++ )
		{
			pDstWords[ uiRowWords + 8 + i ] = *pSrcWords;
			pSrcWords += uiSourceStrideWords;
		}

		pDstWords += uiRowWords * 2;
	}
}

static TBOOL UploadPS2ClutToMode2Layout( const TUINT8* pClutData, TUINT uiLayoutWidth, TUINT16* pOutClut )
{
	if ( pClutData == TNULL || pOutClut == TNULL )
		return TFALSE;

	const TUINT16* pClutWords = TREINTERPRETCAST( const TUINT16*, pClutData );

	switch (uiLayoutWidth)
	{
		case 0x20:
			ReorderClut16Block( pOutClut, 0x10, pClutWords, 4, 0x20 );
			ReorderClut16Block( pOutClut + 0x20, 0x10, pClutWords + 1, 4, 0x20 );
			ReorderClut16Block( pOutClut + 0x40, 0x10, pClutWords + 2, 4, 0x20 );
			ReorderClut16Block( pOutClut + 0x60, 0x10, pClutWords + 3, 4, 0x20 );
			break;
		case 0x40:
			ReorderClut16Block( pOutClut, 0x10, pClutWords, 4, 0x40 );
			ReorderClut16Block( pOutClut + 0x40, 0x10, pClutWords + 1, 4, 0x40 );
			ReorderClut16Block( pOutClut + 0x80, 0x10, pClutWords + 2, 4, 0x40 );
			ReorderClut16Block( pOutClut + 0xc0, 0x10, pClutWords + 3, 4, 0x40 );
			break;
		case 0x80:
			ReorderClut16Block( pOutClut, 0x20, pClutWords, 4, 0x80 );
			ReorderClut16Block( pOutClut + 0x100, 0x20, pClutWords + 1, 4, 0x80 );
			ReorderClut16Block( pOutClut + 0x10, 0x20, pClutWords + 2, 4, 0x80 );
			ReorderClut16Block( pOutClut + 0x110, 0x20, pClutWords + 3, 4, 0x80 );
			break;
		case 0x100:
			ReorderClut16Block( pOutClut, 0x20, pClutWords, 4, 0x100 );
			ReorderClut16Block( pOutClut + 0x200, 0x20, pClutWords + 1, 4, 0x100 );
			ReorderClut16Block( pOutClut + 0x10, 0x20, pClutWords + 2, 4, 0x100 );
			ReorderClut16Block( pOutClut + 0x210, 0x20, pClutWords + 3, 4, 0x100 );
			break;
	}

	return TTRUE;
}

TBOOL ResourceLoader::TTL_Load_Barnyard_PS2( void* pData, Endianess eEndianess, TBOOL bCreateTextures, Textures& rOutVector, TString8* pOutName /*= TNULL */ )
{
	TTL_PS2* pTTL = TSTATICCAST( TTL_PS2, pData );
	if ( !pTTL ) return TFALSE;

	TUINT uiNumTextures = CONVERTENDIANESS( eEndianess, pTTL->uiNumTextures );
	if ( uiNumTextures == 0 ) return TFALSE;

	rOutVector.Reserve( uiNumTextures );

	for ( TUINT i = 0; i < uiNumTextures; i++ )
	{
		TTL_PS2::TexInfo* pTex = &pTTL->pTextureInfos[ i ];

		const TUINT uiFormat        = CONVERTENDIANESS( eEndianess, pTex->uiFormat );
		const TUINT uiGSWidth       = CONVERTENDIANESS( eEndianess, pTex->uiGSWidth );
		const TUINT uiGSHeight      = CONVERTENDIANESS( eEndianess, pTex->uiGSHeight );
		const TUINT uiPixelDataSize = CONVERTENDIANESS( eEndianess, pTex->uiPixelDataSize );
		const TUINT uiClutWidth     = CONVERTENDIANESS( eEndianess, pTex->uiCLUTWidth );
		const TUINT uiClutHeight    = CONVERTENDIANESS( eEndianess, pTex->uiCLUTHeight );

		if ( uiFormat != TTEX_FMT_PS2_PSMT8_RGB888 && uiFormat != TTEX_FMT_PS2_PSMT8_PSMCT16 )
		{
			TERROR( "TTL_Load_Barnyard_PS2: unsupported texture format 0x%X in '%s'\n", uiFormat, pTex->szFileName );
			continue;
		}

		const TUINT8* pRawPixels = TSTATICCAST( const TUINT8, pTex->pPixelData );
		const TUINT8* pRawClut   = pTex->pCLUT != TNULL
			? TSTATICCAST( const TUINT8, pTex->pCLUT )
			: TSTATICCAST( const TUINT8, pTex->pPixelData ) + uiPixelDataSize;

		if ( uiPixelDataSize == 0 || pRawPixels == TNULL || pRawClut == TNULL )
		{
			TERROR( "TTL_Load_Barnyard_PS2: missing pixel/CLUT data in '%s'\n", pTex->szFileName );
			continue;
		}

		TUINT uiWidth  = 0;
		TUINT uiHeight = 0;
		if ( !InferPS2TextureDimensions( uiFormat, uiPixelDataSize, uiGSWidth, uiGSHeight, uiWidth, uiHeight ) )
		{
			TERROR( "TTL_Load_Barnyard_PS2: couldn't infer dimensions for '%s' from %u bytes\n", pTex->szFileName, uiPixelDataSize );
			continue;
		}

		// Result image data
		TBYTE* pImgData = TNULL;

		switch (uiFormat)
		{
			case TTEX_FMT_PS2_PSMT8_RGB888:
			{
				pImgData = TSTATICCAST( TBYTE, TMalloc( uiWidth * uiHeight * 4 ) );

				for ( TUINT px = 0; px < uiWidth * uiHeight; px++ )
				{
					const TUINT8* pColor   = pRawClut + pRawPixels[ px ] * 3;
					pImgData[ px * 4 + 0 ] = pColor[ 0 ];
					pImgData[ px * 4 + 1 ] = pColor[ 1 ];
					pImgData[ px * 4 + 2 ] = pColor[ 2 ];
					pImgData[ px * 4 + 3 ] = 255;
				}

				break;
			}
			
			case TTEX_FMT_PS2_PSMT8_PSMCT16:
			{
				const TUINT uiClutEntries  = uiClutWidth * uiClutHeight;
				TUINT16*    pReorderedClut = TSTATICCAST( TUINT16, TMalloc( sizeof( TUINT16 ) * uiClutEntries ) );

				if ( !UploadPS2ClutToMode2Layout( pRawClut, uiClutWidth, pReorderedClut ) )
				{
					TERROR( "TTL_Load_Barnyard_PS2: failed to upload CLUT layout for '%s'\n", pTex->szFileName );
					TFree( pReorderedClut );
					continue;
				}

				pImgData = TSTATICCAST( TBYTE, TMalloc( uiWidth * uiHeight * 4 ) );

				for ( TUINT px = 0; px < uiWidth * uiHeight; px++ )
				{
					const TUINT uiClutIndex = DecodePS2ClutIndexForLayout( pRawPixels[ px ], uiClutWidth );
					TUINT16     uiClutEntry = uiClutIndex < uiClutEntries ? pReorderedClut[ uiClutIndex ] : 0xFC1F;

					pImgData[ px * 4 + 0 ] = Expand5To8( ( uiClutEntry >> 0 ) & 0x1F );
					pImgData[ px * 4 + 1 ] = Expand5To8( ( uiClutEntry >> 5 ) & 0x1F );
					pImgData[ px * 4 + 2 ] = Expand5To8( ( uiClutEntry >> 10 ) & 0x1F );
					pImgData[ px * 4 + 3 ] = ( uiClutEntry & 0x8000 ) != 0 ? 255 : 0;
				}

				TFree( pReorderedClut );
				break;
			}
		}

		TASSERT( pImgData != TNULL );
		rOutVector.EmplaceBack(
		    Resource::StreamedTexture_Create( TPS8D( pTex->szFileName ), TINT( uiWidth ), TINT( uiHeight ), pImgData, bCreateTextures )
		);
	}

	return TTRUE;
}
