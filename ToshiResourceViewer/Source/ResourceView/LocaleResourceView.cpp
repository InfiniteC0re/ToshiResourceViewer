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
	m_strName = "Localisation";
}

LocaleResourceView::~LocaleResourceView()
{
}

TBOOL LocaleResourceView::OnCreate()
{
	TRBResourceView::OnCreate();

	T2Locale::LocaleStrings* pLocaleStrings = TSTATICCAST( T2Locale::LocaleStrings, m_pData );

	if ( pLocaleStrings )
	{
		for ( TINT i = 0; i < pLocaleStrings->m_numstrings; i++ )
		{
			LocaleString* pString = new LocaleString();
			pString->strLocalised = ImGuiUtils::UnicodeToUTF8( pLocaleStrings->Strings[ i ] );

			m_vecStrings.Insert( pString, i );
		}

		return TTRUE;
	}

	return TFALSE;
}

void LocaleResourceView::OnDestroy()
{
	while ( !m_vecStrings.IsEmpty() )
		delete m_vecStrings.Begin();

	m_vecStrings.RemoveAll();
}

void LocaleResourceView::OnRender( TFLOAT flDeltaTime )
{
	LocaleString* pReOrder = TNULL;

	// Draw header
	ImGui::PushItemWidth( 40.0f );
	ImGui::Text( "ID" );

	// Draw the localisation strings
	TINT iIndex = 0;
	T2_FOREACH( m_vecStrings, pString )
	{
		pString->PreRender();
		TINT iOldIndex = iIndex;
		TINT iNewIndex = iOldIndex;

		ImGui::PushItemWidth( 40.0f );
		ImGui::LabelText( "##ID", "%d", iIndex );
		ImGui::SameLine();

		if ( ImGui::Button( "Down" ) ) iNewIndex++;
		ImGui::SameLine();
		if ( ImGui::Button( "Up" ) ) iNewIndex--;

		if ( iNewIndex != iOldIndex )
		{
			if ( iNewIndex < 0 )
				iNewIndex = 0;

			auto pPrev = m_vecStrings.GetPrev( pString );
			auto pNext = m_vecStrings.GetNext( pString );

			if ( iNewIndex > iOldIndex && pNext != m_vecStrings.End() )
				iNewIndex = pNext->GetPriority();
			else if ( iNewIndex < iOldIndex && pPrev != m_vecStrings.End() )
				iNewIndex = pPrev->GetPriority() - 1;

			pReOrder = pString;
			pString->SetPriority( iNewIndex );
		}

		ImGui::SameLine();
		ImGui::PushItemWidth( -1.0f );
		ImGuiUtils::InputText( "##Localised", pString->strLocalised );

		pString->PostRender();
		iIndex++;
	}

	if ( pReOrder )
	{
		pReOrder->Remove();
		m_vecStrings.Insert( pReOrder );
	}
}
