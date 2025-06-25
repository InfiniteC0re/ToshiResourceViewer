#include "pch.h"
#include "TextureTool.h"
#include "ResourceLoader/TextureLoader.h"

#include <ToshiTools/T2DynamicVector.h>
#include <Toshi/T2String.h>
#include <Plugins/PTRB.h>

#include <imgui.h>
#include <filesystem>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

static TBOOL                     s_bVisible = TFALSE;
static T2DynamicVector<TString8> s_vecInputFiles;

void TextureTool::Show()
{
	s_bVisible = TTRUE;
}

void TextureTool::Hide()
{
	s_bVisible = TFALSE;
}

void TextureTool::Toggle()
{
	s_bVisible = !s_bVisible;
}

void TextureTool::Render()
{
	if ( !s_bVisible )
		return;

	ImGui::SetNextWindowSize( ImVec2( 0.0f, 0.0f ), ImGuiCond_Appearing );
	ImGui::Begin( "Texture Tool", &s_bVisible );
	
	static TCHAR s_DirInBuffer[ 300 ] = "D:\\Barnyard\\Barnyard - Shuyaku wa Ore, Ushi (Japan)\\Japan\\files\\Data\\Matlibs";
	ImGui::InputText( "Input Directory", s_DirInBuffer, sizeof( s_DirInBuffer ) );

	static TCHAR s_DirOutBuffer[ 300 ] = ".\\Output";
	ImGui::InputText( "Output Directory", s_DirOutBuffer, sizeof( s_DirOutBuffer ) );

	if ( ImGui::BeginListBox( "##Input Files", ImVec2( -FLT_MIN, 0.0f ) ) )
	{
		T2_FOREACH( s_vecInputFiles, it )
		{
			ImGui::Selectable( *it );
		}

		ImGui::EndListBox();
	}

	if ( ImGui::Button( "Scan Files" ) )
	{
		s_vecInputFiles.Clear();

		for ( const auto& entry : std::filesystem::directory_iterator( s_DirInBuffer ) )
		{
			if ( !entry.path().extension().compare( ".trb" ) ||
				 !entry.path().extension().compare( ".ttl" ) )
			{
				char szPath[ MAX_PATH ];
				TStringManager::StringUnicodeToChar( szPath, entry.path().native().c_str(), -1 );

				PTRB trb;
				TBOOL bRead = trb.ReadFromFile( szPath );

				if ( !bRead )
					continue;
				
				PTRBSymbols* pSymbols = trb.GetSymbols();

				for ( auto& strSymbolName : *pSymbols )
				{
					if ( !strSymbolName.CompareNoCase( "ttl" ) || strSymbolName.EndsWithNoCase( "_ttl" ) )
					{
						s_vecInputFiles.PushBack( szPath );
						break;
					}
				}
			}
		}
	}

	ImGui::SameLine();
	
	ImGui::BeginDisabled( s_DirOutBuffer[ 0 ] == '\0' );
	if ( ImGui::Button( "Extract" ) )
	{
		T2_FOREACH( s_vecInputFiles, it )
		{
			PTRB  trb;
			TBOOL bRead = trb.ReadFromFile( it->GetString() );

			if ( !bRead )
				continue;

			PTRBSymbols* pSymbols = trb.GetSymbols();

			for ( TUINT i = 0; i < pSymbols->GetCount(); i++ )
			{
				TString8 strSymbolName = pSymbols->GetName( i ).Get();

				if ( !strSymbolName.CompareNoCase( "ttl" ) || strSymbolName.EndsWithNoCase( "_ttl" ) )
				{
					// Load textures and unpack them
					ResourceLoader::Textures vecTextures;

					if ( ResourceLoader::TTL_Load( pSymbols->GetByIndex<TBYTE>( trb.GetSections(), i ).get(), trb.GetEndianess(), TFALSE, TFALSE, vecTextures, TNULL ) )
					{
						ResourceLoader::TTL_UnpackTextures( vecTextures, s_DirOutBuffer );
						ResourceLoader::TTL_Destroy( vecTextures );
					}
				}
			}
		}
	}
	ImGui::EndDisabled();

	ImGui::End();
}
