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

TBOOL TRBFileWindow::LoadFile( Toshi::T2ConstString8 strFileName )
{
	UnloadFile();
	m_pFile = new PTRB;

	m_strFilePath   = strFileName;
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
	ImGui::Begin( m_strFilePath, &m_bHidden );

	if ( m_pFile )
	{
		if ( ImGui::BeginTabBar( "TRBFileTabBar" ) )
		{
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

				if ( ImGui::BeginTabItem( pResourceView->GetName() ) )
				{
					pResourceView->OnRender( 0.0f );

					ImGui::EndTabItem();
				}
			}

			ImGui::EndTabBar();
		}
	}

	ImGui::End();
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

