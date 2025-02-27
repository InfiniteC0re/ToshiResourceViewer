#include "pch.h"
#include "WindowManager.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

WindowManager::WindowManager()
{
}

WindowManager::~WindowManager()
{
}

void WindowManager::AddWindow( TRBFileWindow* pWindow )
{
	if ( pWindow )
		m_vecWindows.PushBack( pWindow );
}

void WindowManager::Render()
{
	T2_FOREACH( m_vecWindows, it )
	{
		( *it )->Render();
	}
}

