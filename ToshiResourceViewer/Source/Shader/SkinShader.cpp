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
				for ( TUINT k = 0; k < subMesh->uiNumBones && k < TARRAYSIZE( s_aBoneTransforms ); k++ )
					s_aBoneTransforms[ k ] = pSkeletonInstance->GetBone( subMesh->aBones[ k ] ).m_Transform;
			}
			else
			{
				// No skeleton instance, reset matrices
				for ( TUINT k = 0; k < subMesh->uiNumBones && k < TARRAYSIZE( s_aBoneTransforms ); k++ )
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

TBOOL SkinMesh::SerializeGLTFMesh( tinygltf::Model& a_rOutModel, Toshi::TSkeletonInstance* a_pSkeletonInstance )
{
	tinygltf::Buffer gltfBuffer;

	TINT iMeshStartIndex = TINT( a_rOutModel.meshes.size() );
	TINT iBufferIndex    = TINT( a_rOutModel.buffers.size() );

	// Find or create material
	SkinMaterial* pMaterial = TSTATICCAST( SkinMaterial, GetMaterial() );
	TINT iMaterialIndex = pMaterial ? a_rOutModel.FindMaterialIndex( GetMaterialName() ) : -1;

	if ( pMaterial && iMaterialIndex == -1 )
	{
		// Create new material
		tinygltf::Material gltfMaterial;
		gltfMaterial.name = GetMaterialName();

		// Find or create texture
		TPString8 strTextureUri = pMaterial->AccessTexture()->GetTexture().strName;
		TINT iTextureIndex = a_rOutModel.FindTextureIndex( strTextureUri );
		if ( iTextureIndex == -1 )
		{
			// Create new texture
			tinygltf::Image gltfImage;
			gltfImage.uri = strTextureUri;
			a_rOutModel.images.push_back( std::move( gltfImage ) );

			tinygltf::Sampler gltfSampler;
			gltfSampler.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
			gltfSampler.magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
			a_rOutModel.samplers.push_back( std::move( gltfSampler ) );

			tinygltf::Texture gltfTexture;
			gltfTexture.source = TINT( a_rOutModel.images.size() - 1 );
			gltfTexture.sampler = TINT( a_rOutModel.samplers.size() - 1 );

			a_rOutModel.textures.push_back( std::move( gltfTexture ) );
			iTextureIndex = TINT( a_rOutModel.textures.size() - 1 );
		}

		// Push the material
		gltfMaterial.pbrMetallicRoughness.baseColorTexture.index = iTextureIndex;
		a_rOutModel.materials.push_back( std::move( gltfMaterial ) );
		iMaterialIndex = TINT( a_rOutModel.materials.size() - 1 );
	}

	//-----------------------------------------------------------------------------
	// 1. Vertex Buffer
	//-----------------------------------------------------------------------------
	GLint iVertexBufferSize = 0;
	oVertexBuffer.GetParameter( GL_BUFFER_SIZE, iVertexBufferSize );

	// Allocate array for the vertex data
	Toshi::T2DynamicVector<TBYTE> vecVertices;
	vecVertices.SetSize( iVertexBufferSize );
	oVertexBuffer.GetSubData( vecVertices.Begin(), 0, iVertexBufferSize );

	// Insert data to the GLTF buffer
	gltfBuffer.data.insert( gltfBuffer.data.end(), vecVertices.Begin(), vecVertices.End() );

	//-----------------------------------------------------------------------------
	// 2. Vertex Buffer View
	//-----------------------------------------------------------------------------
	tinygltf::BufferView gltfBufferViewVertex;
	gltfBufferViewVertex.buffer = iBufferIndex;
	gltfBufferViewVertex.byteOffset = 0;
	gltfBufferViewVertex.byteLength = iVertexBufferSize;
	gltfBufferViewVertex.byteStride = sizeof( SkinVertex );
	gltfBufferViewVertex.target = TINYGLTF_TARGET_ARRAY_BUFFER;

	a_rOutModel.bufferViews.push_back( std::move( gltfBufferViewVertex ) );
	const TINT iVertexBufferView = TINT( a_rOutModel.bufferViews.size() - 1 );

	const TUINT uiNumTotalVertices = iVertexBufferSize / sizeof( SkinVertex );

	//-----------------------------------------------------------------------------
	// 3. Vertex Buffer Accessor
	//-----------------------------------------------------------------------------
	
	// Calculate min/max vertices
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

	SkinVertex* pVertices = (SkinVertex*)&*vecVertices.Begin();
	for ( TUINT i = 0; i < uiNumTotalVertices; i++ )
	{
		min_vals[ 0 ] = TMath::Min( min_vals[ 0 ], double( pVertices[ i ].Position.x ) );
		min_vals[ 1 ] = TMath::Min( min_vals[ 1 ], double( pVertices[ i ].Position.y ) );
		min_vals[ 2 ] = TMath::Min( min_vals[ 2 ], double( pVertices[ i ].Position.z ) );

		max_vals[ 0 ] = TMath::Max( max_vals[ 0 ], double( pVertices[ i ].Position.x ) );
		max_vals[ 1 ] = TMath::Max( max_vals[ 1 ], double( pVertices[ i ].Position.y ) );
		max_vals[ 2 ] = TMath::Max( max_vals[ 2 ], double( pVertices[ i ].Position.z ) );
	}
	
	// Define the actual accessors

	// Position
	tinygltf::Accessor gltfAccPosition;
	gltfAccPosition.bufferView = iVertexBufferView;
	gltfAccPosition.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
	gltfAccPosition.type = TINYGLTF_TYPE_VEC3;
	gltfAccPosition.count = uiNumTotalVertices;
	gltfAccPosition.byteOffset = 0;
	gltfAccPosition.minValues = std::move( min_vals );
	gltfAccPosition.maxValues = std::move( max_vals );

	a_rOutModel.accessors.push_back( std::move( gltfAccPosition ) );
	const TINT iAccPositionIndex = TINT( a_rOutModel.accessors.size() - 1 );

	// UV
	tinygltf::Accessor gltfAccUV;
	gltfAccUV.bufferView = iVertexBufferView;
	gltfAccUV.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
	gltfAccUV.type = TINYGLTF_TYPE_VEC2;
	gltfAccUV.count = uiNumTotalVertices;
	gltfAccUV.byteOffset = 32;

	a_rOutModel.accessors.push_back( std::move( gltfAccUV ) );
	const TINT iAccUVIndex = TINT( a_rOutModel.accessors.size() - 1 );

	T2_FOREACH( vecSubMeshes, it )
	{
		TSIZE uiBufferBaseOffset = gltfBuffer.data.size();

		//-----------------------------------------------------------------------------
		// 1. Index Buffer
		//-----------------------------------------------------------------------------
		GLint iIndexBufferSize = 0;
		it->oVertexArray.GetIndexBuffer().GetParameter( GL_BUFFER_SIZE, iIndexBufferSize );

		// Allocate array for the index data
		Toshi::T2DynamicVector<TBYTE> vecIndices;
		vecIndices.SetSize( iIndexBufferSize );
		it->oVertexArray.GetIndexBuffer().GetSubData( vecIndices.Begin(), 0, iIndexBufferSize );

		// Insert data to the GLTF buffer
		gltfBuffer.data.insert( gltfBuffer.data.end(), vecIndices.Begin(), vecIndices.End() );

		//-----------------------------------------------------------------------------
		// 2. Index Buffer View
		//-----------------------------------------------------------------------------
		tinygltf::BufferView gltfBufferViewIndex;
		gltfBufferViewIndex.buffer = iBufferIndex;
		gltfBufferViewIndex.byteOffset = uiBufferBaseOffset;
		gltfBufferViewIndex.byteLength = iIndexBufferSize;
		gltfBufferViewIndex.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;

		a_rOutModel.bufferViews.push_back( std::move( gltfBufferViewIndex ) );
		const TINT iIndexBufferView = TINT( a_rOutModel.bufferViews.size() - 1 );

		//-----------------------------------------------------------------------------
		// 3. Accessors
		//-----------------------------------------------------------------------------

		// Indices
		tinygltf::Accessor gltfAccIndex;
		gltfAccIndex.bufferView = iIndexBufferView;
		gltfAccIndex.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
		gltfAccIndex.type = TINYGLTF_TYPE_SCALAR;
		gltfAccIndex.count = it->uiNumIndices;

		a_rOutModel.accessors.push_back( std::move( gltfAccIndex ) );
		const TINT iAccIndicesIndex = TINT( a_rOutModel.accessors.size() - 1 );

		//-----------------------------------------------------------------------------
		// 4. Write mesh
		//-----------------------------------------------------------------------------
		tinygltf::Mesh gltfMesh;
		tinygltf::Primitive gltfPrimitive;

		// Add material or use exisiting

		gltfPrimitive.attributes[ "POSITION" ] = iAccPositionIndex;
		gltfPrimitive.attributes[ "TEXCOORD_0" ] = iAccUVIndex;
		gltfPrimitive.indices = iAccIndicesIndex;
		gltfPrimitive.mode = TINYGLTF_MODE_TRIANGLE_STRIP;
		gltfPrimitive.material = iMaterialIndex;

		gltfMesh.primitives.push_back( std::move( gltfPrimitive ) );
		a_rOutModel.meshes.push_back( std::move( gltfMesh ) );
	}

	// Add the buffer for this mesh
	a_rOutModel.buffers.push_back( std::move( gltfBuffer ) );

	// Create nodes for each of the meshes
	for ( TSIZE i = iMeshStartIndex; i < a_rOutModel.meshes.size(); i++ )
	{
		tinygltf::Node gltfNode;
		gltfNode.mesh = i;

		gltfNode.name = ( vecSubMeshes.Size() == 1 )
			? GetName()
			: TString8::VarArgs( "%s_SM%u", GetName(), i - iMeshStartIndex ).GetString();
		
		a_rOutModel.nodes.push_back( std::move( gltfNode ) );
	}

	return TTRUE;
}

void SkinMaterial::PreRender()
{
	g_pRenderGL->SetTexture2D( 0, m_pTexture->GetHandle() );
}
