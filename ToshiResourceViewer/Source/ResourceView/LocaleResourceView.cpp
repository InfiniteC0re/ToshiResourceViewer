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

std::string WstrToUtf8Str( const std::wstring& wstr )
{
	std::string retStr;
	if ( !wstr.empty() )
	{
		int sizeRequired = WideCharToMultiByte( CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL );

		if ( sizeRequired > 0 )
		{
			std::vector<char> utf8String( sizeRequired );
			int               bytesConverted = WideCharToMultiByte( CP_UTF8, 0, wstr.c_str(), -1, &utf8String[ 0 ], utf8String.size(), NULL, NULL );
			if ( bytesConverted != 0 )
			{
				retStr = &utf8String[ 0 ];
			}
		}
	}
	return retStr;
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
			pString->strLocalised = WstrToUtf8Str( pLocaleStrings->Strings[ i ] ).c_str();

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
	T2_FOREACH( m_vecStrings, it )
	{
		LocaleString* pString = *it;
		ImGui::Text( "%d", pString->iIndex );
		ImGui::SameLine();
		ImGui::Text( pString->strLocalised.GetString() );
	}
}
