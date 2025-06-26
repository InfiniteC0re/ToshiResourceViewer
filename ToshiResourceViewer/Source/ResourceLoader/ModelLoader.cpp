#include "pch.h"
#include "ModelLoader.h"
#include "Shader/SkinShader.h"

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

static void ModelLoader_LoadSkinLOD_Barnyard_Windows( PTRB* pTRB, Endianess eEndianess, TINT iIndex, ResourceLoader::ModelLOD& rOutLOD, TTMDWin::TRBLODHeader& rLODHeader )
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
		pMaterial->SetTexture( g_pTextureManager->GetInvalidTexture() );

		pMesh->SetMaterial( pMaterial );
		pMesh->vecSubMeshes.Reserve( uiNumSubMeshes );

		rOutLOD.ppMeshes[ i ] = pMesh;

		for ( TUINT k = 0; k < uiNumSubMeshes; k++ )
		{
			auto pSubMesh    = &pMesh->vecSubMeshes.PushBack();
			auto pTRBSubMesh = &pTRBMesh->m_pSubMeshes[ k ];

			pSubMesh->uiNumIndices  = pTRBSubMesh->m_uiNumIndices;
			pSubMesh->uiNumVertices = pTRBSubMesh->m_uiNumVertices1;
			pSubMesh->uiNumBones    = pTRBSubMesh->m_uiNumBones;
			TUtil::MemCopy( pSubMesh->aBones, pTRBSubMesh->m_pBones, pTRBSubMesh->m_uiNumBones * sizeof( TINT ) );

			// Create render buffers
			T2VertexArray::Unbind();
			
			if ( pSubMesh->uiNumVertices > 0 )
				pMesh->oVertexBuffer = T2Render::CreateVertexBuffer( pTRBSubMesh->m_pVertices, pSubMesh->uiNumVertices * 40, GL_STATIC_DRAW );

			pSubMesh->oIndexBuffer  = T2Render::CreateIndexBuffer( pTRBSubMesh->m_pIndices, pSubMesh->uiNumIndices, GL_STATIC_DRAW );

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

	auto pHeader    = pTRB->GetSymbols()->Find<TTMDWin::TRBWinHeader>( pTRB->GetSections(), "Header" );
	auto pMaterials = pTRB->GetSymbols()->Find<TTMDBase::MaterialsHeader>( pTRB->GetSections(), "Materials" );

	pModel->pTRB         = pTRB;
	pModel->iLODCount    = CONVERTENDIANESS( eEndianess, pHeader->m_iNumLODs );
	pModel->fLODDistance = CONVERTENDIANESS( eEndianess, pHeader->m_fLODDistance );

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
				ModelLoader_LoadSkinLOD_Barnyard_Windows( pTRB, eEndianess, i, pModel->aLODs[ i ], *pTRBLod );
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
}

void ResourceLoader::Model::Render()
{
	for ( TINT i = 0; i < aLODs[ 0 ].iNumMeshes; i++ )
	{
		if ( TMesh* pMesh = aLODs[ 0 ].ppMeshes[ i ] )
			pMesh->Render();
	}
}
