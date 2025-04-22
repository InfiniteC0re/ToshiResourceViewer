#pragma once
#include "TRB/TRBResourceView.h"
#include "ImGuiUtils.h"

#include <ToshiTools/T2DynamicVector.h>
#include <Toshi/TDList.h>
#include <Toshi/TString16.h>

class LocaleResourceView
    : public TRBResourceView
{
public:
	LocaleResourceView();
	~LocaleResourceView();

	virtual TBOOL OnCreate() OVERRIDE;
	virtual void  OnDestroy() OVERRIDE;
	virtual void  OnRender( TFLOAT flDeltaTime ) OVERRIDE;

private:
	struct LocaleString
	    : public ImGuiUtils::ImGuiComponent
	    , public Toshi::TPriList<LocaleString>::TNode
	{
		Toshi::TString8 strLocalised;
	};

	Toshi::TPriList<LocaleString> m_vecStrings;
};
