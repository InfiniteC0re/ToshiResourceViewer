#include "pch.h"
#include "WorldShader.h"
#include "RendererSettings.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TDEFINE_CLASS( WorldShader );
TDEFINE_CLASS( WorldMesh );
TDEFINE_CLASS( WorldMaterial );

WorldShader::WorldShader()
    : m_ShadowColour( 0.3f, 0.3f, 0.3f, 1.0f ), m_AmbientColour( 1.0f, 1.0f, 1.0f, 1.0f )
{
	m_iAlphaRef         = 128;
	m_bRenderEnvMap     = TFALSE;
	m_bAlphaBlendMaterial = TFALSE;
}

WorldShader::~WorldShader()
{
}

TBOOL WorldShader::Validate()
{
	if ( IsValidated() )
		return TTRUE;

	m_hVertexShader   = T2Render::CompileShaderFromFile( GL_VERTEX_SHADER, "Resources/Shaders/WorldShader.vs" );
	m_hFragmentShader = T2Render::CompileShaderFromFile( GL_FRAGMENT_SHADER, "Resources/Shaders/WorldShader.fs" );
	m_oShaderProgram  = T2Render::CreateShaderProgram( m_hVertexShader, m_hFragmentShader );

	return BaseClass::Validate();
}

void WorldShader::Invalidate()
{
	if ( !IsValidated() )
		return;

	// TODO: destroy shader and program

	return BaseClass::Invalidate();
}

void WorldShader::StartFlush()
{
	Validate();

	T2Render::GetRenderContext().EnableBlend( TTRUE );
	T2Render::GetRenderContext().EnableDepthTest( TTRUE );

	static TPString8 s_Projection    = TPS8D( "u_Projection" );
	static TPString8 s_AmbientColour = TPS8D( "u_AmbientColour" );
	static TPString8 s_ShadowColour  = TPS8D( "u_ShadowColour" );

	T2Render::SetShaderProgram( m_oShaderProgram );
	m_oShaderProgram.SetUniform( s_Projection, T2Render::GetRenderContext().GetProjectionMatrix() );
	m_oShaderProgram.SetUniform( s_AmbientColour, m_AmbientColour );
	m_oShaderProgram.SetUniform( s_ShadowColour, m_ShadowColour );

	s_RenderStateFlags = 0x1B;
}

void WorldShader::EndFlush()
{
	Validate();
}

TBOOL WorldShader::Create()
{
	// Priorities match the DX8 implementation
	m_aOrderTables[ 0 ].Create( this, -3000 );
	m_aOrderTables[ 1 ].Create( this, 100 );
	m_aOrderTables[ 2 ].Create( this, 101 );
	m_aOrderTables[ 3 ].Create( this, 601 );
	m_aOrderTables[ 4 ].Create( this, -400 );
	m_aOrderTables[ 5 ].Create( this, 500 );
	m_aOrderTables[ 6 ].Create( this, -6005 );
	m_aOrderTables[ 7 ].Create( this, -7000 );

	return BaseClass::Create();
}

