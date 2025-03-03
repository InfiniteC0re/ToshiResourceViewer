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

TBOOL TRBResourceView::CanSave()
{
	return TFALSE;
}

TBOOL TRBResourceView::OnSave( PTRB* pOutTRB )
{
	return TFALSE;
}

TBOOL TRBResourceView::Create( PTRB* pTRB, void* pData )
{
	m_pTRB  = pTRB;
	m_pData = pData;

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
