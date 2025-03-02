#pragma once
#include "TRB/TRBResourceView.h"

#include <ToshiTools/T2DynamicVector.h>
#include <Toshi/T2SortedVector.h>
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
	{
		TINT            iIndex;
		Toshi::TString8 strLocalised;

		TBOOL operator<( LocaleString& rcOther ) const
		{
			return iIndex < rcOther.iIndex;
		}
	};

	struct LocaleStringSort
	{
		TINT operator()( LocaleString* const& a_rcVal1, LocaleString* const& a_rcVal2 ) const
		{
			return a_rcVal1->iIndex - a_rcVal2->iIndex;
		}
	};

	Toshi::T2SortedVector<LocaleString*, Toshi::T2DynamicVector<LocaleString*>, LocaleStringSort> m_vecStrings;
};
