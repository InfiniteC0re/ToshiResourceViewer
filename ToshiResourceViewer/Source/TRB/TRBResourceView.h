#pragma once
#include "TRBSymbol.h"
#include "ImGuiUtils.h"

#include <Toshi/T2String.h>
#include <Plugins/PTRB.h>

class TRBResourceView
    : public ImGuiUtils::ImGuiComponent
{
public:
	TRBResourceView();
	virtual ~TRBResourceView();

	virtual TBOOL OnCreate( Toshi::T2StringView pchFilePath );
	virtual void  OnDestroy()                    = 0;
	virtual void  OnRender( TFLOAT flDeltaTime ) = 0;
	virtual TBOOL CanSave();
	virtual TBOOL OnSave( PTRB* pOutTRB );

	TBOOL CreateTRB( PTRB* pTRB, void* pData, const TCHAR* pchSymbolName, const TCHAR* pchFilePath );
	TBOOL CreateExternal( const TCHAR* pchFilePath );
	void  Destroy();

	Toshi::T2StringView GetName() const { return m_strName.GetString(); }
	Toshi::T2StringView GetNameId() const { return m_strNameId.Get(); }

	const Toshi::TString8& GetFilePath() const { return m_strFilePath; }
	const Toshi::TString8& GetFileName() const { return m_strFileName; }

	template <typename T>
	T ConvertEndianess( T a_numValue )
	{
		return m_pTRB->ConvertEndianess( a_numValue );
	}

protected:
	Toshi::TString8         m_strName = "Resource View";
	Toshi::T2FormatString64 m_strNameId;
	Toshi::TString8         m_strFilePath;
	Toshi::TString8         m_strFileName;
	PTRB*                   m_pTRB;   // optional pointer to the TRB file
	void*                   m_pData;  // optional pointer to the data of this linked symbol
	TRBSymbol*              m_pOwner; // pointer to the registered symbol that is capable of creating this view
	Toshi::TString8         m_strSymbolName;
	TBOOL                   m_bIsExternal;
};
