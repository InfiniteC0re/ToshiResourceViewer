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

		// First pass: decode quantised int16 positions/UVs and accumulate face normals
		for ( TUINT s = 0; s < uiNumSubMeshes; s++ )
		{
			TTMDPS2::StaticInstance::SubMesh& subMesh = pTRBMesh->m_pSubMeshes[ s ];

			TUINT NV = subMesh.m_uiNumVertices;

			const TINT16* pPosData = subMesh.m_pPositions + TTMDPS2::StaticInstance::POSITIONS_HDR_SIZE / sizeof( TINT16 );
			const TINT16* pUVData  = subMesh.m_pUVs + TTMDPS2::StaticInstance::UV_HDR_SIZE / sizeof( TINT16 );

			for ( TUINT v = 0; v < NV; v++ )
			{
				WorldMesh::WorldVertex& dst = pVertices[ uiVtxOffset + v ];

				dst.Position = TVector3( pPosData[ v * 3 + 0 ], pPosData[ v * 3 + 1 ], pPosData[ v * 3 + 2 ] );
				dst.Position.Multiply( TTMDPS2::StaticInstance::POSITION_SCALE );

				dst.Normal                  = TVector3( 0.0f, 0.0f, 0.0f );
				dst.Color                   = TVector3( 1.0f, 1.0f, 1.0f );
				dst.UV.x                    = pUVData[ v * 2 + 0 ] * TTMDPS2::StaticInstance::UV_SCALE;
				dst.UV.y                    = 1.0f - pUVData[ v * 2 + 1 ] * TTMDPS2::StaticInstance::UV_SCALE;
			}

			// Walk the u8 triangle strip to accumulate face normals per vertex
			const TUINT8* pIdxData = subMesh.m_pIndices + TTMDPS2::StaticInstance::INDEX_HDR_SIZE;
			TUINT         NI       = subMesh.m_uiNumIndices;
			TINT          stripLen = 0;
			TUINT8        win[ 2 ] = {};

			for ( TUINT k = 0; k < NI; k++ )
			{
				TUINT8 raw = pIdxData[ k ];
				TUINT8 vi  = raw & 0x7F;
				TBOOL  adc = ( raw & 0x80 ) != 0;

				if ( adc )
				{
					win[ 0 ] = vi;
					stripLen = 1;
				}
				else if ( stripLen < 2 )
				{
					win[ stripLen++ ] = vi;
				}
				else
				{
					TVector3& pa = pVertices[ uiVtxOffset + win[ 0 ] ].Position;
					TVector3& pb = pVertices[ uiVtxOffset + win[ 1 ] ].Position;
					TVector3& pc = pVertices[ uiVtxOffset + vi ].Position;

					TVector3 edge1( pb.x - pa.x, pb.y - pa.y, pb.z - pa.z );
					TVector3 edge2( pc.x - pa.x, pc.y - pa.y, pc.z - pa.z );
					TVector3 faceNormal;
					faceNormal.CrossProduct( edge1, edge2 );

					pVertices[ uiVtxOffset + win[ 0 ] ].Normal += faceNormal;
					pVertices[ uiVtxOffset + win[ 1 ] ].Normal += faceNormal;
					pVertices[ uiVtxOffset + vi ].Normal += faceNormal;

					win[ 0 ] = win[ 1 ];
					win[ 1 ] = vi;
				}
			}

			uiVtxOffset += NV;
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

		// Second pass: expand each sub-mesh's u8 strip into a TUINT16 triangle list
		pMesh->vecSubMeshes.Reserve( uiNumSubMeshes );
		uiVtxOffset = 0;

		for ( TUINT s = 0; s < uiNumSubMeshes; s++ )
		{
			TTMDPS2::StaticInstance::SubMesh& subMesh = pTRBMesh->m_pSubMeshes[ s ];

			TUINT NV = subMesh.m_uiNumVertices;
			TUINT NI = subMesh.m_uiNumIndices;

			TUINT16*      pTriBuf    = new TUINT16[ NI * 3 ];
			TUINT         uiTriCount = 0;
			TINT          stripLen   = 0;
			TUINT16       win[ 2 ]   = {};
			const TUINT8* pIdxData   = subMesh.m_pIndices + TTMDPS2::StaticInstance::INDEX_HDR_SIZE;

			for ( TUINT k = 0; k < NI; k++ )
			{
				TUINT8  raw = pIdxData[ k ];
				TUINT16 vi  = TUINT16( ( raw & 0x7F ) + uiVtxOffset );
				TBOOL   adc = ( raw & 0x80 ) != 0;

				if ( adc )
				{
					win[ 0 ] = vi;
					stripLen = 1;
				}
				else if ( stripLen < 2 )
				{
					win[ stripLen++ ] = vi;
				}
				else
				{
					pTriBuf[ uiTriCount * 3 + 0 ] = win[ 0 ];
					pTriBuf[ uiTriCount * 3 + 1 ] = win[ 1 ];
					pTriBuf[ uiTriCount * 3 + 2 ] = vi;
					uiTriCount++;
					win[ 0 ] = win[ 1 ];
					win[ 1 ] = vi;
				}
			}

			if ( uiTriCount > 0 )
			{
				auto& sm         = pMesh->vecSubMeshes.PushBack();
				sm.uiNumIndices  = uiTriCount * 3;
				sm.uiNumVertices = NV;

				T2VertexArray::Unbind();

				sm.oIndexBuffer = T2Render::CreateIndexBuffer( pTriBuf, uiTriCount * 3, GL_STATIC_DRAW );
				sm.oVertexArray = T2Render::CreateVertexArray( pMesh->oVertexBuffer, sm.oIndexBuffer );
				sm.oVertexArray.Bind();
				sm.oVertexArray.GetVertexBuffer().SetAttribPointer( 0, 3, GL_FLOAT, sizeof( WorldMesh::WorldVertex ), offsetof( WorldMesh::WorldVertex, Position ) );
				sm.oVertexArray.GetVertexBuffer().SetAttribPointer( 1, 3, GL_FLOAT, sizeof( WorldMesh::WorldVertex ), offsetof( WorldMesh::WorldVertex, Normal ) );
				sm.oVertexArray.GetVertexBuffer().SetAttribPointer( 2, 3, GL_FLOAT, sizeof( WorldMesh::WorldVertex ), offsetof( WorldMesh::WorldVertex, Color ) );
				sm.oVertexArray.GetVertexBuffer().SetAttribPointer( 3, 2, GL_FLOAT, sizeof( WorldMesh::WorldVertex ), offsetof( WorldMesh::WorldVertex, UV ) );
			}

			delete[] pTriBuf;
			uiVtxOffset += NV;
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
	pModel->aLODDistances[ 0 ] = pPS2Header->m_fLODDistance;
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

	pModel->eModelType = eFiguredOutModelType;

	return pModel;
}
