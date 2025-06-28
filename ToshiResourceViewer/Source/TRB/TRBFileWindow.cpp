#include "pch.h"
#include "TRBFileWindow.h"
#include "TRBSymbolManager.h"
#include "Application.h"
#include "imgui.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TRBFileWindow::TRBFileWindow()
    : m_pFile( TNULL )
{
}

TRBFileWindow::~TRBFileWindow()
{
	UnloadFile();
}

TBOOL TRBFileWindow::LoadFile( Toshi::T2StringView strFilePath )
{
	UnloadFile();
	m_pFile = new PTRB;

	m_strFilePath = strFilePath;
	FixPathSlashes( m_strFilePath );

	TINT     iLastSlashIndex = m_strFilePath.FindReverse( '\\' );
	TString8 strFileName     = ( iLastSlashIndex != -1 ) ? TString8( m_strFilePath.GetString( iLastSlashIndex + 1 ) ) : m_strFilePath;

	// Setup tab names
	m_strTRBInfoTabName.Format( "%s##%u", "TRB Information", GetImGuiID() );

	TBOOL bReadFile = m_pFile->ReadFromFile( m_strFilePath.GetString() );

	if ( bReadFile )
	{
		if ( m_pFile->GetEndianess() == Endianess_Big )
		{
			// [6/25/2025 InfiniteC0re]
			// HACK: since the only supported Big Endian platform is Wii, assume we are trying to load a Wii file
			g_oTheApp.SetSelectedPlatform( TOSHISKU_REV );
		}
		else
		{
			g_oTheApp.SetSelectedPlatform( TOSHISKU_WINDOWS );
		}

		m_strWindowName = strFileName.GetString();

		// Complete the window title with the game name
		switch ( g_oTheApp.GetSelectedGame() )
		{
			case TOSHIGAME_BARNYARD:
				m_strWindowName += " - Barnyard";
				break;
			case TOSHIGAME_DEBLOB:
				m_strWindowName += " - de Blob";
				break;
		}

		// Complete the window title with the platform name
		switch ( g_oTheApp.GetSelectedPlatform() )
		{
			case TOSHISKU_WINDOWS:
				m_strWindowName += " (Windows)";
				break;
			case TOSHISKU_REV:
				m_strWindowName += " (GameCube / Wii)";
				break;
		}

		// Add ID to the window title
		m_strWindowName = TString8::VarArgs( "%s##%u", m_strWindowName.GetString(), GetImGuiID() );

		// Check out each of the symbols
		PTRBSections* pSections = m_pFile->GetSections();
		PTRBSymbols*  pSymbols  = m_pFile->GetSymbols();

		for ( TUINT i = 0; i < pSymbols->GetCount(); i++ )
		{
			TRBSymbol* pSymbol = TRBSymbolManager::GetSymbol( pSymbols->GetName( i ) );

			if ( !pSymbol )
				continue;
			
			// Create resource view from the symbol
			TRBResourceView* pResourceView = pSymbol->CreateResourceView();

			if ( !pResourceView )
				continue;

			// Initialise resource view from the data stored within TRB
			if ( !pResourceView->Create( m_pFile, pSymbols->Get<void*>( *pSections, 0 ).get(), pSymbols->GetName( i ) ) )
			{
				pResourceView->Destroy();
				continue;
			}

			m_vecResourceViews.PushBack( pResourceView );
		}

		return TTRUE;
	}
	
	return TFALSE;
}

TBOOL TRBFileWindow::SaveFile( Toshi::T2StringView strFilePath, TBOOL bCompress, Endianess eEndianess )
{
	PTRB trb{ eEndianess };

	trb.GetSections()->CreateStream();

	T2_FOREACH( m_vecResourceViews, it )
	{
		TRBResourceView* pResourceView = *it;

		// Skip resources that cannot be saved
		if ( !pResourceView->CanSave() )
			continue;

		pResourceView->OnSave( &trb );
	}

	trb.WriteToFile( strFilePath, bCompress );
	return TTRUE;
}

void TRBFileWindow::Render( TFLOAT fDeltaTime )
{
	if ( !m_bVisible )
		return;

	ImGuiComponent::PreRender();
	ImGuiID uiDockSpaceID = ImGui::GetID( m_strWindowName.GetString() );

	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.0f, 0.0f ) );
	ImGui::SetNextWindowSize( ImVec2( 640, 480 ), ImGuiCond_Appearing );
	ImGui::Begin( m_strWindowName, &m_bVisible, ImGuiWindowFlags_NoSavedSettings );
	ImGui::PopStyleVar();

	ImGui::DockSpace( uiDockSpaceID, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode );

	if ( m_pFile )
	{
		// Render general tab containing basic info about TRB file
		ImGui::SetNextWindowDockID( uiDockSpaceID, ImGuiCond_Always );
		ImGui::Begin( m_strTRBInfoTabName.Get() );
		{
			PTRBSymbols* pSybmols     = m_pFile->GetSymbols();
			TUINT        uiNumSymbols = pSybmols->GetCount();

			if ( ImGui::Button( "Save File" ) )
			{
				SaveFile( "test.trb", m_bUseCompression );
				//m_pFile->WriteToFile( m_strFilePath.GetString(), m_bUseCompression );
			}
			ImGui::SameLine();
			ImGui::Checkbox( "Use Compression", &m_bUseCompression );

			ImGui::Separator();

			ImGui::Checkbox( "Show Symbols", &m_bShowSymbols );
			if ( m_bShowSymbols )
			{
				ImGui::Text( "Symbols:" );

				for ( TUINT i = 0; i < uiNumSymbols; i++ )
				{
					ImGui::Button( pSybmols->GetName( i ) );
				}
			}
		}
		ImGui::End();

		// Render resource views
		T2_FOREACH( m_vecResourceViews, it )
		{
			TRBResourceView* pResourceView = *it;

			pResourceView->PreRender();

			ImGui::SetNextWindowDockID( uiDockSpaceID, ImGuiCond_Always );
			ImGui::Begin( pResourceView->GetNameId() );
			pResourceView->OnRender( fDeltaTime );
			ImGui::End();

			pResourceView->PostRender();
		}
	}

	ImGui::End();
	ImGuiComponent::PostRender();
}

TBOOL TRBFileWindow::Update()
{
	return m_bVisible;
}

void TRBFileWindow::UnloadFile()
{
	if ( m_pFile )
	{
		delete m_pFile;
		m_pFile = TNULL;
	}

	T2_FOREACH( m_vecResourceViews, it )
	{
		TRBResourceView* pResourceView = *it;

		if ( pResourceView )
			pResourceView->Destroy();
	}

	m_vecResourceViews.Clear();
}

