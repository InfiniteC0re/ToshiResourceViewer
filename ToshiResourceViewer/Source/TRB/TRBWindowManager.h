#pragma once
#include "TRBFileWindow.h"

#include <ToshiTools/T2DynamicVector.h>

class TRBWindowManager
    : public Toshi::TSingleton<TRBWindowManager>
{
public:
	TRBWindowManager();
	~TRBWindowManager();

	void AddWindow( TRBFileWindow* pWindow );
	void Render( TFLOAT fDeltaTime );

private:
	Toshi::T2DynamicVector<TRBFileWindow*> m_vecWindows;
};
