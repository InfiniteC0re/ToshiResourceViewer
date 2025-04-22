#pragma once
#include "TRBResourceView.h"
#include "ImGuiUtils.h"

#include <Plugins/PTRB.h>
#include <Toshi/T2String.h>

class TRBFileWindow
    : public ImGuiUtils::ImGuiComponent
{
public:
	TRBFileWindow();
	~TRBFileWindow();

	TBOOL LoadFile( Toshi::T2StringView strFilePath );
	TBOOL SaveFile( Toshi::T2StringView strFilePath, TBOOL bCompress = TFALSE, Endianess eEndianess = Endianess_Little );

	void Render();
	TBOOL Update();

private:
	void UnloadFile();

private:
	Toshi::TString8 m_strWindowName;
	Toshi::TString8 m_strFilePath;
	PTRB*           m_pFile;

	Toshi::T2FormatString128 m_strTRBInfoTabName;

	// Vector of all resource views bound to this file
	Toshi::T2DynamicVector<TRBResourceView*> m_vecResourceViews;

	TBOOL m_bVisible        = TTRUE;
	TBOOL m_bShowSymbols    = TFALSE;
	TBOOL m_bUseCompression = TFALSE;
};
