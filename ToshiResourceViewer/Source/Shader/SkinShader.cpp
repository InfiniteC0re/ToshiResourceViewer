#include "pch.h"
#include "SkinShader.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TDEFINE_CLASS( SkinShader );
TDEFINE_CLASS( SkinMesh );

SkinShader::SkinShader()
{
	
}

SkinShader::~SkinShader()
{
}

TBOOL SkinShader::Validate()
{
	if ( IsValidated() )
		return TTRUE;

	// Compile shaders
	m_hVertexShader   = T2Render::CompileShaderFromFile( GL_VERTEX_SHADER, "Resources/Shaders/SkinShader.vs" );
	m_hFragmentShader = T2Render::CompileShaderFromFile( GL_FRAGMENT_SHADER, "Resources/Shaders/SkinShader.fs" );

	// Create shader programs
	m_oShaderProgram = T2Render::CreateShaderProgram( m_hVertexShader, m_hFragmentShader );

	return BaseClass::Validate();
}

void SkinShader::Invalidate()
{
	if ( !IsValidated() )
		return;

	// TODO: destroy shader and program

	return BaseClass::Invalidate();
}

void SkinShader::StartFlush()
{
	Validate();

	T2Render::GetRenderContext().EnableBlend( TTRUE );
	T2Render::GetRenderContext().EnableDepthTest( TTRUE );

	static TPString8 s_Projection = TPS8D( "u_Projection" );

	T2Render::SetShaderProgram( m_oShaderProgram );
	m_oShaderProgram.SetUniform( s_Projection, T2Render::GetRenderContext().GetProjectionMatrix() );
}

void SkinShader::EndFlush()
{
	Validate();
}

TBOOL SkinShader::Create()
{
	m_aOrderTable.Create( this, 0 );

	return BaseClass::Create();
}

void SkinShader::Render( TRenderPacket* a_pRenderPacket )
{
	Validate();

	static TPString8 s_ModelView = TPS8D( "u_ModelView" );

	T2Render::SetShaderProgram( m_oShaderProgram );
	m_oShaderProgram.SetUniform( s_ModelView, a_pRenderPacket->GetModelViewMatrix() );

	TSkeletonInstance* pSkeletonInstance = a_pRenderPacket->GetSkeletonInstance();

	if ( SkinMesh* pSkinMesh = TSTATICCAST( SkinMesh, a_pRenderPacket->GetMesh() ) )
	{
		static TPString8 s_NumBones       = TPS8D( "u_NumBones" );
		static TPString8 s_BoneTransforms = TPS8D( "u_BoneTransforms" );

		TMatrix44 s_aBoneTransforms[ 28 ];

		T2_FOREACH( pSkinMesh->vecSubMeshes, subMesh )
		{
			if ( pSkeletonInstance )
			{
				// Fill matrices from the skeleton instance
				for ( TINT k = 0; k < subMesh->uiNumBones && k < TARRAYSIZE( s_aBoneTransforms ); k++ )
					s_aBoneTransforms[ k ] = pSkeletonInstance->GetBone( subMesh->aBones[ k ] ).m_Transform;
			}
			else
			{
				// No skeleton instance, reset matrices
				for ( TINT k = 0; k < subMesh->uiNumBones && k < TARRAYSIZE( s_aBoneTransforms ); k++ )
					s_aBoneTransforms[ k ].Identity();
			}

			m_oShaderProgram.SetUniform( s_BoneTransforms, s_aBoneTransforms, subMesh->uiNumBones );
			m_oShaderProgram.SetUniform( s_NumBones, subMesh->uiNumBones );

			subMesh->oVertexArray.Bind();
			
			glDrawElements( GL_TRIANGLE_STRIP, subMesh->uiNumIndices, GL_UNSIGNED_SHORT, NULL );
		}
	}
}

SkinMesh* SkinShader::CreateMesh()
{
	Validate();

	SkinMesh* pMesh = new SkinMesh();
	pMesh->SetOwnerShader( this );

	return pMesh;
}

SkinMaterial* SkinShader::CreateMaterial()
{
	Validate();

	SkinMaterial* pMaterial = new SkinMaterial();
	pMaterial->SetShader( this );
	pMaterial->SetOrderTable( &m_aOrderTable );

	return pMaterial;
}

TBOOL SkinMesh::Render()
{
	T2RenderContext& rContext = g_pRenderGL->GetRenderContext();

	TRenderPacket* pRenderPacket = GetMaterial()->AddRenderPacket( this );

	TMatrix44 matModelView;
	matModelView.Multiply( rContext.GetViewMatrix(), rContext.GetModelMatrix() );

	pRenderPacket->SetModelViewMatrix( matModelView );
	pRenderPacket->SetSkeletonInstance( rContext.GetSkeletonInstance() );

	//pRenderPacket->SetAmbientColour( pCurrentContext->GetAmbientColour().AsVector3() );
	//pRenderPacket->SetLightColour( pRenderInterface->GetLightColour().AsBasisVector3( 0 ) );
	//pRenderPacket->SetLightDirection( pRenderInterface->GetLightDirection().AsBasisVector3( 0 ) );
	//pRenderPacket->SetAlpha( pCurrentContext->GetAlphaBlend() );
	//pRenderPacket->SetShadeCoeff( TUINT( pCurrentContext->GetShadeCoeff() * 255.0f ) );

	return TTRUE;
}

void SkinMesh::OnDestroy()
{
	BaseClass::OnDestroy();

	delete m_pMaterial;
	m_pMaterial = TNULL;
}

void SkinMaterial::PreRender()
{
	g_pRenderGL->SetTexture2D( 0, m_pTexture->GetHandle() );
}
