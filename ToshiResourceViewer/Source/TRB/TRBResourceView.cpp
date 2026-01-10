#include "pch.h"
#include "TRBResourceView.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TRBResourceView::TRBResourceView()
{
}

TRBResourceView::~TRBResourceView()
{
}

TBOOL TRBResourceView::OnCreate( Toshi::T2StringView pchFilePath )
{
	// Create unique name
	m_strNameId.Format( "%s##%u", m_strName.GetString(), GetImGuiID() );

	return TTRUE;
}

TBOOL TRBResourceView::CanSave()
{
	return TFALSE;
}

TBOOL TRBResourceView::OnSave( PTRB* pOutTRB )
{
	return TFALSE;
}

TBOOL TRBResourceView::CreateTRB( PTRB* pTRB, void* pData, const TCHAR* pchSymbolName, const TCHAR* pchFilePath )
{
	TString8   strFilePath     = TString8( pchFilePath );
	const TINT iLastSlashIndex = strFilePath.FindReverse( '\\' );

	m_pTRB          = pTRB;
	m_pData         = pData;
	m_strSymbolName = pchSymbolName;
	m_strFilePath   = pchFilePath;
	m_strFileName   = ( iLastSlashIndex != -1 ) ? TString8( strFilePath.GetString( iLastSlashIndex + 1 ) ) : std::move( strFilePath );
	m_bIsExternal   = TFALSE;

	return OnCreate( pchFilePath );
}

TBOOL TRBResourceView::CreateExternal( const TCHAR* pchFilePath )
{
	TString8   strFilePath     = TString8( pchFilePath );
	const TINT iLastSlashIndex = strFilePath.FindReverse( '\\' );

	m_pTRB          = TNULL;
	m_pData         = TNULL;
	m_strSymbolName = "";
	m_strFilePath   = pchFilePath;
	m_strFileName   = ( iLastSlashIndex != -1 ) ? TString8( strFilePath.GetString( iLastSlashIndex + 1 ) ) : std::move( strFilePath );
	m_bIsExternal   = TFALSE;
	m_bIsExternal   = TTRUE;

	return OnCreate( pchFilePath );
}

void TRBResourceView::Destroy()
{
	if ( this )
	{
		OnDestroy();
		delete this;
	}
}
