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

	virtual TBOOL OnCreate()                     = 0;
	virtual void  OnDestroy()                    = 0;
	virtual void  OnRender( TFLOAT flDeltaTime ) = 0;
	virtual TBOOL CanSave();
	virtual TBOOL OnSave( PTRB* pOutTRB );

	TBOOL Create( PTRB* pTRB, void* pData );
	void  Destroy();

	Toshi::T2ConstString8 GetName() const { return m_strName; }

protected:
	Toshi::T2ConstString8 m_strName = "Resource View";
	PTRB*                 m_pTRB;   // optional pointer to the TRB file
	void*                 m_pData;  // optional pointer to the data of this linked symbol
	TRBSymbol*            m_pOwner; // pointer to the registered symbol that is capable of creating this view
};
