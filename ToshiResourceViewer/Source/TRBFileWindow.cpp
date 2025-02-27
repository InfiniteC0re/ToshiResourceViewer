#include "pch.h"
#include "TRBFileWindow.h"
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
	
	return bReadFile;
}

void TRBFileWindow::Render()
{
	ImGui::Begin( m_strFilePath, &m_bHidden );

	if ( m_pFile )
	{
		PTRBSymbols* pSybmols = m_pFile->GetSymbols();
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
}

