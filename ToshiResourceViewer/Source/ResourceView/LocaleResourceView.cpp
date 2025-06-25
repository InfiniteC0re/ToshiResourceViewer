#include "pch.h"
#include "LocaleResourceView.h"

#include <T2Locale/T2Locale.h>
#include <ToshiTools/T2DynamicVector.h>
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
		TINT iNumStrings = ConvertEndianess( pLocaleStrings->m_numstrings );
		for ( TINT i = 0; i < iNumStrings; i++ )
		{
			TINT iStringLength = T2String16::Length( pLocaleStrings->Strings[ i ] );
			TString16 strLocalisedString( iStringLength );

			for ( TINT k = 0; k < iStringLength; k++ )
				strLocalisedString[ k ] = ConvertEndianess( pLocaleStrings->Strings[ i ][ k ] );

			strLocalisedString[ iStringLength ] = L'\0';

			LocaleString* pString = new LocaleString();
			pString->strLocalised = ImGuiUtils::UnicodeToUTF8( strLocalisedString );

			m_vecStrings.Insert( pString, i );
		}

		return TTRUE;
	}

	return TFALSE;
}

TBOOL LocaleResourceView::CanSave()
{
	// This file supports saving
	return TTRUE;
}

TBOOL LocaleResourceView::OnSave( PTRB* pOutTRB )
{
	if ( !pOutTRB )
		return TFALSE;

	PTRBSections* pSections = pOutTRB->GetSections();
	PTRBSymbols*  pSymbols  = pOutTRB->GetSymbols();
	
	// Check if the file already contains some locale strings
	if ( pSymbols->Find<T2Locale::LocaleStrings>( pSections, "LocaleStrings" ) )
		return TFALSE;

	PTRBSections::MemoryStream* pMemStream = pSections->GetStack( 0 );

	auto pLocaleStrings = pMemStream->Alloc<T2Locale::LocaleStrings>();

	// Convert all strings into UTF16
	T2DynamicVector<TString16> foundStrings( GetGlobalAllocator(), 1024, 2048 );

	T2_FOREACH( m_vecStrings, str )
	{
		foundStrings.PushBack( Platform_UTF8ToUnicode( str->strLocalised ) );
	}

	// Fill header of the file
	pLocaleStrings->m_numstrings = pOutTRB->ConvertEndianess( foundStrings.Size() );
	pMemStream->Alloc<T2LocalisedString>( &pLocaleStrings->Strings, foundStrings.Size() );

	// Copy strings to the file
	TINT iIndex = 0;
	T2_FOREACH( foundStrings, str )
	{
		pMemStream->Alloc<TWCHAR>( &pLocaleStrings->Strings[ iIndex ], str->Length() + 1 );

		TWCHAR* pDstStr = pLocaleStrings->Strings[ iIndex ];
		const TWCHAR* pSrcStr = *str;

		// Copy string taking endianess into account
		while ( *pSrcStr != L'\0' )
		{
			*pDstStr = pOutTRB->ConvertEndianess( *pSrcStr );
			pSrcStr++;
			pDstStr++;
		}

		iIndex++;
	}

	// Add LocaleStrings symbol to the TRB
	pSymbols->Add( pMemStream, "LocaleStrings", pLocaleStrings.get() );

	return TTRUE;
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