void WorldShader::Render( TRenderPacket* a_pRenderPacket )
{
	Validate();

	static TPString8 s_ModelView = TPS8D( "u_ModelView" );
	static TPString8 s_AlphaRef  = TPS8D( "u_AlphaRef" );
	static TPString8 s_IsWater   = TPS8D( "u_IsWater" );

	T2Render::SetShaderProgram( m_oShaderProgram );
	m_oShaderProgram.SetUniform( s_ModelView, a_pRenderPacket->GetModelViewMatrix() );

	// Scale alpha ref by packet alpha, matching DX8 behaviour
	TFLOAT fAlpha = a_pRenderPacket->GetAlpha();
	if ( fAlpha >= 1.0f || isnan( fAlpha ) )
	{
		if ( s_RenderStateFlags & 24 )
		{
			m_oShaderProgram.SetUniform( s_AlphaRef, TFLOAT( m_iAlphaRef ) );
			s_RenderStateFlags &= ~24;
		}
	}
	else
	{
		m_oShaderProgram.SetUniform( s_AlphaRef, TFLOAT( TUINT8( m_iAlphaRef * fAlpha ) ) );
		s_RenderStateFlags |= 24;
	}

	if ( WorldMesh* pWorldMesh = TSTATICCAST( WorldMesh, a_pRenderPacket->GetMesh() ) )
	{
		T2SharedPtr<WorldMaterial> pMaterial = pWorldMesh->GetMaterial();

		// Enable/disable alpha blend based on blend mode and packet alpha
		TBOOL bNeedsBlend = ( pMaterial->GetBlendMode() != 0 ) || ( fAlpha < 1.0f );
		if ( bNeedsBlend && ISZERO( s_RenderStateFlags & 0x1b ) )
		{
			T2Render::GetRenderContext().EnableBlend( TTRUE );
			s_RenderStateFlags |= 0x1b;
		}
		else if ( !bNeedsBlend && !ISZERO( s_RenderStateFlags & 0x1b ) )
		{
			T2Render::GetRenderContext().EnableBlend( TFALSE );
			s_RenderStateFlags &= ~0x1b;
		}

		m_oShaderProgram.SetUniform( s_IsWater, pWorldMesh->IsWater() ? 1.0f : 0.0f );

		T2_FOREACH( pWorldMesh->vecSubMeshes, subMesh )
		{
			subMesh->oVertexArray.Bind();
			glDrawElements( GL_TRIANGLE_STRIP, subMesh->uiNumIndices, GL_UNSIGNED_SHORT, NULL );
		}
	}
}

WorldMesh* WorldShader::CreateMesh()
{
	Validate();

	WorldMesh* pMesh = new WorldMesh();
	pMesh->SetOwnerShader( this );

	return pMesh;
}

Toshi::T2SharedPtr<WorldMaterial> WorldShader::CreateMaterial()
{
	Validate();

	auto pMaterial = T2SharedPtr<WorldMaterial>::New();
	pMaterial->SetShader( this );
	pMaterial->SetBlendMode( 0 );

	if ( IsAlphaBlendMaterial() )
	{
		auto pAlphaBlendMaterial = T2SharedPtr<WorldMaterial>::New();
		pAlphaBlendMaterial->SetShader( this );
		pAlphaBlendMaterial->SetBlendMode( 1 );
		pAlphaBlendMaterial->SetIsAlphaBlend( TTRUE );

		pMaterial->SetAlphaBlendMaterial( pAlphaBlendMaterial.Get() );
	}

	return pMaterial;
}

void WorldShader::SetColours( const TVector4& a_rShadowColour, const TVector4& a_rAmbientColour )
{
	m_ShadowColour  = a_rShadowColour;
	m_AmbientColour = a_rAmbientColour;

	TMath::Clip( m_ShadowColour.x, 0.0f, 1.0f );
	TMath::Clip( m_ShadowColour.y, 0.0f, 1.0f );
	TMath::Clip( m_ShadowColour.z, 0.0f, 1.0f );
	TMath::Clip( m_AmbientColour.x, 0.0f, 1.0f );
	TMath::Clip( m_AmbientColour.y, 0.0f, 1.0f );
	TMath::Clip( m_AmbientColour.z, 0.0f, 1.0f );
}

//-----------------------------------------------------------------------------
// WorldMesh
//-----------------------------------------------------------------------------

TBOOL WorldMesh::Render()
{
	T2RenderContext& rContext = g_pRenderGL->GetRenderContext();

	TRenderPacket* pRenderPacket = GetMaterial()->AddRenderPacket( this );

	TMatrix44 matModelView;
	matModelView.Multiply( rContext.GetViewMatrix(), rContext.GetModelMatrix() );

	pRenderPacket->SetModelViewMatrix( matModelView );
	pRenderPacket->SetAlpha( 1.0f );

	return TTRUE;
}

void WorldMesh::OnDestroy()
{
	BaseClass::OnDestroy();
}

