#include "pch.h"
#include "LocaleResourceView.h"

#include <T2Locale/T2Locale.h>
#include <imgui.h>

#include <string>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

LocaleResourceView::LocaleResourceView()
{
	m_strName = "Locale";
}

LocaleResourceView::~LocaleResourceView()
{
}

TBOOL LocaleResourceView::OnCreate()
{
	T2Locale::LocaleStrings* pLocaleStrings = TSTATICCAST( T2Locale::LocaleStrings, m_pData );

	if ( pLocaleStrings )
	{
		for ( TINT i = 0; i < pLocaleStrings->m_numstrings; i++ )
		{
			LocaleString* pString = new LocaleString();
			pString->iIndex       = i;
			pString->strLocalised = ImGuiUtils::UnicodeToUTF8( pLocaleStrings->Strings[ i ] );

			m_vecStrings.Push( pString );
		}

		return TTRUE;
	}

	return TFALSE;
}

void LocaleResourceView::OnDestroy()
{
	T2_FOREACH( m_vecStrings, it )
	{
		LocaleString* pString = *it;

		delete pString;
	}

	m_vecStrings.Clear();
}

void LocaleResourceView::OnRender( TFLOAT flDeltaTime )
{
	LocaleString* pReOrder1 = TNULL;
	LocaleString* pReOrder2 = TNULL;
	
	T2_FOREACH( m_vecStrings, it )
	{
		LocaleString* pString = *it;

		pString->PreRender();
		TINT iOldIndex = pString->iIndex;
		TINT iNewIndex = iOldIndex;

		ImGui::PushItemWidth( 40.0f );
		ImGui::LabelText( "##ID", "%d", pString->iIndex );
		ImGui::SameLine();

		if ( ImGui::Button( "Down" ) ) iNewIndex--;
		ImGui::SameLine();
		if ( ImGui::Button( "Up" ) ) iNewIndex++;

		if ( iNewIndex != iOldIndex )
		{
			pString->iIndex = iNewIndex;
			
			if ( pString->iIndex < 0 )
				pString->iIndex = 0;
			else if ( iNewIndex < m_vecStrings.Size() )
			{
				m_vecStrings[ iNewIndex ]->iIndex = iOldIndex;
				pString->iIndex                   = iNewIndex;

				pReOrder1 = *it;
				pReOrder2 = m_vecStrings[ iNewIndex ];
			}
		}

		ImGui::SameLine();
		ImGui::PushItemWidth( -1.0f );
		ImGuiUtils::InputText( "##Localised", pString->strLocalised );

		pString->PostRender();
	}

	if ( pReOrder1 )
		m_vecStrings.ReInsert( pReOrder1 );

	if ( pReOrder2 )
		m_vecStrings.ReInsert( pReOrder2 );
}
