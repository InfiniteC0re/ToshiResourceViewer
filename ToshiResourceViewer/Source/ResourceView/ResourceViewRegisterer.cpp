#include "pch.h"
#include "TRB/TRBSymbolManager.h"
#include "LocaleResourceView.h"
#include "TextureResourceView.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

static TBOOL s_bRegistered = TFALSE;

static struct ResourceViewRegisterer
{

	ResourceViewRegisterer()
	{
		TASSERT( TFALSE == s_bRegistered );

		if ( s_bRegistered )
			return;

		{
			// Locale Resource
			TRBSymbol* pSymbol = new TRBSymbol();
			pSymbol->AddName( "LocaleStrings" );
			pSymbol->SetFactoryMethod( []() -> TRBResourceView* {
				return new LocaleResourceView();
			} );

			TRBSymbolManager::RegisterSymbol( pSymbol );
		}

		{
			// Texture Library Resource
			TRBSymbol* pSymbol = new TRBSymbol();
			pSymbol->AddName( "TTL" );
			pSymbol->SetFactoryMethod( []() -> TRBResourceView* {
				return new TextureResourceView();
			} );

			TRBSymbolManager::RegisterSymbol( pSymbol );
		}

		s_bRegistered = TTRUE;
	}

} s_oRegisterer;
