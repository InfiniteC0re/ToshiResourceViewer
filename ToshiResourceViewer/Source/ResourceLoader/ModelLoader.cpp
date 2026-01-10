#include "pch.h"
#include "ModelLoader.h"
#include "Shader/SkinShader.h"
#include "Resource/StreamedTexture.h"
#include "NvTriStrip/NvTriStrip.h"

#include <Toshi/T2String.h>
#include <Toshi/T2Vector.h>
#include <Toshi/TBitField.h>
#include <Render/TModel.h>
#include <Render/TTMDWin.h>
#include <Plugins/PTRB.h>

#include <Platform/GL/T2Render_GL.h>
#include <Platform/GL/T2GLTexture_GL.h>

#include <tiny_gltf.h>

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

// Specify builder to compile models with common key library
static TKLBuilder* s_pTKLBuilder = TNULL;

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
		pMaterial->SetName( pTRBMesh->m_szMaterialName );
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

T2SharedPtr<ResourceLoader::Model> ResourceLoader::Model_Load_Barnyard_Windows( PTRB* pTRB, Endianess eEndianess )
{
	T2SharedPtr<ResourceLoader::Model> pModel = T2SharedPtr<ResourceLoader::Model>::New();

	auto pHeader         = pTRB->GetSymbols()->Find<TTMDWin::TRBWinHeader>( pTRB->GetSections(), "Header" );
	auto pMaterials      = pTRB->GetSymbols()->Find<TTMDBase::MaterialsHeader>( pTRB->GetSections(), "Materials" );
	auto pSkeletonHeader = pTRB->GetSymbols()->Find<TTMDBase::SkeletonHeader>( pTRB->GetSections(), "SkeletonHeader" );
	auto pSkeleton       = pTRB->GetSymbols()->Find<TSkeleton>( pTRB->GetSections(), "Skeleton" );

	TUtil::MemCopy( s_oCurrentModelMaterials, pMaterials.get() + 1, pMaterials->uiSectionSize );
	s_oCurrentModelMaterialsHeader = *pMaterials;

	pModel->pTRB              = pTRB;
	pModel->iLODCount         = CONVERTENDIANESS( eEndianess, pHeader->m_iNumLODs );
	pModel->fLODDistance      = CONVERTENDIANESS( eEndianess, pHeader->m_fLODDistance );
	pModel->bAnimationsLoaded = TFALSE;

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

Toshi::T2SharedPtr<ResourceLoader::Model> ResourceLoader::Model_LoadSkin_GLTF( Toshi::T2StringView pchFilePath )
{
	TINFO( "Importing skin model from GLTF file: %s\n", pchFilePath.Get() );

	T2SharedPtr<ResourceLoader::Model> pModel = T2SharedPtr<ResourceLoader::Model>::New();

	// Load GLTF
	tinygltf::Model    gltfModel;
	tinygltf::TinyGLTF gltfLoader;

	std::string strError;
	std::string strWarning;

	TBOOL bLoadedGLTF = gltfLoader.LoadASCIIFromFile( &gltfModel, &strError, &strWarning, pchFilePath.Get() );
	TASSERT( bLoadedGLTF == TTRUE );

	if ( !bLoadedGLTF )
	{
		TERROR( "An error has occured while loading the GLTF file\n" );
		if ( !strError.empty() ) TERROR( "ERROR: %s\n", strError.c_str() );
		return {};
	}

	// Initialise the model
	pModel->pTRB              = NULL;
	pModel->iLODCount         = 1;
	pModel->fLODDistance      = 50.0f;
	pModel->bAnimationsLoaded = TFALSE;

	// We can't have more than 1 skins on a single model
	TASSERT( gltfModel.skins.size() <= 1 );

	// Setup skeleton
	T2Map<TINT, TINT> mapGltfBoneToTRVBone;
	const TBOOL bHasSkins = gltfModel.skins.size() == 1;
	if ( bHasSkins )
	{
		TINFO( "Detected skin\n" );

		auto pGLTFSkin = &gltfModel.skins[ 0 ];

		// Initialise TSkeleton
		pModel->pSkeleton = new TSkeleton();

		const TINT iNumBones = TINT( pGLTFSkin->joints.size() );
		const TINT iNumSeq   = TINT( gltfModel.animations.size() );

		pModel->pSkeleton->m_iBoneCount         = iNumBones;
		pModel->pSkeleton->m_iManualBoneCount   = 0;
		pModel->pSkeleton->m_iSequenceCount     = iNumSeq;
		pModel->pSkeleton->m_iAnimationMaxCount = iNumSeq; // TODO: calculate it for better memory management?
		pModel->pSkeleton->m_iInstanceCount     = 0;
		pModel->pSkeleton->m_eQuatLerpType      = TSkeleton::QUATINTERP_Default;

		TINFO( "Found %d bones\n", iNumBones );
		TINFO( "Found %d sequences\n", iNumSeq );

		// Setup bones
		TBitField<128> oWorldSpaceBones;
		pModel->pSkeleton->m_pBones = new TSkeletonBone[ iNumBones ];
		TSkeletonBone* pBones       = pModel->pSkeleton->m_pBones;
		for ( TINT i = 0; i < iNumBones; i++ )
		{
			auto  gltfBoneIdx = pGLTFSkin->joints[ i ];
			auto& gltfBone    = gltfModel.nodes[ gltfBoneIdx ];
			auto& strBoneName = gltfBone.name;

			TINFO( "Bone %d: %s\n", i, strBoneName.c_str() );

			// Copy name
			T2String8::Copy( pBones[ i ].m_szName, strBoneName.c_str(), sizeof( pBones[ i ].m_szName ) - 1 );
			pBones[ i ].m_iNameLength = TUINT8( TMath::Min( strBoneName.size(), sizeof( pBones[ i ].m_szName ) - 1 ) );

			// Setup transform (local to parent)
			// Later will need to update it to be world transform
			if ( !gltfBone.rotation.empty() )
				pBones[ i ].m_Rotation = TQuaternion( TFLOAT( gltfBone.rotation[ 0 ] ), TFLOAT( gltfBone.rotation[ 1 ] ), TFLOAT( gltfBone.rotation[ 2 ] ), TFLOAT( gltfBone.rotation[ 3 ] ) );
			else
				pBones[ i ].m_Rotation = TQuaternion::IDENTITY;

			if ( !gltfBone.translation.empty() )
				pBones[ i ].m_Position = TVector3( TFLOAT( gltfBone.translation[ 0 ] ), TFLOAT( gltfBone.translation[ 1 ] ), TFLOAT( gltfBone.translation[ 2 ] ) );
			else
				pBones[ i ].m_Position = TVector3( 0.0f, 0.0f, 0.0f );

			pBones[ i ].m_Transform.SetFromQuaternion( pBones[ i ].m_Rotation );
			pBones[ i ].m_Transform.SetTranslation( pBones[ i ].m_Position );
			pBones[ i ].m_TransformInv.Invert( pBones[ i ].m_Transform );
			pBones[ i ].m_iParentBone = TBONE_INVALID;
			oWorldSpaceBones.Set( i, TTRUE );

			// Will setup parent bone later when all of the bones are added...
			mapGltfBoneToTRVBone.Insert( gltfBoneIdx, i );
		}

		// Setup parent bones
		for ( TINT i = 0; i < iNumBones; i++ )
		{
			auto  gltfBoneIdx = pGLTFSkin->joints[ i ];
			auto& gltfBone    = gltfModel.nodes[ gltfBoneIdx ];

			const TINT           iThisBoneIdx = mapGltfBoneToTRVBone[ gltfBoneIdx ]->second;
			const TSkeletonBone* pThisBone    = &pBones[ iThisBoneIdx ];

			for ( TUINT k = 0; k < gltfBone.children.size(); k++ )
			{
				TINT iChildrenBoneIdx = mapGltfBoneToTRVBone[ gltfBone.children[ k ] ]->second;

				pBones[ iChildrenBoneIdx ].m_iParentBone = iThisBoneIdx;
				oWorldSpaceBones.Set( iChildrenBoneIdx, TFALSE );
			}
		}

		// Transform bones into world space
		while ( TTRUE )
		{
			TINT iNumValidBones = 0;

			for ( TINT i = 0; i < iNumBones; i++ )
			{
				if ( oWorldSpaceBones.IsSet( i ) )
				{
					// This bone's transform is already in world space, skip...
					iNumValidBones += 1;
					continue;
				}

				TSkeletonBone* pBone = &pBones[ i ];
				TASSERT( pBone->m_iParentBone != TBONE_INVALID );

				TSkeletonBone* pParent = &pBones[ pBone->m_iParentBone ];
				if ( !oWorldSpaceBones.IsSet( pBone->m_iParentBone ) )
				{
					// Parent of this bone is still not transformed, skip for now...
					continue;
				}

				TMatrix44 matTransform;
				matTransform.Multiply( pParent->m_Transform, pBone->m_Transform );

				pBone->m_Transform = matTransform;
				pBone->m_TransformInv.Invert( pBone->m_Transform );
				oWorldSpaceBones.Set( i, TTRUE );
			}

			if ( iNumValidBones == iNumBones ) break;
		}

		// Setup animations
		TKLBuilder  oTKLBuilder;
		const TBOOL bGlobalTKLBuilder = s_pTKLBuilder != TNULL;
		TKLBuilder* pTKLBuilder       = bGlobalTKLBuilder ? s_pTKLBuilder : &oTKLBuilder;

		pModel->pSkeleton->m_SkeletonSequences = new TSkeletonSequence[ iNumSeq ];
		TSkeletonSequence* pSeqs               = pModel->pSkeleton->m_SkeletonSequences;
		for ( TINT i = 0; i < iNumSeq; i++ )
		{
			auto  pSeq        = &pSeqs[ i ];
			auto& gltfAnim    = gltfModel.animations[ i ];
			auto  strAnimName = TString8( gltfAnim.name.c_str() );

			// Calculate animation duration and find all keys
			T2Map<TINT, T2DynamicVector<TINT>> mapBoneChannels;

			TFLOAT flAnimDuration = 0.0f;
			for ( TSIZE k = 0; k < gltfAnim.channels.size(); k++ )
			{
				auto& gltfAnimChannel = gltfAnim.channels[ k ];
				auto& gltfAnimSampler = gltfAnim.samplers[ gltfAnimChannel.sampler ];
				TASSERT( gltfModel.accessors[ gltfAnimSampler.input ].maxValues.empty() == TFALSE );

				const TFLOAT flKeyTime = TFLOAT( gltfModel.accessors[ gltfAnimSampler.input ].maxValues[ 0 ] );
				flAnimDuration = TMath::Max( flAnimDuration, flKeyTime );

				const TINT iTRBBone = mapGltfBoneToTRVBone[ gltfAnimChannel.target_node ]->second;
				if ( mapBoneChannels.Find( iTRBBone ) == mapBoneChannels.End() )
					mapBoneChannels.Insert( iTRBBone, {} );

				// Add channel to the array
				auto itChannels = mapBoneChannels[ iTRBBone ];
				itChannels->second.PushBack( TINT( k ) );
			}

			// Copy name
			T2String8::Copy( pSeq->m_szName, strAnimName.GetString(), sizeof( pSeq->m_szName ) - 1 );
			pSeq->m_iNameLength = TMath::Min( strAnimName.Length(), TINT( sizeof( pSeq->m_szName ) - 1 ) );

			pSeq->m_eFlags        = strAnimName.EndsWithNoCase( "_overlay" ) ? TSkeletonSequence::FLAG_OVERLAY : TSkeletonSequence::FLAG_NONE;
			pSeq->m_eMode         = TSkeletonSequence::MODE_LOOPED;
			pSeq->m_iNumUsedBones = iNumBones;
			pSeq->m_fDuration     = flAnimDuration;
			pSeq->m_pSeqBones     = new TSkeletonSequenceBone[ iNumBones ];

			TBOOL bWarned = TFALSE;
			TINFO( "Sequence %d: %s (%fs)\n", i, strAnimName.GetString(), flAnimDuration );

			TSkeletonSequenceBone* pSeqBones = pSeq->m_pSeqBones;
			for ( TINT k = 0; k < iNumBones; k++ )
			{
				auto pSeqBone = &pSeqBones[ k ];

				auto itChannels = mapBoneChannels.Find( k );
				const TBOOL bAnimated = itChannels != mapBoneChannels.End();

				if ( !bAnimated )
				{
					pSeqBone->m_eFlags   = 2;
					pSeqBone->m_iKeySize = 4;
					pSeqBone->m_iNumKeys = 0;
					pSeqBone->m_pData    = new TBYTE[ sizeof( TUINT16 ) ];
 					pSeqBone->GetKey( 0 )[ 0 ] = 0;
				}
				else
				{
					TASSERT( itChannels->second.Size() >= 1 );
					const TBOOL bTranslationAnimated = itChannels->second.Size() == 2;

					struct KeyFrame
					{
						TINT iTranslation = -1;
						TINT iRotation    = -1;
					};

					// Complete list of keyframes for this bone
					T2Map<TUINT16, KeyFrame> mapKeyFrames;
					T2_FOREACH( itChannels->second, it )
					{
						auto& gltfAnimChannel = gltfAnim.channels[ *it ];
						auto& gltfAnimSampler = gltfAnim.samplers[ gltfAnimChannel.sampler ];

						const TBOOL bIsRotation    = gltfAnimChannel.target_path == "rotation";
						const TBOOL bIsTranslation = !bIsRotation && gltfAnimChannel.target_path == "translation";

						if ( !bIsRotation && !bIsTranslation )
						{
							if ( !bWarned )
								TWARN( "Unsupported channel: '%s'\n", gltfAnimChannel.target_path.c_str() );

							bWarned = TTRUE;
							continue;
						}

						// Time
						auto& gltfTimeAccessor   = gltfModel.accessors[ gltfAnimSampler.input ];
						auto& gltfTimeBufferView = gltfModel.bufferViews[ gltfTimeAccessor.bufferView ];
						auto& gltfTimeBuffer     = gltfModel.buffers[ gltfTimeBufferView.buffer ];

						TASSERT( gltfTimeAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT );
						TASSERT( gltfTimeAccessor.type == TINYGLTF_TYPE_SCALAR );
						auto        pGltfDataTime      = gltfTimeBuffer.data.begin() + gltfTimeBufferView.byteOffset;
						const TUINT uiTimeBufferStride = ( gltfTimeBufferView.byteStride != 0 ) ? gltfTimeBufferView.byteStride : sizeof( TFLOAT );

						// Data (rotation or translation)
						auto& gltfDataAccessor   = gltfModel.accessors[ gltfAnimSampler.output ];
						auto& gltfDataBufferView = gltfModel.bufferViews[ gltfDataAccessor.bufferView ];
						auto& gltfDataBuffer     = gltfModel.buffers[ gltfDataBufferView.buffer ];

						TASSERT( gltfDataAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT );
						TASSERT( gltfDataAccessor.type == TINYGLTF_TYPE_VEC3 || gltfDataAccessor.type == TINYGLTF_TYPE_VEC4 );

						auto        pGltfData          = gltfDataBuffer.data.begin() + gltfDataBufferView.byteOffset;
						const TUINT uiDataBufferStride = ( gltfDataBufferView.byteStride != 0 ) ? gltfDataBufferView.byteStride : ( bIsRotation ? sizeof( TVector4 ) : sizeof( TVector3 ) );

						const TSIZE uiNumKeyFrames = gltfTimeAccessor.count;
						for ( TSIZE j = 0; j < uiNumKeyFrames; j++ )
						{
							TFLOAT flTime = *(TFLOAT*)( &*( pGltfDataTime + ( uiTimeBufferStride * j ) + gltfTimeAccessor.byteOffset ) );
							TUINT16 uiCompressedTime = TUINT16( ( flTime / flAnimDuration ) * 65535 );

							// Find or create keyframe
							KeyFrame* pKeyFrame  = TNULL;
							auto      itKeyFrame = mapKeyFrames.Find( uiCompressedTime );
							if ( itKeyFrame == mapKeyFrames.End() ) pKeyFrame = mapKeyFrames.Insert( uiCompressedTime, {} );
							else pKeyFrame = &itKeyFrame->second;

							if ( bIsRotation )
							{
								TASSERT( pKeyFrame->iRotation == -1 );
								TQuaternion& quatRotation = *(TQuaternion*)( &*( pGltfData + ( uiDataBufferStride * j ) + gltfDataAccessor.byteOffset ) );

								pKeyFrame->iRotation = pTKLBuilder->AddRotation( quatRotation );
							}
							else if ( bIsTranslation )
							{
								TASSERT( pKeyFrame->iTranslation == -1 );
								TVector3& vPosition = *(TVector3*)( &*( pGltfData + ( uiDataBufferStride * j ) + gltfDataAccessor.byteOffset ) );
								
								pKeyFrame->iTranslation = pTKLBuilder->AddTranslation( vPosition );
							}
						}
					}

					TASSERT( mapKeyFrames.Size() <= 65535 );

					// Finally write the data
					pSeqBone->m_eFlags   = bTranslationAnimated ? 1 : 0;
					pSeqBone->m_iKeySize = bTranslationAnimated ? 6 : 4;
					pSeqBone->m_iNumKeys = TUINT16( mapKeyFrames.Size() );
					pSeqBone->m_pData    = new TBYTE[ pSeqBone->m_iKeySize * pSeqBone->m_iNumKeys ];

					TINT iKeyFrameIndex = 0;
					T2_FOREACH( mapKeyFrames, it )
					{
						TASSERT( it->second.iRotation != -1 );
						TASSERT( !bTranslationAnimated || it->second.iTranslation != -1 );
						
						TUINT16* pKey = pSeqBone->GetKey( iKeyFrameIndex++ );

						pKey[ 0 ] = it->first;
						pKey[ 1 ] = it->second.iRotation;
						if ( bTranslationAnimated ) pKey[ 2 ] = it->second.iTranslation;
					}
				}
			}
		}

		if ( bGlobalTKLBuilder )
		{
			// Use name from the global builder
			TASSERT( pTKLBuilder->GetName().Length() > 0 );
			T2String8::Copy( pModel->oSkeletonHeader.m_szTKLName, pTKLBuilder->GetName(), sizeof( pModel->oSkeletonHeader.m_szTKLName ) - 1 );
		}
		else
		{
			// Generate name
			static TINT s_iTKLId = 0;
			TString8 strTKLName = TString8::VarArgs( "dyn_tkl%d", s_iTKLId++ );
			TASSERT( strTKLName.Length() <= sizeof( pModel->oSkeletonHeader.m_szTKLName ) - 1 && "Curse me if this happened" );
			T2String8::Copy( pModel->oSkeletonHeader.m_szTKLName, strTKLName.GetString(), sizeof( pModel->oSkeletonHeader.m_szTKLName ) - 1 );
		}

		pModel->oSkeletonHeader.m_iTKeyCount  = pTKLBuilder->GetTranslations().Size();
		pModel->oSkeletonHeader.m_iQKeyCount  = pTKLBuilder->GetRotations().Size();
		pModel->oSkeletonHeader.m_iSKeyCount  = pTKLBuilder->GetScales().Size();
		pModel->oSkeletonHeader.m_iTBaseIndex = 0;
		pModel->oSkeletonHeader.m_iQBaseIndex = 0;
		pModel->oSkeletonHeader.m_iSBaseIndex = 0;

		if ( bGlobalTKLBuilder )
			pModel->pKeyLib = Resource::StreamedKeyLib_FindOrCreateDummy( TPS8D( pTKLBuilder->GetName() ) );
		else
			pModel->pKeyLib = Resource::StreamedKeyLib_Create( TPS8D( pModel->oSkeletonHeader.m_szTKLName ), *pTKLBuilder );

		Model_PrepareAnimations( pModel );
	}

	// Create main LOD object
	// TODO: support multiple LODs
	const TINT iLODIdx = 0;

	// Calculate actual number of meshes
	// To make it clear, we count meshes only when materials differ, but Skinned models can have submeshes to fit all bones
	TINT iNumMaterials = 0;
	T2DynamicVector<TSIZE> vecActualMeshes;

	T2Map<TINT, TINT> mapGltfMatToTRVMat;
	for ( TSIZE i = 0; i < gltfModel.meshes.size(); i++ )
	{
		auto& gltfMesh = gltfModel.meshes[i];

		TASSERT( gltfMesh.primitives.size() == 1 );

		const TINT iGltfMatIdx = gltfMesh.primitives[ 0 ].material;
		if ( mapGltfMatToTRVMat.Find( iGltfMatIdx ) == mapGltfMatToTRVMat.End() )
		{
			mapGltfMatToTRVMat.Insert( iGltfMatIdx, iNumMaterials );

			iNumMaterials += 1;
			vecActualMeshes.PushBack( i );
		}
	}

	// Setup the LOD
	pModel->aLODs[ iLODIdx ].iNumMeshes     = TINT( vecActualMeshes.Size() );
	pModel->aLODs[ iLODIdx ].ppMeshes       = new TMesh*[ vecActualMeshes.Size() ];
	pModel->aLODs[ iLODIdx ].BoundingSphere = TSphere( 0.0f, 0.0f, 0.0f, 50.0f ); // TODO: CALCULATE IT

	// Process and setup meshes and submeshes
	for ( TINT i = 0; i < vecActualMeshes.Size(); i++ )
	{
		auto  iGltfMeshIdx    = vecActualMeshes[ i ];
		auto& gltfMesh        = gltfModel.meshes[ iGltfMeshIdx ];
		auto& gltfPrimitive   = gltfMesh.primitives[ 0 ];
		auto  gltfMaterialIdx = gltfPrimitive.material;
		auto& gltfMaterial    = gltfModel.materials[ gltfMaterialIdx ];
		
		TASSERT( gltfMesh.primitives.size() == 1 );

		T2FormatString128 symbolName;
		symbolName.Format( "LOD%d_Mesh_%d", iLODIdx, i );

		SkinMesh*     pMesh          = g_pSkinShader->CreateMesh();
		SkinMaterial* pMaterial      = g_pSkinShader->CreateMaterial();

		// Find all submeshes
		T2DynamicVector<TSIZE> vecSubMeshes;
		for ( TSIZE k = 0; k < gltfModel.meshes.size(); k++ )
		{
			if ( gltfModel.meshes[ k ].primitives[ 0 ].material == gltfMaterialIdx )
				vecSubMeshes.PushBack( k );
		}

		// Get the texture
		const TCHAR* pchTextureName = "NoTexture";

		if ( gltfMaterial.pbrMetallicRoughness.baseColorTexture.index >= 0 )
			pchTextureName = gltfModel.images[ gltfModel.textures[ gltfMaterial.pbrMetallicRoughness.baseColorTexture.index ].source ].uri.c_str();

 		auto pTexture = Resource::StreamedTexture_FindOrCreateDummy( TPS8D( pchTextureName ) );

		pMaterial->SetTexture( pTexture );
		pMaterial->SetName( gltfMaterial.name.c_str() );
		pModel->vecUsedTextures.PushBack( pTexture );

		pMesh->SetName( symbolName.Get() );
		pMesh->SetMaterialName( gltfMaterial.name.c_str() );
		pMesh->SetMaterial( pMaterial );
		pMesh->vecSubMeshes.Reserve( vecSubMeshes.Size() );

		pModel->aLODs[ iLODIdx ].ppMeshes[ i ] = pMesh;

		// Prepare vertex buffer
		T2DynamicVector<SkinMesh::SkinVertex> vecVertices;
		pMesh->oVertexBuffer = T2Render::CreateVertexBuffer( TNULL, 0, GL_STATIC_DRAW );

		tinygltf::Skin* pGLTFSkin = ( bHasSkins ) ? &gltfModel.skins[ 0 ] : TNULL;

		// Handle submeshes
		for ( TINT k = 0; k < vecSubMeshes.Size(); k++ )
		{
			auto& gltfSubMesh          = gltfModel.meshes[ vecSubMeshes[ k ] ];
			auto& gltfSubMeshPrimitive = gltfSubMesh.primitives[ 0 ];
			auto  pSubMesh             = &pMesh->vecSubMeshes.PushBack();

			TASSERT( gltfSubMesh.primitives.size() == 1 );

			TASSERT( gltfSubMeshPrimitive.attributes.contains( "POSITION" ) );
			TASSERT( gltfSubMeshPrimitive.attributes.contains( "NORMAL" ) );
			TASSERT( gltfSubMeshPrimitive.attributes.contains( "WEIGHTS_0" ) );
			TASSERT( gltfSubMeshPrimitive.attributes.contains( "JOINTS_0" ) );
			TASSERT( gltfSubMeshPrimitive.attributes.contains( "TEXCOORD_0" ) );

			const TINT iAccIndicesIndex  = gltfSubMeshPrimitive.indices;
			const TINT iAccPositionIndex = gltfSubMeshPrimitive.attributes[ "POSITION" ];
			const TINT iAccNormalIndex   = gltfSubMeshPrimitive.attributes[ "NORMAL" ];
			const TINT iAccWeightsIndex  = gltfSubMeshPrimitive.attributes[ "WEIGHTS_0" ];
			const TINT iAccJointsIndex   = gltfSubMeshPrimitive.attributes[ "JOINTS_0" ];
			const TINT iAccUVIndex       = gltfSubMeshPrimitive.attributes[ "TEXCOORD_0" ];

			auto& gltfIndexAccessor    = gltfModel.accessors[ iAccIndicesIndex ];
			auto& gltfPositionAccessor = gltfModel.accessors[ iAccPositionIndex ];
			auto& gltfNormalAccessor   = gltfModel.accessors[ iAccNormalIndex ];
			auto& gltfWeightsAccessor  = gltfModel.accessors[ iAccWeightsIndex ];
			auto& gltfJointsAccessor   = gltfModel.accessors[ iAccJointsIndex ];
			auto& gltfUVAccessor       = gltfModel.accessors[ iAccUVIndex ];

			auto& gltfIndexBufferView    = gltfModel.bufferViews[ gltfIndexAccessor.bufferView ];
			auto& gltfPositionBufferView = gltfModel.bufferViews[ gltfPositionAccessor.bufferView ];
			auto& gltfNormalBufferView   = gltfModel.bufferViews[ gltfNormalAccessor.bufferView ];
			auto& gltfWeightsBufferView  = gltfModel.bufferViews[ gltfWeightsAccessor.bufferView ];
			auto& gltfJointsBufferView   = gltfModel.bufferViews[ gltfJointsAccessor.bufferView ];
			auto& gltfUVBufferView       = gltfModel.bufferViews[ gltfUVAccessor.bufferView ];

			auto& gltfIndexBuffer    = gltfModel.buffers[ gltfIndexBufferView.buffer ];
			auto& gltfPositionBuffer = gltfModel.buffers[ gltfPositionBufferView.buffer ];
			auto& gltfNormalBuffer   = gltfModel.buffers[ gltfNormalBufferView.buffer ];
			auto& gltfWeightsBuffer  = gltfModel.buffers[ gltfWeightsBufferView.buffer ];
			auto& gltfJointsBuffer   = gltfModel.buffers[ gltfJointsBufferView.buffer ];
			auto& gltfUVBuffer       = gltfModel.buffers[ gltfUVBufferView.buffer ];
			
			TASSERT( gltfIndexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT );
			TASSERT( gltfIndexAccessor.type == TINYGLTF_TYPE_SCALAR );

			const TUINT uiStartVertex = vecVertices.Size();

			// Setup vertices
			const TUINT uiNumVerticesOrig = gltfPositionAccessor.count;
			vecVertices.SetSize( uiStartVertex + uiNumVerticesOrig );

			TASSERT( gltfPositionAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT );
			TASSERT( gltfPositionAccessor.type == TINYGLTF_TYPE_VEC3 );
			auto pGltfDataPosition = gltfPositionBuffer.data.begin() + gltfPositionBufferView.byteOffset;
			const TUINT uiPositionBufferStride = ( gltfPositionBufferView.byteStride != 0 ) ? gltfPositionBufferView.byteStride : sizeof( TVector3 );

			TASSERT( gltfNormalAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT );
			TASSERT( gltfNormalAccessor.type == TINYGLTF_TYPE_VEC3 );
			auto pGltfDataNormal = gltfNormalBuffer.data.begin() + gltfNormalBufferView.byteOffset;
			const TUINT uiNormalBufferStride = ( gltfNormalBufferView.byteStride != 0 ) ? gltfNormalBufferView.byteStride : sizeof( TVector3 );

			TASSERT( gltfWeightsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT || gltfWeightsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE );
			TASSERT( gltfWeightsAccessor.type == TINYGLTF_TYPE_VEC4 );
			auto pGltfDataWeights = gltfWeightsBuffer.data.begin() + gltfWeightsBufferView.byteOffset;
			const TUINT uiWeightsBufferStride = ( gltfWeightsBufferView.byteStride != 0 ) ? gltfWeightsBufferView.byteStride : ( gltfWeightsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT ? sizeof( TVector4 ) : sizeof( TUINT32 ) );

			TASSERT( gltfJointsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE );
			TASSERT( gltfJointsAccessor.type == TINYGLTF_TYPE_VEC4 );
			auto pGltfDataJoints = gltfJointsBuffer.data.begin() + gltfJointsBufferView.byteOffset;
			const TUINT uiJointsBufferStride = ( gltfJointsBufferView.byteStride != 0 ) ? gltfJointsBufferView.byteStride : sizeof( TUINT32 );
			
			TASSERT( gltfUVAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT );
			TASSERT( gltfUVAccessor.type == TINYGLTF_TYPE_VEC2 );
			auto pGltfDataUV = gltfUVBuffer.data.begin() + gltfUVBufferView.byteOffset;
			const TUINT uiUVBufferStride = ( gltfUVBufferView.byteStride != 0 ) ? gltfUVBufferView.byteStride : sizeof( TVector2 );
			
			// Build vertices
			T2Map<TINT, TINT> mapUsedBones;
			for ( TUINT j = uiStartVertex, m = 0; j < uiStartVertex + uiNumVerticesOrig; j++, m++ )
			{
				vecVertices[ j ].Position = *(TVector3*)( &*( pGltfDataPosition + ( uiPositionBufferStride * m ) + gltfPositionAccessor.byteOffset ) );
				vecVertices[ j ].Normal   = *(TVector3*)( &*( pGltfDataNormal + ( uiNormalBufferStride * m ) + gltfNormalAccessor.byteOffset ) );
				vecVertices[ j ].UV       = *(TVector2*)( &*( pGltfDataUV + ( uiUVBufferStride * m ) + gltfUVAccessor.byteOffset ) );

				TUINT8* pJoints = (TUINT8*)( &*( pGltfDataJoints + ( uiJointsBufferStride * m ) + gltfJointsAccessor.byteOffset ) );

				TINT iBoneIndex1 = pGLTFSkin ? mapGltfBoneToTRVBone[ pGLTFSkin->joints[ pJoints[ 0 ] ] ]->second : 0;
				TINT iBoneIndex2 = pGLTFSkin ? mapGltfBoneToTRVBone[ pGLTFSkin->joints[ pJoints[ 1 ] ] ]->second : 0;
				TINT iBoneIndex3 = pGLTFSkin ? mapGltfBoneToTRVBone[ pGLTFSkin->joints[ pJoints[ 2 ] ] ]->second : 0;
				TINT iBoneIndex4 = pGLTFSkin ? mapGltfBoneToTRVBone[ pGLTFSkin->joints[ pJoints[ 3 ] ] ]->second : 0;

				// Read weights
				TFLOAT flWeight1, flWeight2, flWeight3, flWeight4;
				if ( gltfWeightsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT )
				{
					// Read weights as floats
					TFLOAT* pWeights = (TFLOAT*)( &*( pGltfDataWeights + ( uiWeightsBufferStride * m ) + gltfWeightsAccessor.byteOffset ) );

					flWeight1 = pWeights[ 0 ];
					flWeight2 = pWeights[ 1 ];
					flWeight3 = pWeights[ 2 ];
					flWeight4 = pWeights[ 3 ];
				}
				else if ( gltfWeightsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE )
				{
					// Read weights as TUINT8 bytes
					TUINT8* pWeights = (TUINT8*)( &*( pGltfDataWeights + ( uiWeightsBufferStride * m ) + gltfWeightsAccessor.byteOffset ) );

					flWeight1 = pWeights[ 0 ] / 255.0f;
					flWeight2 = pWeights[ 1 ] / 255.0f;
					flWeight3 = pWeights[ 2 ] / 255.0f;
					flWeight4 = pWeights[ 3 ] / 255.0f;
				}

				const TBOOL bHasSpaceForBones = mapUsedBones.Size() < SKINNED_SUBMESH_MAX_BONES;
				const TBOOL bBoneAnimated1    = flWeight1 >= ( 1.0f / 255.0f ) && pJoints[ 0 ];
				const TBOOL bBoneAnimated2    = flWeight2 >= ( 1.0f / 255.0f ) && pJoints[ 1 ];
				const TBOOL bBoneAnimated3    = flWeight3 >= ( 1.0f / 255.0f ) && pJoints[ 2 ];
				const TBOOL bBoneAnimated4    = flWeight4 >= ( 1.0f / 255.0f ) && pJoints[ 3 ];

				if ( bHasSpaceForBones && bBoneAnimated1 && mapUsedBones.Find( iBoneIndex1 ) == mapUsedBones.End() )
				{
					pSubMesh->aBones[ mapUsedBones.Size() ] = iBoneIndex1;
					mapUsedBones.Insert( iBoneIndex1, mapUsedBones.Size() );
				}

				if ( bHasSpaceForBones && bBoneAnimated2 && mapUsedBones.Find( iBoneIndex2 ) == mapUsedBones.End() )
				{
					pSubMesh->aBones[ mapUsedBones.Size() ] = iBoneIndex2;
					mapUsedBones.Insert( iBoneIndex2, mapUsedBones.Size() );
				}

				if ( bHasSpaceForBones && bBoneAnimated3 && mapUsedBones.Find( iBoneIndex3 ) == mapUsedBones.End() )
				{
					pSubMesh->aBones[ mapUsedBones.Size() ] = iBoneIndex3;
					mapUsedBones.Insert( iBoneIndex3, mapUsedBones.Size() );
				}

				if ( bHasSpaceForBones && bBoneAnimated4 && mapUsedBones.Find( iBoneIndex4 ) == mapUsedBones.End() )
				{
					pSubMesh->aBones[ mapUsedBones.Size() ] = iBoneIndex4;
					mapUsedBones.Insert( iBoneIndex4, mapUsedBones.Size() );
				}

				vecVertices[ j ].Bones[ 0 ] = bBoneAnimated1 ? ( mapUsedBones[ iBoneIndex1 ]->second * 3 ) : 0;
				vecVertices[ j ].Bones[ 1 ] = bBoneAnimated2 ? ( mapUsedBones[ iBoneIndex2 ]->second * 3 ) : 0;
				vecVertices[ j ].Bones[ 2 ] = bBoneAnimated3 ? ( mapUsedBones[ iBoneIndex3 ]->second * 3 ) : 0;
				vecVertices[ j ].Bones[ 3 ] = bBoneAnimated4 ? ( mapUsedBones[ iBoneIndex4 ]->second * 3 ) : 0;

				vecVertices[ j ].Weights[ 0 ] = bBoneAnimated1 ? TUINT8( flWeight1 * 255.0f ) : 0;
				vecVertices[ j ].Weights[ 1 ] = bBoneAnimated2 ? TUINT8( flWeight2 * 255.0f ) : 0;
				vecVertices[ j ].Weights[ 2 ] = bBoneAnimated3 ? TUINT8( flWeight3 * 255.0f ) : 0;
				vecVertices[ j ].Weights[ 3 ] = bBoneAnimated4 ? TUINT8( flWeight4 * 255.0f ) : 0;
			}

			TASSERT( mapUsedBones.Size() <= SKINNED_SUBMESH_MAX_BONES && "Too many bones per mesh (> 28) - split the mesh in your editing tool!!!" );

			T2VertexArray::Unbind();

			// Setup submesh data
			// We'll create Vertex Buffer later, because all submeshes use a shared one and we need to store all vertices in it
			const TUINT              uiNumIndicesOrig = gltfIndexAccessor.count;
			T2DynamicVector<TUINT16> vecIndices;
			vecIndices.SetSize( uiNumIndicesOrig );

			auto        pGltfDataIndex      = gltfIndexBuffer.data.begin() + gltfIndexBufferView.byteOffset;
			const TUINT uiIndexBufferStride = ( gltfIndexBufferView.byteStride != 0 ) ? gltfIndexBufferView.byteStride : sizeof( TUINT16 );

			// Copy indices
			for ( TUINT j = 0; j < uiNumIndicesOrig; j++ )
				vecIndices[ j ] = *(TUINT16*)( &*( pGltfDataIndex + ( uiIndexBufferStride * j ) + gltfIndexAccessor.byteOffset ) ) + uiStartVertex;

			PrimitiveGroup* pPrims       = NULL;
			TUINT16*        pIndices     = vecIndices.Begin();
			TUINT           uiNumIndices = vecIndices.Size();

			if ( gltfPrimitive.mode != TINYGLTF_MODE_TRIANGLE_STRIP )
			{
				//SetListsOnly( TTRUE );

				// Generate triangle strips
				TUINT16         iNumPrims;
				TBOOL           bResult = GenerateStrips( &vecIndices[ 0 ], vecIndices.Size(), &pPrims, &iNumPrims );
				TASSERT( bResult == TTRUE );
				TASSERT( iNumPrims == 1 );
				vecIndices.FreeMemory();

				pIndices     = pPrims->indices;
				uiNumIndices = pPrims->numIndices;
			}

			pSubMesh->uiNumIndices      = uiNumIndices;
			pSubMesh->oIndexBuffer      = T2Render::CreateIndexBuffer( pIndices, uiNumIndices, GL_STATIC_DRAW );
			pSubMesh->uiEndVertexId     = uiStartVertex + uiNumVerticesOrig;
			pSubMesh->uiNumBones        = mapUsedBones.Size();

			// Clean NvTriStrip's object
			if ( pPrims )
			{
				delete[] pPrims;
				pPrims = TNULL;
			}

			pSubMesh->oVertexArray = T2Render::CreateVertexArray( pMesh->oVertexBuffer, pSubMesh->oIndexBuffer );
			pSubMesh->oVertexArray.Bind();
			pSubMesh->oVertexArray.GetVertexBuffer().SetAttribPointer( 0, 3, GL_FLOAT, sizeof( SkinMesh::SkinVertex ), offsetof( SkinMesh::SkinVertex, Position ) );
			pSubMesh->oVertexArray.GetVertexBuffer().SetAttribPointer( 1, 3, GL_FLOAT, sizeof( SkinMesh::SkinVertex ), offsetof( SkinMesh::SkinVertex, Normal ) );
			pSubMesh->oVertexArray.GetVertexBuffer().SetAttribPointer( 2, 4, GL_UNSIGNED_BYTE, sizeof( SkinMesh::SkinVertex ), offsetof( SkinMesh::SkinVertex, Weights ), GL_TRUE );
			pSubMesh->oVertexArray.GetVertexBuffer().SetAttribPointer( 3, 4, GL_UNSIGNED_BYTE, sizeof( SkinMesh::SkinVertex ), offsetof( SkinMesh::SkinVertex, Bones ), GL_TRUE );
			pSubMesh->oVertexArray.GetVertexBuffer().SetAttribPointer( 4, 2, GL_FLOAT, sizeof( SkinMesh::SkinVertex ), offsetof( SkinMesh::SkinVertex, UV ) );
		}
		
		TASSERT( pMesh->vecSubMeshes.Size() > 0 );
		auto pHeadSubMesh = pMesh->vecSubMeshes.Front();
		
		// Send vertex data to the GPU
		pMesh->oVertexBuffer.SetData( vecVertices.Begin(), vecVertices.Size() * sizeof( SkinMesh::SkinVertex ), GL_STATIC_DRAW );
		pHeadSubMesh->uiNumAllocatedVertices = vecVertices.Size();
	}

	return pModel;
}

TBOOL ResourceLoader::Model_PrepareAnimations( Model* pModel )
{
	if ( !pModel->bAnimationsLoaded && pModel->pKeyLib && pModel->pKeyLib->IsLoaded() )
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

		pModel->bAnimationsLoaded = TTRUE;
	}

	return pModel->bAnimationsLoaded;
}

TBOOL ResourceLoader::Model_CreateInstance( Toshi::T2SharedPtr<Model> pModel, ModelInstance& rOutInstance )
{
	rOutInstance.pModel = pModel;
	rOutInstance.oTransform.SetMatrix( TMatrix44::IDENTITY );
	rOutInstance.pSkeletonInstance = ( pModel->pSkeleton ) ? pModel->pSkeleton->CreateInstance( TTRUE ) : TNULL;

	return TTRUE;
}

void ResourceLoader::ModelLoader_SetTKLBuilder( TKLBuilder* pTKLBuilder )
{
	s_pTKLBuilder = pTKLBuilder;
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
