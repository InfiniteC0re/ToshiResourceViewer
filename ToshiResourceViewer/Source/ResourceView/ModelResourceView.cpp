#include "pch.h"
#include "ModelResourceView.h"

#include <Render/TTMDWin.h>
#include <Render/T2Render.h>

#include <Platform/GL/T2FrameBuffer_GL.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

ModelResourceView::ModelResourceView()
    : m_vecCameraPosition( TVector3::VEC_ZERO )
    , m_fCameraFOV( 90.0f )
{
	m_ViewportFrameBuffer.Create();
	m_ViewportFrameBuffer.CreateDepthTexture( 1920, 1080 );
	m_ViewportFrameBuffer.CreateAttachment( 0, 1920, 1080, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE );
}

ModelResourceView::~ModelResourceView()
{
}

TBOOL ModelResourceView::OnCreate()
{
	if ( m_strSymbolName == "FileHeader" )
	{
		TTMDBase::FileHeader* pFileHeader = TSTATICCAST( TTMDBase::FileHeader, m_pData );

		if ( pFileHeader->m_uiMagic != TFourCC( "TMDL" ) &&
		     pFileHeader->m_uiMagic != TFourCC( "LDMT" ) )
			return TFALSE;

		ResourceLoader::Model_CreateInstance( ResourceLoader::Model_Load_Barnyard_Windows( m_pTRB, m_pTRB->GetEndianess() ), m_ModelInstance );
		m_ModelInstance.oTransform.SetMatrix( TMatrix44::IDENTITY );
		m_ModelInstance.oTransform.SetEuler( TVector3( TMath::PI * 0.5f, 0.0f, 0.0f ) );
		m_ModelInstance.oTransform.SetTranslate( TVector3::VEC_ZERO );
	}
	
	return TRBResourceView::OnCreate() && m_ModelInstance.pModel.IsValid();
}

TBOOL ModelResourceView::CanSave()
{
	return TFALSE;
}

TBOOL ModelResourceView::OnSave( PTRB* pOutTRB )
{
	return TFALSE;
}

void ModelResourceView::OnDestroy()
{
}

void ModelResourceView::OnRender( TFLOAT flDeltaTime )
{
	TVector3& oCamTranslation = m_oCamera->GetTranslation();

	ImGui::DragFloat3( "Camera Position", (float*)&m_vecCameraPosition, 0.1f, -25.0f, 25.0f );
	ImGui::DragFloat( "Camera FOV", &m_fCameraFOV, 0.1f, 10.0f, 90.0f );

	m_oCamera.SetFOV( TMath::DegToRad( m_fCameraFOV ) );
	m_oCamera->SetTranslate( m_vecCameraPosition );

	// Update render context
	ImVec2 oRegion = ImGui::GetContentRegionAvail();
	
	g_pRenderGL->SetRenderContext( m_oRenderContext );
	m_oRenderContext.ForceRefreshFeatures();

	m_oRenderContext.GetViewport().SetWidth( oRegion.x );
	m_oRenderContext.GetViewport().SetHeight( oRegion.y );

	m_oRenderContext.SetCamera( m_oCamera );
	m_oRenderContext.UpdateCamera();

	// Render scene
	{
		m_ViewportFrameBuffer.Bind();
		m_oRenderContext.GetViewport().Begin();

		m_ModelInstance.oTransform.GetLocalMatrixImp( m_oRenderContext.GetModelMatrix() );

		if ( m_ModelInstance.pModel )
			m_ModelInstance.pModel->Render();
	
		g_pRenderGL->FlushOrderTables();

		m_oRenderContext.GetViewport().End();
		m_ViewportFrameBuffer.Unbind();
	}

	g_pRenderGL->SetDefaultRenderContext();

	// Render to the viewport
	ImVec2 oImagePos = ImGui::GetCursorPos();
	ImGui::Image( m_ViewportFrameBuffer.GetAttachment( 0 ), ImVec2( oRegion.x, oRegion.y ), ImVec2( 0.0f, oRegion.y / 1080.0f ), ImVec2( oRegion.x / 1920.0f, 0.0f ) );

	// Draw info

	TINT iNumMessages = 0;

	auto fnPrintErrorMessage = [ & ]( const TCHAR* szMessage ) {
		iNumMessages += 1;

		ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.0f, 0.0f, 1.0f ) );
		ImGui::SetCursorPos( ImVec2( 20.0f, oImagePos.y + ( oRegion.y - ImGui::GetFontSize() * iNumMessages - 4.0f ) ) );
		ImGui::Text( szMessage );
		ImGui::PopStyleColor();
	};
	
	if ( m_ModelInstance.pModel->pKeyLib->IsDummy() )
	{
		T2String8::Format( T2String8::ms_aScratchMem, "Missing keyframe library '%s'", m_ModelInstance.pModel->pKeyLib->GetName().GetString() );
		fnPrintErrorMessage( T2String8::ms_aScratchMem );
	}

	T2_FOREACH( m_ModelInstance.pModel->vecUsedTextures, it )
	{
		if ( it->Get()->IsDummy() )
		{
			T2String8::Format( T2String8::ms_aScratchMem, "Missing texture '%s'", it->Get()->GetTexture().strName.GetString() );
			fnPrintErrorMessage( T2String8::ms_aScratchMem );
		}
	}
}
