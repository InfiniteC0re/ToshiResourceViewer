#pragma once
#include "TRBResourceView.h"

#include <Plugins/PTRB.h>
#include <Toshi/T2String.h>

class TRBFileWindow
{
public:
	TRBFileWindow();
	~TRBFileWindow();

	TBOOL LoadFile( Toshi::T2ConstString8 strFileName );

	void Render();

private:
	void UnloadFile();

private:
	Toshi::TString8 m_strFilePath;
	PTRB*           m_pFile;

	// Vector of all resource views bound to this file
	Toshi::T2DynamicVector<TRBResourceView*> m_vecResourceViews;

	TBOOL m_bHidden         = TFALSE;
	TBOOL m_bShowSymbols    = TFALSE;
	TBOOL m_bUseCompression = TFALSE;
};
