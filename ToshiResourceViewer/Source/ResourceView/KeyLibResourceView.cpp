#include "pch.h"
#include "KeyLibResourceView.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

KeyLibResourceView::KeyLibResourceView()
{
	m_strName = "KeyFrame Library";
}

KeyLibResourceView::~KeyLibResourceView()
{
	
}

TBOOL KeyLibResourceView::OnCreate( Toshi::T2StringView pchFilePath )
{
	TRBResourceView::OnCreate( pchFilePath );

	TKeyframeLibrary::TRBHeader* pTRBHeader = TSTATICCAST( TKeyframeLibrary::TRBHeader, m_pData );
	
	if ( m_strSymbolName == "keylib" )
	{
		m_pKeyLib = Resource::StreamedKeyLib_Create( TPS8D( pTRBHeader->m_szName ), pTRBHeader );
		return TTRUE;
	}

	return TFALSE;
}

TBOOL KeyLibResourceView::CanSave()
{
	return TFALSE;
}

TBOOL KeyLibResourceView::OnSave( PTRB* pOutTRB )
{
	return TFALSE;
}

void KeyLibResourceView::OnDestroy()
{
}

void KeyLibResourceView::OnRender( TFLOAT flDeltaTime )
{
	ImGui::Text( "Name: %s", m_pKeyLib->GetName().GetString() );
	ImGui::Text( "Number of translation keys: %d", m_pKeyLib->GetLibrary()->GetNumTranslations() );
	ImGui::Text( "Number of quaternion keys: %d", m_pKeyLib->GetLibrary()->GetNumQuaternions() );
	ImGui::Text( "Number of scale keys: %d", m_pKeyLib->GetLibrary()->GetNumScales() );
}
