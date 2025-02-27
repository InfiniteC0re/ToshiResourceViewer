#pragma once
#include "TRBFileWindow.h"

#include <ToshiTools/T2DynamicVector.h>

class WindowManager
    : public Toshi::TSingleton<WindowManager>
{
public:
	WindowManager();
	~WindowManager();

	void AddWindow( TRBFileWindow* pWindow );
	void Render();

private:
	Toshi::T2DynamicVector<TRBFileWindow*> m_vecWindows;
};
