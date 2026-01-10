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

		TMatrix44 s_aBoneTransforms[ SKINNED_SUBMESH_MAX_BONES ];

		T2_FOREACH( pSkinMesh->vecSubMeshes, subMesh )
		{
			if ( pSkeletonInstance )
			{
				// Fill matrices from the skeleton instance
				for ( TUINT k = 0; k < subMesh->uiNumBones && k < SKINNED_SUBMESH_MAX_BONES; k++ )
					s_aBoneTransforms[ k ] = pSkeletonInstance->GetBone( subMesh->aBones[ k ] ).m_Transform;
			}
			else
			{
				// No skeleton instance, reset matrices
				for ( TUINT k = 0; k < subMesh->uiNumBones && k < SKINNED_SUBMESH_MAX_BONES; k++ )
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

	//-----------------------------------------------------------------------------
	// 1. Materials
	//-----------------------------------------------------------------------------
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
	// 2. Vertex Buffer
	//-----------------------------------------------------------------------------
	GLint iVertexBufferSize = 0;
	oVertexBuffer.GetParameter( GL_BUFFER_SIZE, iVertexBufferSize );

	const TUINT uiNumTotalVertices = iVertexBufferSize / sizeof( SkinVertex );

	// Allocate array for the vertex data
	Toshi::T2DynamicVector<TBYTE> vecVertices;
	vecVertices.SetSize( iVertexBufferSize );
	oVertexBuffer.GetSubData( vecVertices.Begin(), 0, iVertexBufferSize );

	// Data pre-processing
	SkinVertex* pVertices = (SkinVertex*)&*vecVertices.Begin();
	{
		// The joints NEED to be fixed!!! The shader normalizes them and divides by 3, so preprocessing is needed here!!!
		TUINT uiStartVertex = 0;
		TUINT uiPrevNumVertices = 0;
		T2_FOREACH( vecSubMeshes, it )
		{
			TUINT uiNumSubMeshVertices = ( it.Index() == 0 ) ? it->uiEndVertexId : it->uiEndVertexId - uiPrevNumVertices;
			uiPrevNumVertices          = it->uiEndVertexId;

			TASSERT( uiStartVertex < uiNumTotalVertices );
			TASSERT( uiStartVertex + uiNumSubMeshVertices <= uiNumTotalVertices );

			for ( TUINT i = uiStartVertex; i < uiStartVertex + uiNumSubMeshVertices; i++ )
			{
				pVertices[ i ].Bones[ 0 ] = it->aBones[ TUINT8( pVertices[ i ].Bones[ 0 ] / 3.0f ) ];
				pVertices[ i ].Bones[ 1 ] = it->aBones[ TUINT8( pVertices[ i ].Bones[ 1 ] / 3.0f ) ];
				pVertices[ i ].Bones[ 2 ] = it->aBones[ TUINT8( pVertices[ i ].Bones[ 2 ] / 3.0f ) ];
				pVertices[ i ].Bones[ 3 ] = it->aBones[ TUINT8( pVertices[ i ].Bones[ 3 ] / 3.0f ) ];

				// For some reason models in Barnyard can have zero weight but still assigned a bone
				// This causes errors, so has to fix it manually here
				if ( pVertices[ i ].Weights[ 0 ] == 0 ) pVertices[ i ].Bones[ 0 ] = 0;
				if ( pVertices[ i ].Weights[ 1 ] == 0 ) pVertices[ i ].Bones[ 1 ] = 0;
				if ( pVertices[ i ].Weights[ 2 ] == 0 ) pVertices[ i ].Bones[ 2 ] = 0;
				if ( pVertices[ i ].Weights[ 3 ] == 0 ) pVertices[ i ].Bones[ 3 ] = 0;
			}

			uiStartVertex += uiNumSubMeshVertices;
		}
	}

	// Insert data to the GLTF buffer
	gltfBuffer.data.insert( gltfBuffer.data.end(), vecVertices.Begin(), vecVertices.End() );

	TUINT uiStartVertex = 0;
	TUINT uiPrevNumVertices = 0;
	T2_FOREACH( vecSubMeshes, it )
	{
		TSIZE uiBufferBaseOffset   = gltfBuffer.data.size();
		TUINT uiNumSubMeshVertices = ( it.Index() == 0 ) ? it->uiEndVertexId : it->uiEndVertexId - uiPrevNumVertices;
		uiPrevNumVertices          = it->uiEndVertexId;

		//-----------------------------------------------------------------------------
		// 1. Vertex Buffer View
		//-----------------------------------------------------------------------------
		tinygltf::BufferView gltfBufferViewVertex;
		gltfBufferViewVertex.buffer     = iBufferIndex;
		gltfBufferViewVertex.byteOffset = uiStartVertex * sizeof( SkinVertex );
		gltfBufferViewVertex.byteLength = uiNumSubMeshVertices * sizeof( SkinVertex );
		gltfBufferViewVertex.byteStride = sizeof( SkinVertex );
		gltfBufferViewVertex.target     = TINYGLTF_TARGET_ARRAY_BUFFER;

		a_rOutModel.bufferViews.push_back( std::move( gltfBufferViewVertex ) );
		const TINT iVertexBufferView = TINT( a_rOutModel.bufferViews.size() - 1 );

		//-----------------------------------------------------------------------------
		// 2. Vertex Buffer Accessor
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

		for ( TUINT i = 0; i < uiNumSubMeshVertices; i++ )
		{
			min_vals[ 0 ] = TMath::Min( min_vals[ 0 ], TDOUBLE( pVertices[ uiStartVertex + i ].Position.x ) );
			min_vals[ 1 ] = TMath::Min( min_vals[ 1 ], TDOUBLE( pVertices[ uiStartVertex + i ].Position.y ) );
			min_vals[ 2 ] = TMath::Min( min_vals[ 2 ], TDOUBLE( pVertices[ uiStartVertex + i ].Position.z ) );

			max_vals[ 0 ] = TMath::Max( max_vals[ 0 ], TDOUBLE( pVertices[ uiStartVertex + i ].Position.x ) );
			max_vals[ 1 ] = TMath::Max( max_vals[ 1 ], TDOUBLE( pVertices[ uiStartVertex + i ].Position.y ) );
			max_vals[ 2 ] = TMath::Max( max_vals[ 2 ], TDOUBLE( pVertices[ uiStartVertex + i ].Position.z ) );
		}

		// Position
		tinygltf::Accessor gltfAccPosition;
		gltfAccPosition.bufferView    = iVertexBufferView;
		gltfAccPosition.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
		gltfAccPosition.type          = TINYGLTF_TYPE_VEC3;
		gltfAccPosition.count         = uiNumSubMeshVertices;
		gltfAccPosition.byteOffset    = offsetof( SkinVertex, Position );
		gltfAccPosition.minValues     = std::move( min_vals );
		gltfAccPosition.maxValues     = std::move( max_vals );

		a_rOutModel.accessors.push_back( std::move( gltfAccPosition ) );
		const TINT iAccPositionIndex = TINT( a_rOutModel.accessors.size() - 1 );

		// Normal
		tinygltf::Accessor gltfAccNormal;
		gltfAccNormal.bufferView    = iVertexBufferView;
		gltfAccNormal.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
		gltfAccNormal.type          = TINYGLTF_TYPE_VEC3;
		gltfAccNormal.count         = uiNumSubMeshVertices;
		gltfAccNormal.byteOffset    = offsetof( SkinVertex, Normal );

		a_rOutModel.accessors.push_back( std::move( gltfAccNormal ) );
		const TINT iAccNormalIndex = TINT( a_rOutModel.accessors.size() - 1 );

		// Weights
		tinygltf::Accessor gltfAccWeights;
		gltfAccWeights.bufferView    = iVertexBufferView;
		gltfAccWeights.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
		gltfAccWeights.type          = TINYGLTF_TYPE_VEC4;
		gltfAccWeights.count         = uiNumSubMeshVertices;
		gltfAccWeights.normalized    = TTRUE;
		gltfAccWeights.byteOffset    = offsetof( SkinVertex, Weights );

		a_rOutModel.accessors.push_back( std::move( gltfAccWeights ) );
		const TINT iAccWeightsIndex = TINT( a_rOutModel.accessors.size() - 1 );

		// Joints
		tinygltf::Accessor gltfAccJoints;
		gltfAccJoints.bufferView    = iVertexBufferView;
		gltfAccJoints.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
		gltfAccJoints.type          = TINYGLTF_TYPE_VEC4;
		gltfAccJoints.count         = uiNumSubMeshVertices;
		gltfAccJoints.byteOffset    = offsetof( SkinVertex, Bones );

		a_rOutModel.accessors.push_back( std::move( gltfAccJoints ) );
		const TINT iAccJointsIndex = TINT( a_rOutModel.accessors.size() - 1 );

		// UV
		tinygltf::Accessor gltfAccUV;
		gltfAccUV.bufferView    = iVertexBufferView;
		gltfAccUV.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
		gltfAccUV.type          = TINYGLTF_TYPE_VEC2;
		gltfAccUV.count         = uiNumSubMeshVertices;
		gltfAccUV.byteOffset    = offsetof( SkinVertex, UV );

		a_rOutModel.accessors.push_back( std::move( gltfAccUV ) );
		const TINT iAccUVIndex = TINT( a_rOutModel.accessors.size() - 1 );

		//-----------------------------------------------------------------------------
		// 1. Index Buffer
		//-----------------------------------------------------------------------------
		GLint iIndexBufferSize = 0;
		it->oVertexArray.GetIndexBuffer().GetParameter( GL_BUFFER_SIZE, iIndexBufferSize );

		// Allocate array for the index data
		Toshi::T2DynamicVector<TBYTE> vecIndices;
		vecIndices.SetSize( iIndexBufferSize );
		it->oVertexArray.GetIndexBuffer().GetSubData( vecIndices.Begin(), 0, iIndexBufferSize );

		for ( TINT i = 0; i < iIndexBufferSize; i += sizeof( TUINT16 ) )
		{
			TUINT16* pId = (TUINT16*)&vecIndices[ i ];

			TASSERT( *pId >= uiStartVertex );
			*pId -= uiStartVertex;
		}

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

		gltfPrimitive.attributes[ "POSITION" ] = iAccPositionIndex;
		gltfPrimitive.attributes[ "NORMAL" ] = iAccNormalIndex;
		gltfPrimitive.attributes[ "WEIGHTS_0" ] = iAccWeightsIndex;
		gltfPrimitive.attributes[ "JOINTS_0" ] = iAccJointsIndex;
		gltfPrimitive.attributes[ "TEXCOORD_0" ] = iAccUVIndex;
		gltfPrimitive.indices = iAccIndicesIndex;
		gltfPrimitive.mode = TINYGLTF_MODE_TRIANGLE_STRIP;
		gltfPrimitive.material = iMaterialIndex;

		gltfMesh.primitives.push_back( std::move( gltfPrimitive ) );
		a_rOutModel.meshes.push_back( std::move( gltfMesh ) );

		uiStartVertex += uiNumSubMeshVertices;
	}

	// Add the buffer for this mesh
	a_rOutModel.buffers.push_back( std::move( gltfBuffer ) );

	// Create nodes for each of the meshes
	for ( TSIZE i = iMeshStartIndex; i < a_rOutModel.meshes.size(); i++ )
	{
		tinygltf::Node gltfNode;
		gltfNode.mesh = i;
		gltfNode.skin = 0;

		gltfNode.name = ( vecSubMeshes.Size() == 1 )
			? GetName()
			: TString8::VarArgs( "%s_SM%u", GetName(), i - iMeshStartIndex ).GetString();
		
		a_rOutModel.nodes.push_back( std::move( gltfNode ) );
	}

	return TTRUE;
}

TBOOL SkinMesh::SerializeTRBMesh( PTRB* a_pTRB, PTRBSections::MemoryStream::Ptr<Toshi::TTMDWin::TRBLODMesh> a_pTRBMesh )
{
	auto pMemStream = a_pTRBMesh.stack();

	//-----------------------------------------------------------------------------
	// 1. Basic Info
	//-----------------------------------------------------------------------------
	const TUINT uiNumSubMeshes = vecSubMeshes.Size();
	TUINT       uiNumIndices  = 0;

	a_pTRBMesh->m_uiNumSubMeshes = a_pTRB->ConvertEndianess( uiNumSubMeshes );

	const TCHAR* pchMaterialName = GetMaterialName();
	pMemStream->Alloc<TCHAR>( &a_pTRBMesh->m_szMaterialName, T2String8::Length( pchMaterialName ) + 1 );
	T2String8::Copy( a_pTRBMesh->m_szMaterialName, pchMaterialName );

	//-----------------------------------------------------------------------------
	// 2. Vertex Buffer
	//-----------------------------------------------------------------------------
	GLint iVertexBufferSize = 0;
	oVertexBuffer.GetParameter( GL_BUFFER_SIZE, iVertexBufferSize );

	const TUINT uiNumTotalVertices = iVertexBufferSize / sizeof( SkinVertex );

	// Allocate array for the vertex data
	Toshi::T2DynamicVector<TBYTE> vecVertices;
	vecVertices.SetSize( iVertexBufferSize );
	oVertexBuffer.GetSubData( vecVertices.Begin(), 0, iVertexBufferSize );
	SkinVertex* pVertices = (SkinVertex*)&*vecVertices.Begin();

	a_pTRBMesh->m_uiNumVertices = a_pTRB->ConvertEndianess( uiNumTotalVertices );

	//-----------------------------------------------------------------------------
	// 3. Sub Meshes
	//-----------------------------------------------------------------------------
	auto pTRBSubMeshes = pMemStream->Alloc<TTMDWin::SubMesh>( &a_pTRBMesh->m_pSubMeshes, uiNumSubMeshes );

	for ( TUINT i = 0; i < uiNumSubMeshes; i++ )
	{
		auto pSubMesh    = &vecSubMeshes[ i ];
		auto pTRBSubMesh = pTRBSubMeshes + i;

		const TBOOL bAllocateVertexBuffer = ( i == 0 );
		TASSERT( bAllocateVertexBuffer || pSubMesh->uiNumAllocatedVertices == 0 );
		TASSERT( !bAllocateVertexBuffer || pSubMesh->uiNumAllocatedVertices == uiNumTotalVertices );

		pTRBSubMesh->m_uiNumVertices1 = a_pTRB->ConvertEndianess( bAllocateVertexBuffer ? uiNumTotalVertices : 0 );
		pTRBSubMesh->m_uiNumVertices2 = a_pTRB->ConvertEndianess( pSubMesh->uiEndVertexId );
		pTRBSubMesh->m_uiNumIndices   = a_pTRB->ConvertEndianess( pSubMesh->uiNumIndices );
		pTRBSubMesh->m_uiNumBones     = a_pTRB->ConvertEndianess( pSubMesh->uiNumBones );
		pTRBSubMesh->m_Zero           = a_pTRB->ConvertEndianess( 0 );
		pTRBSubMesh->m_Unk2           = a_pTRB->ConvertEndianess( 1277680 );
		pTRBSubMesh->m_Unk3           = a_pTRB->ConvertEndianess( 2034559604 );
		pTRBSubMesh->m_Unk4           = a_pTRB->ConvertEndianess( 2034728964 );
		pTRBSubMesh->m_Unk5           = a_pTRB->ConvertEndianess( 2034474770 );
		pTRBSubMesh->m_Unk6           = a_pTRB->ConvertEndianess( 2034728980 );

		// Allocate bones
		TASSERT( pSubMesh->uiNumBones <= SKINNED_SUBMESH_MAX_BONES );
		auto pTRBBones = pMemStream->Alloc<TUINT>( &pTRBSubMesh->m_pBones, pSubMesh->uiNumBones );
		for ( TUINT k = 0; k < pSubMesh->uiNumBones; k++ )
		{
			auto pBone    = pSubMesh->aBones + k;
			auto pTRBBone = pTRBBones + k;

			*pTRBBone = a_pTRB->ConvertEndianess( *pBone );
		}

		// Allocate indices
		GLint iIndexBufferSize = 0;
		pSubMesh->oVertexArray.GetIndexBuffer().GetParameter( GL_BUFFER_SIZE, iIndexBufferSize );

		Toshi::T2DynamicVector<TBYTE> vecIndices;
		vecIndices.SetSize( iIndexBufferSize );
		pSubMesh->oVertexArray.GetIndexBuffer().GetSubData( vecIndices.Begin(), 0, iIndexBufferSize );
		TUINT16* pIndices = (TUINT16*)&*vecIndices.Begin();

		TASSERT( pSubMesh->uiNumIndices == ( iIndexBufferSize / sizeof( *pIndices ) ) );

		auto pTRBIndices = pMemStream->Alloc<TUINT16>( &pTRBSubMesh->m_pIndices, pSubMesh->uiNumIndices );
		for ( TUINT k = 0; k < pSubMesh->uiNumIndices; k++ )
		{
			auto pIndex    = pIndices + k;
			auto pTRBIndex = pTRBIndices + k;

			*pTRBIndex = a_pTRB->ConvertEndianess( *pIndex );
		}

		uiNumIndices += pSubMesh->uiNumIndices;

		// Allocate vertices
		// Vertex buffer is allocated only for the first mesh
		if ( bAllocateVertexBuffer )
		{
			auto pTRBVertices = pMemStream->Alloc<TTMDWin::SkinVertex>( &pTRBSubMesh->m_pVertices, uiNumTotalVertices );

			for ( TUINT k = 0; k < uiNumTotalVertices; k++ )
			{
				auto pVertex    = pVertices + k;
				auto pTRBVertex = pTRBVertices + k;

				pTRBVertex->Position.x = a_pTRB->ConvertEndianess( pVertex->Position.x );
				pTRBVertex->Position.y = a_pTRB->ConvertEndianess( pVertex->Position.y );
				pTRBVertex->Position.z = a_pTRB->ConvertEndianess( pVertex->Position.z );

				pTRBVertex->Normal.x = a_pTRB->ConvertEndianess( pVertex->Normal.x );
				pTRBVertex->Normal.y = a_pTRB->ConvertEndianess( pVertex->Normal.y );
				pTRBVertex->Normal.z = a_pTRB->ConvertEndianess( pVertex->Normal.z );

				pTRBVertex->Weights[ 0 ] = a_pTRB->ConvertEndianess( pVertex->Weights[ 0 ] );
				pTRBVertex->Weights[ 1 ] = a_pTRB->ConvertEndianess( pVertex->Weights[ 1 ] );
				pTRBVertex->Weights[ 2 ] = a_pTRB->ConvertEndianess( pVertex->Weights[ 2 ] );
				pTRBVertex->Weights[ 3 ] = a_pTRB->ConvertEndianess( pVertex->Weights[ 3 ] );

				pTRBVertex->Bones[ 0 ] = a_pTRB->ConvertEndianess( pVertex->Bones[ 0 ] );
				pTRBVertex->Bones[ 1 ] = a_pTRB->ConvertEndianess( pVertex->Bones[ 1 ] );
				pTRBVertex->Bones[ 2 ] = a_pTRB->ConvertEndianess( pVertex->Bones[ 2 ] );
				pTRBVertex->Bones[ 3 ] = a_pTRB->ConvertEndianess( pVertex->Bones[ 3 ] );

				pTRBVertex->UV.x = a_pTRB->ConvertEndianess( pVertex->UV.x );
				pTRBVertex->UV.y = a_pTRB->ConvertEndianess( pVertex->UV.y );
			}
		}
	}

	//-----------------------------------------------------------------------------
	// 2. Finish
	//-----------------------------------------------------------------------------
	a_pTRBMesh->m_uiNumIndices = a_pTRB->ConvertEndianess( uiNumIndices );
	
	return TTRUE;
}

void SkinMesh::GetMaterialInfo( Toshi::TString8& a_rMatName, Toshi::TString8& a_rTexName )
{
	SkinMaterial* pMaterial = TSTATICCAST( SkinMaterial, GetMaterial() );

	a_rMatName = GetMaterialName();
	a_rTexName = pMaterial->AccessTexture()->GetTexture().strName;
}

void SkinMaterial::PreRender()
{
	if ( m_pTexture )
		g_pRenderGL->SetTexture2D( 0, m_pTexture->GetHandle() );
}
