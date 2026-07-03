#include "pch.h"
#include "ModelLoader.h"
#include "Shader/SkinShader.h"
#include "Shader/WorldShader.h"
#include "Resource/StreamedTexture.h"

#include <Render/TTMDWin.h>

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

static void ModelLoader_LoadCollision_Barnyard( PTRB* pTRB, ResourceLoader::Model* pModel )
{
	auto pCollisionHeader = pTRB->GetSymbols()->Find<TTMDBase::CollisionHeader>( pTRB->GetSections(), "Collision" );
	if ( !pCollisionHeader ) return;

	pModel->iNumCollisionMeshes = pTRB->ConvertEndianess( pCollisionHeader->m_iNumMeshes );
	if ( pModel->iNumCollisionMeshes <= 0 ) return;

	pModel->pCollisionMeshes = new ResourceLoader::Model::CollisionMeshInfo[ pModel->iNumCollisionMeshes ];

	for ( TINT i = 0; i < pModel->iNumCollisionMeshes; i++ )
	{
		auto& rCollisionMesh = pCollisionHeader->m_pMeshes[ i ];

		const TINT  iBoneID       = pTRB->ConvertEndianess( rCollisionMesh.m_iBoneID );
		const TUINT uiNumVertices = pTRB->ConvertEndianess( rCollisionMesh.m_uiNumVertices );
		const TUINT uiNumIndices  = pTRB->ConvertEndianess( rCollisionMesh.m_uiNumIndices );

		auto& rOutCollisionMesh = pModel->pCollisionMeshes[ i ];
		rOutCollisionMesh.iBoneID       = iBoneID;
		rOutCollisionMesh.uiNumVertices = uiNumVertices;
		rOutCollisionMesh.uiNumIndices  = uiNumIndices;

		const TUINT uiNumCollTypes = pTRB->ConvertEndianess( rCollisionMesh.m_uiNumCollTypes );
		for ( TUINT k = 0; k < uiNumCollTypes; k++ )
		{
			auto& rCollisionGroup = rCollisionMesh.m_pCollGroups[ k ];

			auto& rOutCollisionGroup = rOutCollisionMesh.vecGroups.PushBack();
			rOutCollisionGroup.strName    = rCollisionGroup.pszName ? rCollisionGroup.pszName : "default";
			rOutCollisionGroup.uiNumFaces = pTRB->ConvertEndianess( rCollisionGroup.uiNumFaces );
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

//-----------------------------------------------------------------------------
// World model TRB binary structure definitions
//-----------------------------------------------------------------------------
namespace WorldTRB
{

struct CellMesh
{
	Toshi::TMesh* pMesh;
	TUINT32       uiNumIndices;
	TUINT32       uiNumVertices1;
	TUINT16       uiNumVertices2;
	TCHAR*        szMaterialName;
	void*         pVertices; // WorldMesh::WorldVertex*
	TUINT16*      pIndices;
};

struct CellMeshSphere
{
	Toshi::TSphere m_BoundingSphere;
	CellMesh*      m_pCellMesh;
};

struct CellSphereTreeLeafNode
{
	TUINT32 m_uiNumMeshes;

	TUINT16& GetMeshIndex( TUINT32 a_uiIndex )
	{
		TASSERT( a_uiIndex < m_uiNumMeshes );
		return *( (TUINT16*)( this + 1 ) + a_uiIndex );
	}
};

struct CellSphereTreeBranchNode
{
	Toshi::TSphere            m_BoundingSphere;
	CellSphereTreeBranchNode* m_pRight;

	TBOOL IsLeaf() const { return m_pRight == TNULL; }

	CellSphereTreeLeafNode* GetLeafNode()
	{
		TASSERT( IsLeaf() );
		return (CellSphereTreeLeafNode*)( this + 1 );
	}

	CellSphereTreeBranchNode* GetSubNode()
	{
		TASSERT( !IsLeaf() );
		return (CellSphereTreeBranchNode*)( this + 1 );
	}
};

struct Cell
{
	TUINT                     uiFlags;
	TCHAR                     UNKNOWNDATA1[ 108 ];
	TINT                      m_iSomeCount;
	TCHAR                     UNKNOWNDATA2[ 12 ];
	void*                     pNode;
	TUINT32                   uiNumMeshes;
	CellMeshSphere**          ppCellMeshSpheres;
	CellSphereTreeBranchNode* pTreeBranchNodes;
};

struct World
{
	TINT32 m_iNumCells;
	Cell** m_ppCells;
};

struct WorldDatabase
{
	TUINT32 m_uiNumWorlds;
	World** m_ppWorlds;
};

} // namespace WorldTRB

static void ModelLoader_LoadWorldTreeIntersect(
    PTRB*                               pTRB,
    ResourceLoader::Model*              pModel,
    WorldTRB::CellSphereTreeBranchNode* a_pNode,
    WorldTRB::Cell*                     a_pCell,
    TModelLOD&                          rOutLOD,
    ResourceLoader::MaterialCache&      rMatCache
)
{
	auto pNode = a_pNode;

	// Traverse non-leaf nodes: recurse left, iterate right spine
	while ( !pNode->IsLeaf() )
	{
		ModelLoader_LoadWorldTreeIntersect( pTRB, pModel, pNode->GetSubNode(), a_pCell, rOutLOD, rMatCache );
		pNode = pNode->m_pRight;
	}

	TASSERT( pNode->IsLeaf() );
	auto pLeafNode = pNode->GetLeafNode();

	for ( TUINT i = 0; i < pLeafNode->m_uiNumMeshes; i++ )
	{
		TASSERT( s_iWorldMeshIndex < TUINT( rOutLOD.iNumMeshes ) );
		if ( s_iWorldMeshIndex >= TUINT( rOutLOD.iNumMeshes ) )
			break;

		auto pCellMeshSphere = a_pCell->ppCellMeshSpheres[ pLeafNode->GetMeshIndex( i ) ];
		auto pTerrainMesh    = pCellMeshSphere->m_pCellMesh;

		WorldMesh* pMesh = g_pWorldShader->CreateMesh();

		T2SharedPtr<WorldMaterial> pMaterial;
		if ( auto pCachedMat = rMatCache.Find( pTerrainMesh->szMaterialName ) )
		{
			TASSERT( pCachedMat->IsA( &TGetClass( WorldMaterial ) ) );
			pMaterial = pCachedMat;
		}

		if ( !pMaterial )
		{
			pMaterial = g_pWorldShader->CreateMaterial();

			auto pMatInfo = FindMaterialInModel( pTerrainMesh->szMaterialName );
			if ( pMatInfo )
			{
				auto pTexture = Resource::StreamedTexture_FindOrCreateDummy( TPS8D( pMatInfo->szTextureFile ) );
				pMaterial->SetTexture( pTexture );
				pModel->vecUsedTextures.PushBack( pTexture );
			}

			pMaterial->SetName( pTerrainMesh->szMaterialName );
			rMatCache.Touch( pTerrainMesh->szMaterialName, pMaterial );
		}

		pMesh->SetName( pTerrainMesh->szMaterialName );
		pMesh->SetMaterialName( pTerrainMesh->szMaterialName );
		pMesh->SetMaterial( pMaterial );

		rOutLOD.ppMeshes[ s_iWorldMeshIndex++ ] = pMesh;

		// Upload vertex data to GL
		if ( pTerrainMesh->uiNumVertices1 > 0 && pTerrainMesh->pVertices != TNULL )
		{
			pMesh->oVertexBuffer = T2Render::CreateVertexBuffer(
			    pTerrainMesh->pVertices,
			    pTerrainMesh->uiNumVertices1 * sizeof( WorldMesh::WorldVertex ),
			    GL_STATIC_DRAW
			);
		}

		// Create submesh: index buffer + VAO with WorldVertex attribute layout
		if ( pTerrainMesh->uiNumIndices > 0 && pTerrainMesh->pIndices != TNULL )
		{
			auto& subMesh         = pMesh->vecSubMeshes.PushBack();
			subMesh.uiNumIndices  = pTerrainMesh->uiNumIndices;
			subMesh.uiNumVertices = pTerrainMesh->uiNumVertices1;

			T2VertexArray::Unbind();

			subMesh.oIndexBuffer = T2Render::CreateIndexBuffer(
			    pTerrainMesh->pIndices, pTerrainMesh->uiNumIndices, GL_STATIC_DRAW
			);
			subMesh.oVertexArray = T2Render::CreateVertexArray( pMesh->oVertexBuffer, subMesh.oIndexBuffer );
			subMesh.oVertexArray.Bind();
			subMesh.oVertexArray.GetVertexBuffer().SetAttribPointer( 0, 3, GL_FLOAT, sizeof( WorldMesh::WorldVertex ), offsetof( WorldMesh::WorldVertex, Position ) );
			subMesh.oVertexArray.GetVertexBuffer().SetAttribPointer( 1, 3, GL_FLOAT, sizeof( WorldMesh::WorldVertex ), offsetof( WorldMesh::WorldVertex, Normal ) );
			subMesh.oVertexArray.GetVertexBuffer().SetAttribPointer( 2, 3, GL_FLOAT, sizeof( WorldMesh::WorldVertex ), offsetof( WorldMesh::WorldVertex, Color ) );
			subMesh.oVertexArray.GetVertexBuffer().SetAttribPointer( 3, 2, GL_FLOAT, sizeof( WorldMesh::WorldVertex ), offsetof( WorldMesh::WorldVertex, UV ) );
		}
	}
}

static void ModelLoader_LoadWorldLOD_Barnyard_Windows( PTRB* pTRB, ResourceLoader::Model* pModel, TModelLOD& rOutLOD, ResourceLoader::MaterialCache& rMatCache )
{
	s_iWorldMeshIndex = 0;

	auto pDatabase = pTRB->GetSymbols()->Find<WorldTRB::WorldDatabase>( pTRB->GetSections(), "Database" );
	if ( !pDatabase )
	{
		TASSERT( !"World model TRB is missing Database symbol" );
		return;
	}

	for ( TUINT i = 0; i < pDatabase->m_uiNumWorlds; i++ )
	{
		auto pWorld = pDatabase->m_ppWorlds[ i ];

		for ( TINT k = 0; k < pWorld->m_iNumCells; k++ )
		{
			auto pCell = pWorld->m_ppCells[ k ];
			if ( pCell->pTreeBranchNodes )
				ModelLoader_LoadWorldTreeIntersect( pTRB, pModel, pCell->pTreeBranchNodes, pCell, rOutLOD, rMatCache );
		}
	}

	// Shrink the mesh count to the number actually loaded so the destructor doesn't
	// try to destroy uninitialized entries in the remainder of ppMeshes
	rOutLOD.iNumMeshes = TINT( s_iWorldMeshIndex );
}


static void ModelLoader_LoadSkinLOD_Barnyard_Windows( PTRB* pTRB, Endianess eEndianess, ResourceLoader::Model* pModel, TINT iIndex, TModelLOD& rOutLOD, TTMDWin::TRBLODHeader& rLODHeader, ResourceLoader::MaterialCache& rMatCache )
{
	TINT iMeshCount = CONVERTENDIANESS( eEndianess, rLODHeader.m_iMeshCount1 ) + CONVERTENDIANESS( eEndianess, rLODHeader.m_iMeshCount2 );

	for ( TINT i = 0; i < iMeshCount; i++ )
	{
		T2FormatString128 symbolName;
		symbolName.Format( "LOD%d_Mesh_%d", iIndex, i );

		auto pTRBMesh = pTRB->GetSymbols()->Find<TTMDWin::TRBMeshLODHeader>( pTRB->GetSections(), symbolName.Get() );

		if ( !pTRBMesh )
			continue;

		SkinMesh*     pMesh          = g_pSkinShader->CreateMesh();
		TUINT         uiNumSubMeshes = CONVERTENDIANESS( eEndianess, pTRBMesh->m_uiNumSubMeshes );

		// TODO: find and set real texture for this material
		auto pTexture = Resource::StreamedTexture_FindOrCreateDummy(
		    TPS8D( FindMaterialInModel( pTRBMesh->m_szMaterialName )->szTextureFile )
		);

		T2SharedPtr<SkinMaterial> pMaterial;
		if ( auto pCachedMat = rMatCache.Find( pTRBMesh->m_szMaterialName ) )
		{
			TASSERT( pCachedMat->IsA( &TGetClass( SkinMaterial ) ) );
			pMaterial = pCachedMat;
		}

		if ( !pMaterial )
		{
			pMaterial = g_pSkinShader->CreateMaterial();
			pMaterial->SetTexture( pTexture );
			pMaterial->SetName( pTRBMesh->m_szMaterialName );

			rMatCache.Touch( pTRBMesh->m_szMaterialName, pMaterial );
		}

		pModel->vecUsedTextures.PushBack( pTexture );

		pMesh->SetName( symbolName.Get() );
		pMesh->SetMaterialName( pTRBMesh->m_szMaterialName );
		pMesh->SetMaterial( pMaterial );
		pMesh->vecSubMeshes.Reserve( uiNumSubMeshes );

		rOutLOD.ppMeshes[ i ] = pMesh;

		for ( TUINT k = 0; k < uiNumSubMeshes; k++ )
		{
			auto pSubMesh    = &pMesh->vecSubMeshes.PushBack();
			auto pTRBSubMesh = &pTRBMesh->m_pSubMeshes[ k ];

			pSubMesh->uiNumIndices           = pTRBSubMesh->m_uiNumIndices;
			pSubMesh->uiNumAllocatedVertices = pTRBSubMesh->m_uiNumVertices1;
			pSubMesh->uiEndVertexId          = pTRBSubMesh->m_uiNumVertices2;
			pSubMesh->uiNumBones             = pTRBSubMesh->m_uiNumBones;

			TASSERT( pTRBSubMesh->m_uiNumBones <= SKINNED_SUBMESH_MAX_BONES );
			TUtil::MemCopy( pSubMesh->aBones, pTRBSubMesh->m_pBones, pTRBSubMesh->m_uiNumBones * sizeof( TINT ) );

			// Create render buffers
			T2VertexArray::Unbind();

			if ( pSubMesh->uiNumAllocatedVertices > 0 )
				pMesh->oVertexBuffer = T2Render::CreateVertexBuffer( pTRBSubMesh->m_pVertices, pSubMesh->uiNumAllocatedVertices * 40, GL_STATIC_DRAW );

			pSubMesh->oIndexBuffer = T2Render::CreateIndexBuffer( pTRBSubMesh->m_pIndices, pSubMesh->uiNumIndices, GL_STATIC_DRAW );

			pSubMesh->oVertexArray = T2Render::CreateVertexArray( pMesh->oVertexBuffer, pSubMesh->oIndexBuffer );
			pSubMesh->oVertexArray.Bind();
			pSubMesh->oVertexArray.GetVertexBuffer().SetAttribPointer( 0, 3, GL_FLOAT, sizeof( SkinMesh::SkinVertex ), offsetof( SkinMesh::SkinVertex, Position ) );
			pSubMesh->oVertexArray.GetVertexBuffer().SetAttribPointer( 1, 3, GL_FLOAT, sizeof( SkinMesh::SkinVertex ), offsetof( SkinMesh::SkinVertex, Normal ) );
			pSubMesh->oVertexArray.GetVertexBuffer().SetAttribPointer( 2, 4, GL_UNSIGNED_BYTE, sizeof( SkinMesh::SkinVertex ), offsetof( SkinMesh::SkinVertex, Weights ), GL_TRUE );
			pSubMesh->oVertexArray.GetVertexBuffer().SetAttribPointer( 3, 4, GL_UNSIGNED_BYTE, sizeof( SkinMesh::SkinVertex ), offsetof( SkinMesh::SkinVertex, Bones ), GL_TRUE );
			pSubMesh->oVertexArray.GetVertexBuffer().SetAttribPointer( 4, 2, GL_FLOAT, sizeof( SkinMesh::SkinVertex ), offsetof( SkinMesh::SkinVertex, UV ) );
		}
	}
}

T2SharedPtr<ResourceLoader::Model> ResourceLoader::Model_Load_Barnyard_Windows( PTRB* pTRB )
{
	MaterialCache matCache;

	const Endianess eEndianess = pTRB->GetEndianess();

	T2SharedPtr<ResourceLoader::Model> pModel = T2SharedPtr<ResourceLoader::Model>::New();

	auto pHeader         = pTRB->GetSymbols()->Find<TTMDWin::TRBWinHeader>( pTRB->GetSections(), "Header" );
	auto pMaterials      = pTRB->GetSymbols()->Find<TTMDBase::MaterialsHeader>( pTRB->GetSections(), "Materials" );
	auto pSkeletonHeader = pTRB->GetSymbols()->Find<TTMDBase::SkeletonHeader>( pTRB->GetSections(), "SkeletonHeader" );
	auto pSkeleton       = pTRB->GetSymbols()->Find<TSkeleton>( pTRB->GetSections(), "Skeleton" );

	TUtil::MemCopy( s_oCurrentModelMaterials, pMaterials.get() + 1, pTRB->ConvertEndianess( pMaterials->uiSectionSize ) );
	s_oCurrentModelMaterialsHeader = *pMaterials;

	pModel->pTRB               = pTRB;
	pModel->iLODCount          = pTRB->ConvertEndianess( pHeader->m_iNumLODs );
	pModel->aLODDistances[ 0 ] = pTRB->ConvertEndianess( pHeader->m_fLODDistance );
	pModel->bAnimationsLoaded  = TFALSE;

	if ( pSkeleton )
	{
		pModel->pSkeleton = new TSkeleton();

		TUtil::MemCopy( pModel->pSkeleton, pSkeleton.get(), sizeof( TSkeleton ) );

		pModel->pSkeleton->m_pBones = new TSkeletonBone[ pTRB->ConvertEndianess( pSkeleton->m_iBoneCount ) ];
		TUtil::MemCopy( pModel->pSkeleton->m_pBones, pSkeleton->m_pBones, sizeof( TSkeletonBone ) * pTRB->ConvertEndianess( pSkeleton->m_iBoneCount ) );

		pModel->pSkeleton->m_SkeletonSequences = new TSkeletonSequence[ pSkeleton->m_iSequenceCount ];

		const TINT iAutoBoneCount = pModel->pSkeleton->GetAutoBoneCount();

		for ( TINT i = 0; i < pSkeleton->m_iSequenceCount; i++ )
		{
			TUtil::MemCopy( &pModel->pSkeleton->m_SkeletonSequences[ i ], &pSkeleton->m_SkeletonSequences[ i ], sizeof( TSkeletonSequence ) );

			pModel->pSkeleton->m_SkeletonSequences[ i ].m_pSeqBones = new TSkeletonSequenceBone[ iAutoBoneCount ];
			TUtil::MemCopy( pModel->pSkeleton->m_SkeletonSequences[ i ].m_pSeqBones, pSkeleton->m_SkeletonSequences[ i ].m_pSeqBones, sizeof( TSkeletonSequenceBone ) * iAutoBoneCount );

			for ( TINT k = 0; k < iAutoBoneCount; k++ )
			{
				TSIZE uiDataSize = pSkeleton->m_SkeletonSequences[ i ].m_pSeqBones[ k ].m_iNumKeys * pSkeleton->m_SkeletonSequences[ i ].m_pSeqBones[ k ].m_iKeySize;

				pModel->pSkeleton->m_SkeletonSequences[ i ].m_pSeqBones[ k ].m_pData = new TBYTE[ uiDataSize ];
				TUtil::MemCopy(
				    pModel->pSkeleton->m_SkeletonSequences[ i ].m_pSeqBones[ k ].m_pData,
				    pSkeleton->m_SkeletonSequences[ i ].m_pSeqBones[ k ].m_pData,
				    uiDataSize
				);
			}
		}
	}

	if ( pSkeletonHeader )
	{
		pModel->oSkeletonHeader = *pSkeletonHeader;
		pModel->pKeyLib         = Resource::StreamedKeyLib_FindOrCreateDummy( TPS8D( pSkeletonHeader->m_szTKLName ) );

		Model_PrepareAnimations( pModel.Get() );
	}

	ModelType eFiguredOutModelType = ModelType::None;

	for ( TINT i = 0; i < pTRB->ConvertEndianess( pHeader->m_iNumLODs ); i++ )
	{
		auto pTRBLod = pHeader->GetLOD( i );

		pModel->aLODs[ i ].iNumMeshes     = pTRB->ConvertEndianess( pTRBLod->m_iMeshCount1 ) + pTRB->ConvertEndianess( pTRBLod->m_iMeshCount2 );
		pModel->aLODs[ i ].ppMeshes       = new TMesh*[ pModel->aLODs[ i ].iNumMeshes ](); // zero-initialise
		pModel->aLODs[ i ].BoundingSphere = pTRB->ConvertEndianess( pTRBLod->m_RenderVolume );

		switch ( pTRB->ConvertEndianess( pTRBLod->m_eShader ) )
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

		switch ( pTRB->ConvertEndianess( pTRBLod->m_eShader ) )
		{
			case TTMDBase::SHADERTYPE_GRASS:
			case TTMDBase::SHADERTYPE_WORLD:
				ModelLoader_LoadWorldLOD_Barnyard_Windows( pTRB, pModel.Get(), pModel->aLODs[ i ], matCache );
				break;
			case TTMDBase::SHADERTYPE_STATICINSTANCE:
			case TTMDBase::SHADERTYPE_SKIN:
				ModelLoader_LoadSkinLOD_Barnyard_Windows( pTRB, eEndianess, pModel.Get(), i, pModel->aLODs[ i ], *pTRBLod, matCache );
				break;
			default:
				TASSERT( !"The model is using an unknown shader" );
				return {};
				break;
		}
	}

	ModelLoader_LoadCollision_Barnyard( pTRB, pModel.Get() );

	pModel->eModelType = eFiguredOutModelType;

	return pModel;
}
