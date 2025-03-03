#include "pch.h"
#include "TRBFileWindow.h"
#include "TRBSymbolManager.h"
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

TBOOL TRBFileWindow::LoadFile( Toshi::T2ConstString8 strFilePath )
{
	UnloadFile();
	m_pFile = new PTRB;

	m_strFilePath = strFilePath;
	FixPathSlashes( m_strFilePath );

	TINT     iLastSlashIndex = m_strFilePath.FindReverse( '\\' );
	TString8 strFileName     = ( iLastSlashIndex != -1 ) ? TString8( m_strFilePath.GetString( iLastSlashIndex + 1 ) ) : m_strFilePath;

	m_strWindowName = TString8::VarArgs( "%s##%u", strFileName.GetString(), GetImGuiID() );

	TBOOL bReadFile = m_pFile->ReadFromFile( m_strFilePath.GetString() );

	if ( bReadFile )
	{
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
			if ( !pResourceView->Create( m_pFile, pSymbols->Get<void*>( *pSections, 0 ).get() ) )
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

void TRBFileWindow::Render()
{
	if ( !m_bVisible )
		return;

	ImGui::Begin( m_strWindowName, &m_bVisible, ImGuiWindowFlags_NoSavedSettings );

	if ( m_pFile )
	{
		if ( ImGui::BeginTabBar( "TRBFileTabBar" ) )
		{
			ImGuiComponent::PreRender();

			// Render general tab containing basic info about TRB file
			if ( ImGui::BeginTabItem( "TRB File" ) )
			{
				PTRBSymbols* pSybmols     = m_pFile->GetSymbols();
				TUINT        uiNumSymbols = pSybmols->GetCount();

				ImGui::Checkbox( "Use Compression", &m_bUseCompression );
				if ( ImGui::Button( "Save File" ) )
					m_pFile->WriteToFile( m_strFilePath.GetString(), m_bUseCompression );

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

				ImGui::EndTabItem();
			}

			// Render resource views
			T2_FOREACH( m_vecResourceViews, it )
			{
				TRBResourceView* pResourceView = *it;

				pResourceView->PreRender();
				if ( ImGui::BeginTabItem( pResourceView->GetName() ) )
				{
					pResourceView->OnRender( 0.0f );
					ImGui::EndTabItem();
				}
				pResourceView->PostRender();
			}

			ImGuiComponent::PostRender();
			ImGui::EndTabBar();
		}
	}

	ImGui::End();
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