TBOOL WorldMesh::SerializeGLTFMesh( tinygltf::Model& a_rOutModel, TSkeletonInstance* a_pSkeletonInstance )
{
	tinygltf::Buffer gltfBuffer;

	TINT iMeshStartIndex = TINT( a_rOutModel.meshes.size() );
	TINT iBufferIndex    = TINT( a_rOutModel.buffers.size() );

	T2SharedPtr<WorldMaterial> pMaterial      = GetMaterial();
	TINT                       iMaterialIndex = pMaterial ? a_rOutModel.FindMaterialIndex( GetMaterialName() ) : -1;

	if ( pMaterial && iMaterialIndex == -1 )
	{
		tinygltf::Material gltfMaterial;
		gltfMaterial.name = GetMaterialName();

		if ( pMaterial->AccessTexture() )
		{
			TPString8 strTextureUri = pMaterial->AccessTexture()->GetTexture().strName;
			TINT      iTextureIndex = a_rOutModel.FindTextureIndex( strTextureUri );

			if ( iTextureIndex == -1 )
			{
				tinygltf::Image gltfImage;
				gltfImage.uri = strTextureUri;
				a_rOutModel.images.push_back( std::move( gltfImage ) );

				tinygltf::Sampler gltfSampler;
				gltfSampler.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
				gltfSampler.magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
				a_rOutModel.samplers.push_back( std::move( gltfSampler ) );

				tinygltf::Texture gltfTexture;
				gltfTexture.source  = TINT( a_rOutModel.images.size() - 1 );
				gltfTexture.sampler = TINT( a_rOutModel.samplers.size() - 1 );
				a_rOutModel.textures.push_back( std::move( gltfTexture ) );
				iTextureIndex = TINT( a_rOutModel.textures.size() - 1 );
			}

			gltfMaterial.pbrMetallicRoughness.baseColorTexture.index = iTextureIndex;
		}

		a_rOutModel.materials.push_back( std::move( gltfMaterial ) );
		iMaterialIndex = TINT( a_rOutModel.materials.size() - 1 );
	}

	// Collect all vertex data
	GLint iVertexBufferSize = 0;
	oVertexBuffer.GetParameter( GL_BUFFER_SIZE, iVertexBufferSize );
	const TUINT uiNumTotalVertices = iVertexBufferSize / sizeof( WorldVertex );

	Toshi::T2DynamicVector<TBYTE> vecVertices;
	vecVertices.SetSize( iVertexBufferSize );
	oVertexBuffer.GetSubData( vecVertices.Begin(), 0, iVertexBufferSize );
	gltfBuffer.data.insert( gltfBuffer.data.end(), vecVertices.Begin(), vecVertices.End() );

	WorldVertex* pVertices = (WorldVertex*)&*vecVertices.Begin();

	tinygltf::BufferView gltfBufferViewVertex;
	gltfBufferViewVertex.buffer     = iBufferIndex;
	gltfBufferViewVertex.byteOffset = 0;
	gltfBufferViewVertex.byteLength = uiNumTotalVertices * sizeof( WorldVertex );
	gltfBufferViewVertex.byteStride = sizeof( WorldVertex );
	gltfBufferViewVertex.target     = TINYGLTF_TARGET_ARRAY_BUFFER;

	a_rOutModel.bufferViews.push_back( std::move( gltfBufferViewVertex ) );
	const TINT iVertexBufferView = TINT( a_rOutModel.bufferViews.size() - 1 );

	std::vector<double> min_vals = {
		std::numeric_limits<double>::max(),
		std::numeric_limits<double>::max(),
		std::numeric_limits<double>::max()
	};
	std::vector<double> max_vals = {
		std::numeric_limits<double>::lowest(),
		std::numeric_limits<double>::lowest(),
		std::numeric_limits<double>::lowest()
	};
	for ( TUINT i = 0; i < uiNumTotalVertices; i++ )
	{
		min_vals[ 0 ] = TMath::Min( min_vals[ 0 ], TDOUBLE( pVertices[ i ].Position.x ) );
		min_vals[ 1 ] = TMath::Min( min_vals[ 1 ], TDOUBLE( pVertices[ i ].Position.y ) );
		min_vals[ 2 ] = TMath::Min( min_vals[ 2 ], TDOUBLE( pVertices[ i ].Position.z ) );
		max_vals[ 0 ] = TMath::Max( max_vals[ 0 ], TDOUBLE( pVertices[ i ].Position.x ) );
		max_vals[ 1 ] = TMath::Max( max_vals[ 1 ], TDOUBLE( pVertices[ i ].Position.y ) );
		max_vals[ 2 ] = TMath::Max( max_vals[ 2 ], TDOUBLE( pVertices[ i ].Position.z ) );
	}

	tinygltf::Accessor gltfAccPosition;
	gltfAccPosition.bufferView    = iVertexBufferView;
	gltfAccPosition.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
	gltfAccPosition.type          = TINYGLTF_TYPE_VEC3;
	gltfAccPosition.count         = uiNumTotalVertices;
	gltfAccPosition.byteOffset    = offsetof( WorldVertex, Position );
	gltfAccPosition.minValues     = std::move( min_vals );
	gltfAccPosition.maxValues     = std::move( max_vals );
	a_rOutModel.accessors.push_back( std::move( gltfAccPosition ) );
	const TINT iAccPositionIndex = TINT( a_rOutModel.accessors.size() - 1 );

	tinygltf::Accessor gltfAccNormal;
	gltfAccNormal.bufferView    = iVertexBufferView;
	gltfAccNormal.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
	gltfAccNormal.type          = TINYGLTF_TYPE_VEC3;
	gltfAccNormal.count         = uiNumTotalVertices;
	gltfAccNormal.byteOffset    = offsetof( WorldVertex, Normal );
	a_rOutModel.accessors.push_back( std::move( gltfAccNormal ) );
	const TINT iAccNormalIndex = TINT( a_rOutModel.accessors.size() - 1 );

	tinygltf::Accessor gltfAccColor;
	gltfAccColor.bufferView    = iVertexBufferView;
	gltfAccColor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
	gltfAccColor.type          = TINYGLTF_TYPE_VEC3;
	gltfAccColor.count         = uiNumTotalVertices;
	gltfAccColor.byteOffset    = offsetof( WorldVertex, Color );
	a_rOutModel.accessors.push_back( std::move( gltfAccColor ) );
	const TINT iAccColorIndex = TINT( a_rOutModel.accessors.size() - 1 );

	tinygltf::Accessor gltfAccUV;
	gltfAccUV.bufferView    = iVertexBufferView;
	gltfAccUV.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
	gltfAccUV.type          = TINYGLTF_TYPE_VEC2;
	gltfAccUV.count         = uiNumTotalVertices;
	gltfAccUV.byteOffset    = offsetof( WorldVertex, UV );
	a_rOutModel.accessors.push_back( std::move( gltfAccUV ) );
	const TINT iAccUVIndex = TINT( a_rOutModel.accessors.size() - 1 );

	T2_FOREACH( vecSubMeshes, it )
	{
		TSIZE uiBufferBaseOffset = gltfBuffer.data.size();

		GLint iIndexBufferSize = 0;
		it->oVertexArray.GetIndexBuffer().GetParameter( GL_BUFFER_SIZE, iIndexBufferSize );

		Toshi::T2DynamicVector<TBYTE> vecIndices;
		vecIndices.SetSize( iIndexBufferSize );
		it->oVertexArray.GetIndexBuffer().GetSubData( vecIndices.Begin(), 0, iIndexBufferSize );
		gltfBuffer.data.insert( gltfBuffer.data.end(), vecIndices.Begin(), vecIndices.End() );

		tinygltf::BufferView gltfBufferViewIndex;
		gltfBufferViewIndex.buffer     = iBufferIndex;
		gltfBufferViewIndex.byteOffset = uiBufferBaseOffset;
		gltfBufferViewIndex.byteLength = iIndexBufferSize;
		gltfBufferViewIndex.target     = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
		a_rOutModel.bufferViews.push_back( std::move( gltfBufferViewIndex ) );
		const TINT iIndexBufferView = TINT( a_rOutModel.bufferViews.size() - 1 );

		tinygltf::Accessor gltfAccIndex;
		gltfAccIndex.bufferView    = iIndexBufferView;
		gltfAccIndex.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
		gltfAccIndex.type          = TINYGLTF_TYPE_SCALAR;
		gltfAccIndex.count         = it->uiNumIndices;
		a_rOutModel.accessors.push_back( std::move( gltfAccIndex ) );
		const TINT iAccIndicesIndex = TINT( a_rOutModel.accessors.size() - 1 );

		tinygltf::Mesh      gltfMesh;
		tinygltf::Primitive gltfPrimitive;
		gltfPrimitive.attributes[ "POSITION" ]   = iAccPositionIndex;
		gltfPrimitive.attributes[ "NORMAL" ]     = iAccNormalIndex;
		gltfPrimitive.attributes[ "COLOR_0" ]    = iAccColorIndex;
		gltfPrimitive.attributes[ "TEXCOORD_0" ] = iAccUVIndex;
		gltfPrimitive.indices                    = iAccIndicesIndex;
		gltfPrimitive.mode                       = TINYGLTF_MODE_TRIANGLE_STRIP;
		gltfPrimitive.material                   = iMaterialIndex;

		gltfMesh.primitives.push_back( std::move( gltfPrimitive ) );
		a_rOutModel.meshes.push_back( std::move( gltfMesh ) );
	}

	a_rOutModel.buffers.push_back( std::move( gltfBuffer ) );

	for ( TSIZE i = iMeshStartIndex; i < a_rOutModel.meshes.size(); i++ )
	{
		tinygltf::Node gltfNode;
		gltfNode.mesh = i;
		gltfNode.name = ( vecSubMeshes.Size() == 1 )
		                    ? GetName()
		                    : TString8::VarArgs( "%s_WM%u", GetName(), i - iMeshStartIndex ).GetString();
		a_rOutModel.nodes.push_back( std::move( gltfNode ) );
	}

	return TTRUE;
}

