#include "pch.h"
#include "TRBSymbol.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TRBSymbol::TRBSymbol()
    : m_fnCreateResourceView( TNULL )
    , m_bIsRegistered( TFALSE )
{
}

TRBSymbol::~TRBSymbol()
{
}

void TRBSymbol::AddName( Toshi::T2StringView strName )
{
	if ( strName )
		m_vecNames.PushBack( strName );
}

TBOOL TRBSymbol::HasName( Toshi::T2StringView strName )
{
	if ( !strName )
		return TFALSE;

	T2_FOREACH( m_vecNames, it )
	{
		if ( strName == *it )
			return TTRUE;
	}

	return TFALSE;
}

TRBResourceView* TRBSymbol::CreateResourceView()
{
	return ( m_fnCreateResourceView ) ? m_fnCreateResourceView() : TNULL;
}
