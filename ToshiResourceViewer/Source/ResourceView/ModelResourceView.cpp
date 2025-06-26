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

static T2FrameBuffer s_ViewportFrameBuffer;
static TBOOL         s_bFrameBufferInitialised = TFALSE;

ModelResourceView::ModelResourceView()
    : m_vecCameraPosition( TVector3::VEC_ZERO )
{
	if ( !s_bFrameBufferInitialised )
	{
		s_ViewportFrameBuffer.Create();
		s_ViewportFrameBuffer.CreateDepthTexture( 1920, 1080 );
		s_ViewportFrameBuffer.CreateAttachment( 0, 1920, 1080, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE );

		s_bFrameBufferInitialised = TTRUE;
	}
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

		m_pModel = ResourceLoader::Model_Load_Barnyard_Windows( m_pTRB, m_pTRB->GetEndianess() );
	}
	
	return TRBResourceView::OnCreate() && m_pModel.IsValid();
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
	ImGui::DragFloat3( "Camera Position", (float*)&m_vecCameraPosition, 0.1f, -25.0f, 25.0f );

	ImVec2 availRegion = ImGui::GetContentRegionAvail();

	T2Viewport oViewport( 0.0f, 0.0f, availRegion.x, availRegion.y, 0.1f, 1.0f );

	// Compute perspective projection
	T2RenderContext::Projection oProjection;
	oProjection.SetFromFOV( availRegion.x, availRegion.y, TMath::DegToRad( 45.0f ), 1.0f, 1000.0f );

	T2RenderContext::ComputePerspectiveProjection(
	    T2Render::GetRenderContext().GetProjectionMatrix(),
	    oViewport,
	    oProjection
	);

	// Render Scene
	{
		s_ViewportFrameBuffer.Bind();
		oViewport.Begin();
	
		TMatrix44 matModelView;
		matModelView.Identity();
		matModelView.SetTranslation( m_vecCameraPosition );

		T2Render::GetRenderContext().SetModelViewMatrix( matModelView );

		if ( m_pModel )
			m_pModel->Render();
	
		g_pRenderGL->FlushOrderTables();

		oViewport.End();
		s_ViewportFrameBuffer.Unbind();
	}

	// Render to the viewport
	ImGui::Image( s_ViewportFrameBuffer.GetAttachment( 0 ), ImVec2( availRegion.x, availRegion.y ), ImVec2( 0.0f, 0.0f ), ImVec2( availRegion.x / 1920.0f, availRegion.y / 1080.0f ) );
}