TBOOL WorldMesh::SerializeTRBMesh( PTRB* a_pTRB, PTRBSections::MemoryStream::Ptr<Toshi::TTMDWin::TRBMeshLODHeader> a_pTRBMesh )
{
	// World meshes use a different on-disk format than skin meshes; not yet implemented
	TASSERT( !"WorldMesh::SerializeTRBMesh not implemented" );
	return TFALSE;
}

void WorldMesh::GetMaterialInfo( Toshi::TString8& a_rMatName, Toshi::TString8& a_rTexName )
{
	T2SharedPtr<WorldMaterial> pMaterial = GetMaterial();

	a_rMatName = GetMaterialName();
	a_rTexName = pMaterial && pMaterial->AccessTexture()
	                 ? pMaterial->AccessTexture()->GetTexture().strName
	                 : "";
}

//-----------------------------------------------------------------------------
// WorldMaterial
//-----------------------------------------------------------------------------

WorldMaterial::WorldMaterial()
{
	m_pAlphaBlendMaterial = TNULL;
	m_pAssignedOrderTable = TNULL;
	m_eBlendMode          = 0;
	m_bIsAlphaBlend       = TFALSE;
	m_fUVAnimX            = 0.0f;
	m_fUVAnimY            = 0.0f;
	m_fUVAnimSpeedX       = 0.0f;
	m_fUVAnimSpeedY       = 0.0f;
}

