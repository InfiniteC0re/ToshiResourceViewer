#include "pch.h"
#include "ModelLoader.h"
#include "Shader/SkinShader.h"
#include "Shader/WorldShader.h"
#include "Resource/StreamedTexture.h"

#include <Render/TTMDPS2.h>

#include <Platform/GL/T2Render_GL.h>
#include <Platform/GL/T2GLTexture_GL.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

static TUINT s_iWorldMeshIndex = 0;

static constexpr TUINT MAX_NUM_MODEL_MATERIALS = 150;

static Toshi::TTMDBase::MaterialsHeader s_oCurrentModelMaterialsHeader;
static Toshi::TTMDBase::Material        s_oCurrentModelMaterials[ MAX_NUM_MODEL_MATERIALS ];

// NOTE: replicates Windows code path, haven't tested it
static void ModelLoader_LoadCollision_Barnyard_PS2( PTRB* pTRB, ResourceLoader::Model* pModel )
{
	auto pCollisionHeader = pTRB->GetSymbols()->Find<TTMDBase::CollisionHeader>( pTRB->GetSections(), "Collision" );
	if ( !pCollisionHeader ) return;

	pModel->iNumCollisionMeshes = pCollisionHeader->m_iNumMeshes;
	if ( pModel->iNumCollisionMeshes <= 0 ) return;

	pModel->pCollisionMeshes = new ResourceLoader::Model::CollisionMeshInfo[ pModel->iNumCollisionMeshes ];

	for ( TINT i = 0; i < pModel->iNumCollisionMeshes; i++ )
	{
		auto& rCollisionMesh = pCollisionHeader->m_pMeshes[ i ];

		auto& rOutCollisionMesh = pModel->pCollisionMeshes[ i ];
		rOutCollisionMesh.iBoneID       = rCollisionMesh.m_iBoneID;
		rOutCollisionMesh.uiNumVertices = rCollisionMesh.m_uiNumVertices;
		rOutCollisionMesh.uiNumIndices  = rCollisionMesh.m_uiNumIndices;
		rOutCollisionMesh.vecVertices.SetSize( rCollisionMesh.m_uiNumVertices );
		rOutCollisionMesh.vecIndices.SetSize( rCollisionMesh.m_uiNumIndices );

		for ( TUINT k = 0; k < rCollisionMesh.m_uiNumVertices; k++ )
		{
			rOutCollisionMesh.vecVertices[ k ] = rCollisionMesh.m_pVertices[ k ];
		}

		for ( TUINT k = 0; k < rCollisionMesh.m_uiNumIndices; k++ )
		{
			rOutCollisionMesh.vecIndices[ k ] = rCollisionMesh.m_pIndices[ k ];
		}

		for ( TUINT k = 0; k < rCollisionMesh.m_uiNumCollTypes; k++ )
		{
			auto& rCollisionGroup = rCollisionMesh.m_pCollGroups[ k ];

			auto& rOutCollisionGroup = rOutCollisionMesh.vecGroups.PushBack();
			rOutCollisionGroup.strName    = rCollisionGroup.pszName ? rCollisionGroup.pszName : "default";
			rOutCollisionGroup.uiNumFaces = rCollisionGroup.uiNumFaces;
		}
	}
}

static TTMDBase::Material* FindMaterialInModel( const TCHAR* a_szName )
{
	for ( TINT i = 0; i < s_oCurrentModelMaterialsHeader.iNumMaterials; i++ )
	{
		if ( TStringManager::String8CompareNoCase( s_oCurrentModelMaterials[ i ].szMatName, a_szName ) == 0 )
		{
			return &s_oCurrentModelMaterials[ i ];
		}
	}

	return TNULL;
}

