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

TBOOL TRBResourceView::OnCreate()
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

TBOOL TRBResourceView::Create( PTRB* pTRB, void* pData, const TCHAR* pchSymbolName, const TCHAR* pchFileName )
{
	m_pTRB          = pTRB;
	m_pData         = pData;
	m_strSymbolName = pchSymbolName;
	m_strFileName   = pchFileName;

	return OnCreate();
}

void TRBResourceView::Destroy()
{
	if ( this )
	{
		OnDestroy();
		delete this;
	}
}
