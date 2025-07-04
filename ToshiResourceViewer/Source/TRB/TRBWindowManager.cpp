#include "pch.h"
#include "TRBWindowManager.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TRBWindowManager::TRBWindowManager()
{
}

TRBWindowManager::~TRBWindowManager()
{
}

void TRBWindowManager::AddWindow( TRBFileWindow* pWindow )
{
	if ( pWindow )
		m_vecWindows.PushBack( pWindow );
}

void TRBWindowManager::Render( TFLOAT fDeltaTime )
{
	T2_FOREACH( m_vecWindows, it )
	{
		TRBFileWindow* pWindow = *it;
		TBOOL          bValid  = pWindow->Update();

		if ( !bValid )
		{
			delete pWindow;
			m_vecWindows.EraseFast( it );
			it--;
			continue;
		}

		pWindow->Render( fDeltaTime );
	}
}

