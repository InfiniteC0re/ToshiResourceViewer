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

void TRBWindowManager::Render()
{
	T2_FOREACH( m_vecWindows, it )
	{
		( *it )->Render();
	}
}

