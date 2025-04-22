#include "pch.h"
#include "ImGuiUtils.h"

#include <Toshi/TString8.h>
#include <ToshiTools/T2DynamicVector.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

static ImGuiID s_uiID = 0;

ImGuiID ImGuiUtils::GetComponentID( ImGuiID uiOffset /*= 1 */ )
{
	ImGuiID uiID = s_uiID;
	
	s_uiID += uiOffset;

	return uiID;
}

Toshi::TString8 ImGuiUtils::UnicodeToUTF8( const Toshi::TString16& wstr )
{
	if ( wstr.Length() )
	{
		TINT sizeRequired = WideCharToMultiByte( CP_UTF8, 0, wstr.GetString(), -1, NULL, 0, NULL, NULL );

		if ( sizeRequired > 0 )
		{
			T2DynamicVector<TCHAR> utf8String;
			utf8String.SetSize( sizeRequired );

			TINT bytesConverted = WideCharToMultiByte( CP_UTF8, 0, wstr.GetString(), -1, &utf8String[ 0 ], sizeRequired, NULL, NULL );
		
			if ( bytesConverted != 0 )
				return &utf8String[ 0 ];
		}
	}

	return "";
}

static TINT InputTextTString8Callback( ImGuiInputTextCallbackData* data )
{
	TString8* pString = (TString8*)data->UserData;

	if ( data->EventFlag == ImGuiInputTextFlags_CallbackEdit )
		pString->Copy( data->Buf );
	if ( data->EventFlag == ImGuiInputTextFlags_CallbackResize )
		pString->Reserve( ( data->BufSize - 1 ) * 2 );

	return 0;
}

TBOOL ImGuiUtils::InputText( Toshi::T2StringView label, Toshi::TString8& string, ImGuiInputTextFlags flags /*= 0 */ )
{
	return ImGui::InputText(
	    label,
	    string.GetStringUnsafe(),
	    string.Length() + string.ExcessLength() + 1,
	    ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackResize | flags,
	    InputTextTString8Callback,
		&string
	);
}

void ImGuiUtils::ImGuiComponent::PreRender( ImGuiID uiIndex ) const
{
	ImGui::PushID( uiComponentID + uiIndex );
}

void ImGuiUtils::ImGuiComponent::PostRender( ImGuiID uiIndex ) const
{
	ImGui::PopID();
}