void WorldMaterial::PreRender()
{
	// Bind textures
	for ( TUINT i = 0; i < MAX_TEXTURES; i++ )
	{
		if ( m_aTextures[ i ] )
			g_pRenderGL->SetTexture2D( i, g_oRendererSettings.bDisableTextures ? T2TextureManager::GetSingleton()->GetWhiteTexture()->GetHandle() : m_aTextures[ i ]->GetHandle() );
	}

	// Update UV animation offsets and wrap to [-1, 1]
	m_fUVAnimX += m_fUVAnimSpeedX;
	m_fUVAnimY += m_fUVAnimSpeedY;

	if ( m_fUVAnimX > 1.0f ) m_fUVAnimX -= 1.0f;
	else if ( m_fUVAnimX < -1.0f ) m_fUVAnimX += 1.0f;

	if ( m_fUVAnimY > 1.0f ) m_fUVAnimY -= 1.0f;
	else if ( m_fUVAnimY < -1.0f ) m_fUVAnimY += 1.0f;

	// Set UV offset and alpha ref uniforms on the active world shader
	WorldShader* pShader = WorldShader::GetSingleton();
	if ( pShader )
	{
		static TPString8 s_UVOffset = TPS8D( "u_UVOffset" );
		static TPString8 s_AlphaRef = TPS8D( "u_AlphaRef" );

		TVector4 uvOffset( m_fUVAnimX, m_fUVAnimY, 0.0f, 0.0f );
		pShader->GetShaderProgram().SetUniform( s_UVOffset, uvOffset );

		TUINT uiAlphaRef = m_bIsAlphaBlend ? 1 : 128;
		pShader->GetShaderProgram().SetUniform( s_AlphaRef, TFLOAT( uiAlphaRef ) );
		pShader->SetAlphaRef( uiAlphaRef );

		// Set blend func based on blend mode
		switch ( m_eBlendMode )
		{
			case 3:
				// Additive blending: src_alpha, one
				glBlendFunc( GL_SRC_ALPHA, GL_ONE );
				glDepthMask( GL_FALSE );
				break;
			case 1:
			case 2:
			default:
				// Standard alpha blending: src_alpha, inv_src_alpha
				glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
				glDepthMask( m_eBlendMode == 0 ? GL_TRUE : GL_FALSE );
				break;
		}
	}

	WorldShader::s_RenderStateFlags = 0x1B;
}

