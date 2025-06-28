#pragma once
#include "Resource/StreamedKeyLib.h"
#include "TRB/TRBResourceView.h"

#include <Toshi/T2SharedPtr.h>

class KeyLibResourceView
    : public TRBResourceView
{
public:
	KeyLibResourceView();
	~KeyLibResourceView();

	virtual TBOOL OnCreate() OVERRIDE;
	virtual TBOOL CanSave() OVERRIDE;
	virtual TBOOL OnSave( PTRB* pOutTRB ) OVERRIDE;
	virtual void  OnDestroy() OVERRIDE;
	virtual void  OnRender( TFLOAT flDeltaTime ) OVERRIDE;

private:
	Toshi::T2SharedPtr<Resource::StreamedKeyLib> m_pKeyLib;
};
