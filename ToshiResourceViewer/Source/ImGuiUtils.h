#pragma once
#include <Toshi/TString8.h>
#include <Toshi/TString16.h>
#include <Toshi/T2String.h>

#include <imgui.h>

namespace ImGuiUtils
{

// Returns an unique ID
ImGuiID GetComponentID( ImGuiID uiOffset = 1 );

Toshi::TString8 UnicodeToUTF8( const Toshi::TString16& wstr );

TBOOL InputText( Toshi::T2ConstString8 label, Toshi::TString8& string, ImGuiInputTextFlags flags = 0 );

struct ImGuiComponent
{
	ImGuiID uiComponentID = GetComponentID();

	ImGuiID GetImGuiID() const { return uiComponentID; }
	void    PreRender() const;
	void    PostRender() const;
};

}