void WorldMaterial::PostRender()
{
	glDepthMask( GL_TRUE );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	if ( WorldShader* pShader = WorldShader::GetSingleton() )
	{
		pShader->SetAlphaRef( 128 );
	}

	WorldShader::s_RenderStateFlags = 0x1B;
}

void WorldMaterial::SetOrderTable( Toshi::TOrderTable* a_pOrderTable )
{
	if ( a_pOrderTable != m_pAssignedOrderTable )
	{
		if ( m_pAssignedOrderTable )
		{
			Toshi::TOrderTable::DeregisterMaterial( GetRegMaterial() );
		}

		if ( a_pOrderTable )
			a_pOrderTable->RegisterMaterial( this );

		m_pAssignedOrderTable = a_pOrderTable;
	}
}

void WorldMaterial::SetBlendMode( BLENDMODE a_eBlendMode )
{
	WorldShader* pShader = TDYNAMICCAST( WorldShader, m_pShader );
	if ( !pShader ) { m_eBlendMode = a_eBlendMode; return; }

	switch ( a_eBlendMode )
	{
		case 0: SetOrderTable( pShader->GetOrderTable( 0 ) ); break;
		case 1: SetOrderTable( pShader->GetOrderTable( 2 ) ); break;
		case 2: SetOrderTable( pShader->GetOrderTable( 2 ) ); break;
		case 3: SetOrderTable( pShader->GetOrderTable( 3 ) ); break;
		case 5: SetOrderTable( pShader->GetOrderTable( 6 ) ); break;
		case 6: SetOrderTable( pShader->GetOrderTable( 4 ) ); break;
		case 7: SetOrderTable( pShader->GetOrderTable( 7 ) ); break;
		default: TASSERT( !"Invalid blend mode" ); break;
	}

	m_eBlendMode = a_eBlendMode;
}
