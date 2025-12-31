#include "pch.h"
#include "ModelLoader.h"
#include "Shader/SkinShader.h"
#include "Resource/StreamedTexture.h"

#include <Toshi/T2String.h>
#include <Render/TModel.h>
#include <Render/TTMDWin.h>
#include <Plugins/PTRB.h>

#include <Platform/GL/T2Render_GL.h>
#include <Platform/GL/T2GLTexture_GL.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

static constexpr TUINT MAX_NUM_MODEL_MATERIALS = 150;

static Toshi::TTMDBase::MaterialsHeader s_oCurrentModelMaterialsHeader;
static Toshi::TTMDBase::Material        s_oCurrentModelMaterials[ MAX_NUM_MODEL_MATERIALS ];

static TTMDBase::Material* FindMaterialInModel( const TCHAR* a_szName )
{
	for ( TINT i = 0; i < s_oCurrentModelMaterialsHeader.iNumMaterials; i++ )
	{
		if ( TStringManager::String8Compare( s_oCurrentModelMaterials[ i ].szMatName, a_szName ) == 0 )
		{
			return &s_oCurrentModelMaterials[ i ];
		}
	}

	return TNULL;
}

static void ModelLoader_LoadSkinLOD_Barnyard_Windows( PTRB* pTRB, Endianess eEndianess, ResourceLoader::Model* pModel, TINT iIndex, TModelLOD& rOutLOD, TTMDWin::TRBLODHeader& rLODHeader )
{
	TINT iMeshCount = CONVERTENDIANESS( eEndianess, rLODHeader.m_iMeshCount1 ) + CONVERTENDIANESS( eEndianess, rLODHeader.m_iMeshCount2 );

	for ( TINT i = 0; i < iMeshCount; i++ )
	{
		T2FormatString128 symbolName;
		symbolName.Format( "LOD%d_Mesh_%d", iIndex, i );

		auto pTRBMesh = pTRB->GetSymbols()->Find<TTMDWin::TRBLODMesh>( pTRB->GetSections(), symbolName.Get() );

		if ( !pTRBMesh )
			continue;

		SkinMesh*     pMesh          = g_pSkinShader->CreateMesh();
		SkinMaterial* pMaterial      = g_pSkinShader->CreateMaterial();
		TUINT         uiNumSubMeshes = CONVERTENDIANESS( eEndianess, pTRBMesh->m_uiNumSubMeshes );
		
		// TODO: find and set real texture for this material
		auto pTexture = Resource::StreamedTexture_FindOrCreateDummy(
		    TPS8D( FindMaterialInModel( pTRBMesh->m_szMaterialName )->szTextureFile )
		);

		pMaterial->SetTexture( pTexture );
		pModel->vecUsedTextures.PushBack( pTexture );

		pMesh->SetName( symbolName.Get() );
		pMesh->SetMaterial( pMaterial );
		pMesh->vecSubMeshes.Reserve( uiNumSubMeshes );

		rOutLOD.ppMeshes[ i ] = pMesh;

		for ( TUINT k = 0; k < uiNumSubMeshes; k++ )
		{
			auto pSubMesh    = &pMesh->vecSubMeshes.PushBack();
			auto pTRBSubMesh = &pTRBMesh->m_pSubMeshes[ k ];

			pSubMesh->uiNumIndices           = pTRBSubMesh->m_uiNumIndices;
			pSubMesh->uiNumAllocatedVertices = pTRBSubMesh->m_uiNumVertices1;
			pSubMesh->uiNumUsedVertices      = pTRBSubMesh->m_uiNumVertices2;
			pSubMesh->uiNumBones             = pTRBSubMesh->m_uiNumBones;
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

T2SharedPtr<ResourceLoader::Model> ResourceLoader::Model_Load_Barnyard_Windows( PTRB* pTRB, Endianess eEndianess )
{
	T2SharedPtr<ResourceLoader::Model> pModel = T2SharedPtr<ResourceLoader::Model>::New();

	auto pHeader         = pTRB->GetSymbols()->Find<TTMDWin::TRBWinHeader>( pTRB->GetSections(), "Header" );
	auto pMaterials      = pTRB->GetSymbols()->Find<TTMDBase::MaterialsHeader>( pTRB->GetSections(), "Materials" );
	auto pSkeletonHeader = pTRB->GetSymbols()->Find<TTMDBase::SkeletonHeader>( pTRB->GetSections(), "SkeletonHeader" );
	auto pSkeleton       = pTRB->GetSymbols()->Find<TSkeleton>( pTRB->GetSections(), "Skeleton" );

	TUtil::MemCopy( s_oCurrentModelMaterials, pMaterials.get() + 1, pMaterials->uiSectionSize );
	s_oCurrentModelMaterialsHeader = *pMaterials;

	pModel->pTRB             = pTRB;
	pModel->iLODCount        = CONVERTENDIANESS( eEndianess, pHeader->m_iNumLODs );
	pModel->fLODDistance     = CONVERTENDIANESS( eEndianess, pHeader->m_fLODDistance );
	pModel->bAnimationsReady = TFALSE;

	if ( pSkeleton )
	{
		pModel->pSkeleton = new TSkeleton();
		
		TUtil::MemCopy( pModel->pSkeleton, pSkeleton.get(), sizeof( TSkeleton ) );

		pModel->pSkeleton->m_pBones = new TSkeletonBone[ pSkeleton->m_iBoneCount ];
		TUtil::MemCopy( pModel->pSkeleton->m_pBones, pSkeleton->m_pBones, sizeof( TSkeletonBone ) * pSkeleton->m_iBoneCount );

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
		pModel->pKeyLib = Resource::StreamedKeyLib_FindOrCreateDummy( TPS8D( pSkeletonHeader->m_szTKLName ) );

		Model_PrepareAnimations( pModel );
	}

	for ( TINT i = 0; i < CONVERTENDIANESS( eEndianess, pHeader->m_iNumLODs ); i++ )
	{
		auto pTRBLod = pHeader->GetLOD( i );
		
		pModel->aLODs[ i ].iNumMeshes      = CONVERTENDIANESS( eEndianess, pTRBLod->m_iMeshCount1 ) + CONVERTENDIANESS( eEndianess, pTRBLod->m_iMeshCount2 );
		pModel->aLODs[ i ].ppMeshes        = new TMesh*[ pModel->aLODs[ i ].iNumMeshes ];
		pModel->aLODs[ i ].BoundingSphere  = pTRBLod->m_RenderVolume;

		switch ( pTRBLod->m_eShader )
		{
			case TTMDWin::ST_WORLD:
				//LoadWorldMeshTRB( a_pModel, i, &a_pModel->m_LODs[ i ], pTRBLod );
				break;
			case TTMDWin::ST_SKIN:
				ModelLoader_LoadSkinLOD_Barnyard_Windows( pTRB, eEndianess, pModel, i, pModel->aLODs[ i ], *pTRBLod );
				break;
			case TTMDWin::ST_GRASS:
				//LoadGrassMeshTRB( a_pModel, i, &a_pModel->m_LODs[ i ], pTRBLod );
				break;
			default:
				TASSERT( !"The model is using an unknown shader" );
				return {};
				break;
		}
	}

	return pModel;
}

TBOOL ResourceLoader::Model_PrepareAnimations( Model* pModel )
{
	if ( !pModel->bAnimationsReady && pModel->pKeyLib && pModel->pKeyLib->IsLoaded() )
	{
		pModel->pSkeleton->m_KeyLibraryInstance.CreateEx(
		    pModel->pKeyLib->GetLibrary(),
		    pModel->oSkeletonHeader.m_iTKeyCount,
		    pModel->oSkeletonHeader.m_iQKeyCount,
		    pModel->oSkeletonHeader.m_iSKeyCount,
		    pModel->oSkeletonHeader.m_iTBaseIndex,
		    pModel->oSkeletonHeader.m_iQBaseIndex,
		    pModel->oSkeletonHeader.m_iSBaseIndex
		);

		pModel->bAnimationsReady = TTRUE;
	}

	return pModel->bAnimationsReady;
}

TBOOL ResourceLoader::Model_CreateInstance( Toshi::T2SharedPtr<Model> pModel, ModelInstance& rOutInstance )
{
	rOutInstance.pModel = pModel;
	rOutInstance.oTransform.SetMatrix( TMatrix44::IDENTITY );
	rOutInstance.pSkeletonInstance = ( pModel->pSkeleton ) ? pModel->pSkeleton->CreateInstance( TTRUE ) : TNULL;

	return TTRUE;
}

ResourceLoader::Model::Model()
{
	iLODCount           = 0;
	iNumCollisionMeshes = 0;
	pSkeleton           = TNULL;
	pCollisionMeshes    = TNULL;
	pTRB                = TNULL;
}

ResourceLoader::Model::~Model()
{
	for ( TINT i = 0; i < iLODCount; i++ )
	{
		if ( aLODs[ i ].ppMeshes )
		{
			for ( TINT k = 0; k < aLODs[ i ].iNumMeshes; k++ )
			{
				aLODs[ i ].ppMeshes[ k ]->DestroyResource();
			}

			delete[] aLODs[ i ].ppMeshes;
		}
	}

	if ( pSkeleton )
	{
		for ( TINT i = 0; i < pSkeleton->m_iSequenceCount; i++ )
		{
			for ( TINT k = 0; k < pSkeleton->GetAutoBoneCount(); k++ )
				delete[] pSkeleton->m_SkeletonSequences[ i ].m_pSeqBones[ k ].m_pData;

			delete[] pSkeleton->m_SkeletonSequences[ i ].m_pSeqBones;
		}

		delete[] pSkeleton->m_SkeletonSequences;
		delete[] pSkeleton->m_pBones;

		pSkeleton->m_KeyLibraryInstance.Destroy();
		delete pSkeleton;
	}
}

void ResourceLoader::Model::Render()
{
	for ( TINT i = 0; i < aLODs[ 0 ].iNumMeshes; i++ )
	{
		if ( TMesh* pMesh = aLODs[ 0 ].ppMeshes[ i ] )
			pMesh->Render();
	}
}

ResourceLoader::ModelInstance::~ModelInstance()
{
	if ( pSkeletonInstance )
	{
		pSkeletonInstance->RemoveAllAnimations();
		delete pSkeletonInstance;
	}
}
