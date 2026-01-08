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

TBOOL TRBResourceView::CreateTRB( PTRB* pTRB, void* pData, const TCHAR* pchSymbolName, const TCHAR* pchFilePath, const TCHAR* pchFileName )
{
	m_pTRB          = pTRB;
	m_pData         = pData;
	m_strSymbolName = pchSymbolName;
	m_strFilePath   = pchFilePath;
	m_strFileName   = pchFileName;
	m_bIsExternal   = TFALSE;

	return OnCreate( pchFilePath );
}

TBOOL TRBResourceView::CreateExternal( const TCHAR* pchFilePath, const TCHAR* pchFileName )
{
	m_pTRB          = TNULL;
	m_pData         = TNULL;
	m_strSymbolName = "";
	m_strFilePath   = pchFilePath;
	m_strFileName   = pchFileName;
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
