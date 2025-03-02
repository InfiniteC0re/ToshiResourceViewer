#pragma once
#include <Toshi/T2String.h>
#include <ToshiTools/T2DynamicVector.h>

class TRBResourceView;

class TRBSymbol
{
public:
	using t_CreateResourceView = TRBResourceView*(*)(void);

public:
	TRBSymbol();
	~TRBSymbol();

	void  AddName( Toshi::T2ConstString8 strName );
	TBOOL HasName( Toshi::T2ConstString8 strName );

	TRBResourceView* CreateResourceView();
	void SetFactoryMethod( t_CreateResourceView fnCreateResourceView ) { m_fnCreateResourceView = fnCreateResourceView; }

	TBOOL IsRegistered() const { return m_bIsRegistered; }
	void  MarkRegistered() { m_bIsRegistered = TTRUE; }
	void  MarkUnregistered() { m_bIsRegistered = TFALSE; }

	TBOOL IsComplete() const { return m_vecNames.Size() && m_fnCreateResourceView; }

private:
	Toshi::T2DynamicVector<Toshi::T2ConstString8> m_vecNames;
	t_CreateResourceView                          m_fnCreateResourceView;
	TBOOL                                         m_bIsRegistered;
};