static void ModelLoader_LoadStaticInstanceLOD_Barnyard_PS2(
    PTRB*                  pTRB,
    ResourceLoader::Model* pModel,
    TINT                   iLODIndex,
    TModelLOD&             rOutLOD,
    TTMDPS2::TRBLODHeader& rLODHeader,
    ResourceLoader::MaterialCache& rMatCache
)
{
	TINT iMeshCount = rLODHeader.m_iMeshCount1 + rLODHeader.m_iMeshCount2;

	for ( TINT i = 0; i < iMeshCount; i++ )
	{
		T2FormatString128 symbolName;
		symbolName.Format( "LOD%d_Mesh_%d", iLODIndex, i );

		auto pTRBMesh = pTRB->GetSymbols()->Find<TTMDPS2::StaticInstance::TRBMeshLODHeader>(
		    pTRB->GetSections(), symbolName.Get()
		);

		if ( !pTRBMesh )
			continue;

		const TCHAR*               szMatName = pTRBMesh->m_szMaterialName;
		T2SharedPtr<WorldMaterial> pMaterial;
		if ( auto pCachedMat = rMatCache.Find( szMatName ) )
		{
			TASSERT( pCachedMat->IsA( &TGetClass( WorldMaterial ) ) );
			pMaterial = pCachedMat;
		}

		if ( !pMaterial )
		{
			pMaterial = g_pWorldShader->CreateMaterial();

			auto pMatInfo = FindMaterialInModel( szMatName );
			if ( pMatInfo )
			{
				auto pTexture = Resource::StreamedTexture_FindOrCreateDummy( TPS8D( pMatInfo->szTextureFile ) );
				pMaterial->SetTexture( pTexture );
				pModel->vecUsedTextures.PushBack( pTexture );
			}

			pMaterial->SetName( szMatName );
			rMatCache.Touch( szMatName, pMaterial );
		}

		WorldMesh* pMesh = g_pWorldShader->CreateMesh();
		pMesh->SetName( szMatName );
		pMesh->SetMaterialName( szMatName );
		pMesh->SetMaterial( pMaterial );
		rOutLOD.ppMeshes[ i ] = pMesh;

		TUINT uiNumSubMeshes = pTRBMesh->m_uiNumSubMeshes;

		// Count total vertices across all sub-meshes for the combined vertex buffer
		TUINT uiTotalVerts = 0;
		for ( TUINT s = 0; s < uiNumSubMeshes; s++ )
			uiTotalVerts += pTRBMesh->m_pSubMeshes[ s ].m_uiNumVertices;

		WorldMesh::WorldVertex* pVertices   = new WorldMesh::WorldVertex[ uiTotalVerts ];
		TUINT                   uiVtxOffset = 0;

		// First pass: decode quantised int16 positions/UVs/colours into the shared
		// vertex buffer (normals are accumulated from the triangles below)
		for ( TUINT s = 0; s < uiNumSubMeshes; s++ )
		{
			TTMDPS2::StaticInstance::SubMesh& subMesh = pTRBMesh->m_pSubMeshes[ s ];

			TUINT NV = subMesh.m_uiNumVertices;

			const TINT16* pPosData   = subMesh.m_pPositions + TTMDPS2::StaticInstance::POSITIONS_HDR_SIZE / sizeof( TINT16 );
			const TINT16* pUVData    = subMesh.m_pUVs + TTMDPS2::StaticInstance::UV_HDR_SIZE / sizeof( TINT16 );
			const TUINT8* pColorData = subMesh.m_pColors + TTMDPS2::StaticInstance::COLOR_HDR_SIZE;

			for ( TUINT v = 0; v < NV; v++ )
			{
				WorldMesh::WorldVertex& dst = pVertices[ uiVtxOffset + v ];

				dst.Position = TVector3( pPosData[ v * 3 + 0 ], pPosData[ v * 3 + 1 ], pPosData[ v * 3 + 2 ] );
				dst.Position.Multiply( TTMDPS2::StaticInstance::POSITION_SCALE );

				const TFLOAT flLightIntensity = TMath::Max( pColorData[ v ] - 128, 0 ) / 128.0f;

				dst.Normal                  = TVector3( 0.0f, 0.0f, 0.0f );
				dst.Color                   = TVector3( flLightIntensity, flLightIntensity, flLightIntensity );
				dst.UV.x                    = pUVData[ v * 2 + 0 ] * TTMDPS2::StaticInstance::UV_SCALE;
				dst.UV.y                    = pUVData[ v * 2 + 1 ] * TTMDPS2::StaticInstance::UV_SCALE;
			}

			uiVtxOffset += NV;
		}

		auto fnDecodeTriangles = [ &pVertices ]( const TTMDPS2::StaticInstance::SubMesh& a_rSubMesh, TUINT a_uiVtxOffset, auto&& a_fnEmit ) {
			const TUINT8* pIdxData = a_rSubMesh.m_pIndices + TTMDPS2::StaticInstance::INDEX_HDR_SIZE;
			const TUINT   NI       = a_rSubMesh.m_uiNumIndices;
			TUINT8        uiPrev2  = 0;
			TUINT8        uiPrev1  = 0;

			for ( TUINT k = 0; k < NI; k++ )
			{
				const TUINT8 uiRaw = pIdxData[ k ];
				const TUINT8 vi    = uiRaw & 0x7F;
				const TBOOL  bSkip = ( uiRaw & 0x80 ) != 0;

				// Skip suppressed kicks and degenerate (repeated-vertex) triangles
				if ( k >= 2 && !bSkip && vi != uiPrev1 && vi != uiPrev2 && uiPrev1 != uiPrev2 )
				{
					TUINT8 a = uiPrev2;
					TUINT8 b = uiPrev1;

					// Odd strip position flips winding; swap to keep it consistent
					if ( k & 1 )
					{
						const TUINT8 t = a;
						a              = b;
						b              = t;
					}

					a_fnEmit( TUINT16( a + a_uiVtxOffset ), TUINT16( b + a_uiVtxOffset ), TUINT16( vi + a_uiVtxOffset ) );
				}

				uiPrev2 = uiPrev1;
				uiPrev1 = vi;
			}
		};

		// First index pass: accumulate face normals per vertex
		uiVtxOffset = 0;
		for ( TUINT s = 0; s < uiNumSubMeshes; s++ )
		{
			fnDecodeTriangles( pTRBMesh->m_pSubMeshes[ s ], uiVtxOffset, [ & ]( TUINT16 ia, TUINT16 ib, TUINT16 ic ) {
				TVector3& pa = pVertices[ ia ].Position;
				TVector3& pb = pVertices[ ib ].Position;
				TVector3& pc = pVertices[ ic ].Position;

				TVector3 edge1( pb.x - pa.x, pb.y - pa.y, pb.z - pa.z );
				TVector3 edge2( pc.x - pa.x, pc.y - pa.y, pc.z - pa.z );
				TVector3 faceNormal;
				faceNormal.CrossProduct( edge1, edge2 );

				pVertices[ ia ].Normal += faceNormal;
				pVertices[ ib ].Normal += faceNormal;
				pVertices[ ic ].Normal += faceNormal;
			} );

			uiVtxOffset += pTRBMesh->m_pSubMeshes[ s ].m_uiNumVertices;
		}

		// Normalize the accumulated vertex normals; fall back to up-vector if degenerate
		for ( TUINT v = 0; v < uiTotalVerts; v++ )
		{
			TVector3& n = pVertices[ v ].Normal;

			if ( n.MagnitudeSq() > 1e-12f )
				n.Normalize();
			else
				n = TVector3( 0.0f, 1.0f, 0.0f );
		}

		pMesh->oVertexBuffer = T2Render::CreateVertexBuffer(
		    pVertices,
		    uiTotalVerts * sizeof( WorldMesh::WorldVertex ),
		    GL_STATIC_DRAW
		);
		delete[] pVertices;

		// Second index pass: build and upload each sub-mesh's triangle list
		pMesh->vecSubMeshes.Reserve( uiNumSubMeshes );
		uiVtxOffset = 0;

		for ( TUINT s = 0; s < uiNumSubMeshes; s++ )
		{
			TTMDPS2::StaticInstance::SubMesh& subMesh = pTRBMesh->m_pSubMeshes[ s ];

			Toshi::T2DynamicVector<TUINT16> vecIndices;
			vecIndices.Reserve( subMesh.m_uiNumIndices * 3 );

			fnDecodeTriangles( subMesh, uiVtxOffset, [ & ]( TUINT16 ia, TUINT16 ib, TUINT16 ic ) {
				vecIndices.PushBack( ia );
				vecIndices.PushBack( ib );
				vecIndices.PushBack( ic );
			} );

			uiVtxOffset += subMesh.m_uiNumVertices;

			if ( vecIndices.Size() == 0 )
				continue;

			auto& sm         = pMesh->vecSubMeshes.PushBack();
			sm.uiNumIndices  = TUINT32( vecIndices.Size() );
			sm.uiNumVertices = subMesh.m_uiNumVertices;
			sm.bTriangleList = TTRUE;

			T2VertexArray::Unbind();

			sm.oIndexBuffer = T2Render::CreateIndexBuffer( vecIndices.Begin(), sm.uiNumIndices, GL_STATIC_DRAW );
			sm.oVertexArray = T2Render::CreateVertexArray( pMesh->oVertexBuffer, sm.oIndexBuffer );
			sm.oVertexArray.Bind();
			sm.oVertexArray.GetVertexBuffer().SetAttribPointer( 0, 3, GL_FLOAT, sizeof( WorldMesh::WorldVertex ), offsetof( WorldMesh::WorldVertex, Position ) );
			sm.oVertexArray.GetVertexBuffer().SetAttribPointer( 1, 3, GL_FLOAT, sizeof( WorldMesh::WorldVertex ), offsetof( WorldMesh::WorldVertex, Normal ) );
			sm.oVertexArray.GetVertexBuffer().SetAttribPointer( 2, 3, GL_FLOAT, sizeof( WorldMesh::WorldVertex ), offsetof( WorldMesh::WorldVertex, Color ) );
			sm.oVertexArray.GetVertexBuffer().SetAttribPointer( 3, 2, GL_FLOAT, sizeof( WorldMesh::WorldVertex ), offsetof( WorldMesh::WorldVertex, UV ) );
		}
	}
}

