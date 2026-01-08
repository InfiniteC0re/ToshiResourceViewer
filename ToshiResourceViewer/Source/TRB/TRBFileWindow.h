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

	TBOOL LoadTRBFile( Toshi::T2StringView strFilePath );
	TBOOL LoadExternalFile( Toshi::T2StringView strFilePath );
	TBOOL SaveFile( Toshi::T2StringView strFilePath, TBOOL bCompress = TFALSE, Endianess eEndianess = Endianess_Little );

	TBOOL LoadExternalResourceView( TRBResourceView* pResourceView );
	void  SetWindowName( Toshi::T2StringView strName );

	void Render( TFLOAT fDeltaTime );
	TBOOL Update();

	Toshi::T2StringView GetFilePath() const { return m_strFilePath.GetString(); }
	Toshi::T2StringView GetFileName() const { return m_strFileName.GetString(); }

private:
	TBOOL LoadInternal( Toshi::T2StringView strFilePath, TBOOL bIsTRB  );
	void UnloadFile();

private:
	Toshi::TString8 m_strWindowName;
	Toshi::TString8 m_strFilePath;
	Toshi::TString8 m_strFileName;
	PTRB*           m_pFile;

	Toshi::T2FormatString128 m_strTRBInfoTabName;

	// Vector of all resource views bound to this file
	Toshi::T2DynamicVector<TRBResourceView*> m_vecResourceViews;

	TBOOL m_bVisible        = TTRUE;
	TBOOL m_bShowSymbols    = TFALSE;
	TBOOL m_bUseCompression = TFALSE;
	TBOOL m_bExternal       = TFALSE;
};
