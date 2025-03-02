#include "pch.h"
#include "TRBSymbolManager.h"

#include <ToshiTools/T2DynamicVector.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

static T2DynamicVector<TRBSymbol*>& GetRegisteredSymbols()
{
    static T2DynamicVector<TRBSymbol*> s_vecRegisteredSymbols;
	return s_vecRegisteredSymbols;
}

void TRBSymbolManager::RegisterSymbol( TRBSymbol* pSymbol )
{
	if ( pSymbol && !pSymbol->IsRegistered() && pSymbol->IsComplete() )
	{
		GetRegisteredSymbols().PushBack( pSymbol );
		pSymbol->MarkRegistered();
	}
}

void TRBSymbolManager::UnregisterSymbol( TRBSymbol* pSymbol )
{
	if ( pSymbol )
	{
		GetRegisteredSymbols().FindAndEraseFast( pSymbol );
		pSymbol->MarkUnregistered();
	}
}

TRBSymbol* TRBSymbolManager::GetSymbol( Toshi::T2ConstString8 strName )
{
	T2_FOREACH( GetRegisteredSymbols(), it )
	{
		TRBSymbol* pSymbol = *it;
		
		if ( pSymbol->HasName( strName ) )
			return pSymbol;
	}

	return TNULL;
}