Toshi::T2SharedPtr<ResourceLoader::Model> ResourceLoader::Model_Load_Barnyard_PS2( PTRB* pTRB )
{
	ResourceLoader::MaterialCache matCache;

	auto pPS2Header  = pTRB->GetSymbols()->Find<TTMDPS2::TRBPS2Header>( pTRB->GetSections(), "Header" );
	auto pMaterials  = pTRB->GetSymbols()->Find<TTMDBase::MaterialsHeader>( pTRB->GetSections(), "Materials" );
	auto pSkelHeader = pTRB->GetSymbols()->Find<TTMDBase::SkeletonHeader>( pTRB->GetSections(), "SkeletonHeader" );

	if ( !pPS2Header )
		return {};

	T2SharedPtr<ResourceLoader::Model> pModel = T2SharedPtr<ResourceLoader::Model>::New();

	if ( pMaterials )
	{
		TUtil::MemCopy( s_oCurrentModelMaterials, pMaterials.get() + 1, pMaterials->uiSectionSize );
		s_oCurrentModelMaterialsHeader = *pMaterials;
	}

	pModel->eModelType         = ModelType::Skin;
	pModel->pTRB               = pTRB;
	pModel->iLODCount          = pPS2Header->m_iNumLODs;
	pModel->fRenderDistance    = pPS2Header->m_fLODDistance;
	pModel->bAnimationsLoaded  = TFALSE;
	pModel->pSkeleton          = TNULL;

	if ( pSkelHeader )
		pModel->oSkeletonHeader = *pSkelHeader;

	ModelType eFiguredOutModelType = ModelType::None;
	for ( TINT i = 0; i < pPS2Header->m_iNumLODs; i++ )
	{
		TTMDPS2::TRBLODHeader* pLODHeader = pPS2Header->GetLOD( i );
		TINT                   iMeshCount = pLODHeader->m_iMeshCount1 + pLODHeader->m_iMeshCount2;

		pModel->aLODs[ i ].iNumMeshes     = iMeshCount;
		pModel->aLODs[ i ].ppMeshes       = new TMesh*[ iMeshCount ]();
		pModel->aLODs[ i ].BoundingSphere = pLODHeader->m_RenderVolume;

		switch ( pLODHeader->m_eShader )
		{
			case TTMDBase::SHADERTYPE_GRASS:
				TASSERT( eFiguredOutModelType == ModelType::None || eFiguredOutModelType == ModelType::Grass );
				eFiguredOutModelType = ModelType::Grass;
				break;
			case TTMDBase::SHADERTYPE_WORLD:
				TASSERT( eFiguredOutModelType == ModelType::None || eFiguredOutModelType == ModelType::World );
				eFiguredOutModelType = ModelType::World;
				break;
			case TTMDBase::SHADERTYPE_STATICINSTANCE:
				TASSERT( eFiguredOutModelType == ModelType::None || eFiguredOutModelType == ModelType::StaticInstance );
				eFiguredOutModelType = ModelType::StaticInstance;
				break;
			case TTMDBase::SHADERTYPE_SKIN:
				TASSERT( eFiguredOutModelType == ModelType::None || eFiguredOutModelType == ModelType::Skin );
				eFiguredOutModelType = ModelType::Skin;
				break;
		}

		switch ( pLODHeader->m_eShader )
		{
			case TTMDBase::SHADERTYPE_STATICINSTANCE:
				ModelLoader_LoadStaticInstanceLOD_Barnyard_PS2( pTRB, pModel.Get(), i, pModel->aLODs[ i ], *pLODHeader, matCache );
				break;
			default:
				TASSERT( !"The model is using an unknown shader" );
				return {};
				break;
		}
	}

	//ModelLoader_LoadCollision_Barnyard_PS2( pTRB, pModel.Get() );

	pModel->eModelType = eFiguredOutModelType;

	return pModel;
}
