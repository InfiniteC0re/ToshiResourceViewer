#include "pch.h"
#include "ModelResourceView.h"
#include "Shader/Mesh.h"
#include "Shader/RendererSettings.h"
#include "Application.h"
#include "Serializer/CollisionTreeBuilder.h"

#include <Render/TTMDWin.h>
#include <Render/T2Render.h>
#include <Toshi/T2Map.h>

#include <Platform/GL/T2FrameBuffer_GL.h>
#include <assimp/Exporter.hpp>
#include <assimp/scene.h>
#include <imgui_internal.h>

#include "tiny_gltf.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

ModelResourceView::ModelResourceView()
    : m_vecCameraCenter( TVector4::VEC_ZERO )
    , m_fCameraDistance( 2.0f )
    , m_fCameraDistanceTarget( 2.0f )
    , m_fCameraFOV( 90.0f )
    , m_fCameraRotX( 0.0f )
    , m_fCameraRotY( 0.0f )
    , m_iSelectedSequence( -1 )
    , m_bAutoSaveTKL( TFALSE )
    , m_bWireFrame( TFALSE )
    , m_bDockingSetUp( TFALSE )
    , m_flWireframeThickness( 1.0f )
    , m_bDisableTextures( TFALSE )
    , m_vViewportColor( 0.18f, 0.185f, 0.20f, 1.0f )
{
	m_ViewportFrameBuffer.Create();
	m_ViewportFrameBuffer.CreateDepthTexture( 1920, 1080 );
	m_ViewportFrameBuffer.CreateAttachment( 0, 1920, 1080, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE );

	m_oCamera.SetNearPlane( 0.1f );
}

ModelResourceView::~ModelResourceView()
{
}

TBOOL ModelResourceView::OnCreate( Toshi::T2StringView pchFilePath )
{
	// Create unique IDs
	m_strDockspaceId.Format( "##Dockspace%u", GetImGuiID() );
	m_strSequencesId.Format( "Sequences##Sequences%u", GetImGuiID() );
	m_strViewportId.Format( "Viewport##Viewport%u", GetImGuiID() );
	m_strPreferencesId.Format( "Scene##Preferences%u", GetImGuiID() );

	if ( m_bIsExternal )
	{
		const TString8 strPath = pchFilePath.Get();

		if ( strPath.EndsWithNoCase( ".xml" ) )
		{
			// Create from the model's XML (applies its own animation/bone filters)
			LoadModelFromXML( strPath.GetString() );
		}
		else
		{
			// Create straight from a GLTF
			ResourceLoader::Model_CreateInstance( ResourceLoader::Model_LoadSkin_GLTF( pchFilePath ), m_ModelInstance );
		}
	}
	else
	{
		// Create from TRB
		const TBOOL bIsSkinModel  = ( m_strSymbolName == "FileHeader" );
		const TBOOL bIsWorldModel = ( m_strSymbolName == "Database" );

		// Skinned mesh
		if ( bIsSkinModel || bIsWorldModel )
		{
			if ( bIsSkinModel )
			{
				TTMDBase::FileHeader* pFileHeader = TSTATICCAST( TTMDBase::FileHeader, m_pData );

				if ( m_pTRB->ConvertEndianess( pFileHeader->m_uiMagic ) != TFourCC( "TMDL" ) )
					return TFALSE;
			}
			else if ( !m_pData )
			{
				return TFALSE;
			}

			Toshi::T2SharedPtr<ResourceLoader::Model> pModel;

			// Multiple threads is not supported anyways, so don't care about the global state
			switch ( g_oTheApp.GetSelectedPlatform() )
			{
				case TOSHISKU_PS2:
					pModel = ResourceLoader::Model_Load_Barnyard_PS2( m_pTRB );
					break;
				case TOSHISKU_WINDOWS:
					pModel = ResourceLoader::Model_Load_Barnyard_Windows( m_pTRB );
					break;
				default:
					TASSERT( !"Unsupported SKU of the model!!!" );
					break;
			}

			if ( pModel ) ResourceLoader::Model_CreateInstance( pModel, m_ModelInstance );
		}
	}

	// Update transform
	m_ModelInstance.oTransform.SetMatrix( TMatrix44::IDENTITY );
	m_ModelInstance.oTransform.SetEuler( TVector3( TMath::DegToRad( -90.0f ), 0.0f, 0.0f ) );
	m_ModelInstance.oTransform.SetTranslate( TVector3::VEC_ZERO );

	return TRBResourceView::OnCreate( pchFilePath ) && m_ModelInstance.pModel.IsValid() && ( TryFixingMissingTKL(), TTRUE );
}

void ModelResourceView::LoadModelFromXML( const TCHAR* pchXMLPath )
{
	tinyxml2::XMLDocument oXML;
	if ( oXML.LoadFile( pchXMLPath ) != tinyxml2::XML_SUCCESS ) return;

	auto pTMDL = oXML.FirstChildElement( "TMDL" );
	if ( !pTMDL ) return;

	// Resolve the source GLTF next to the XML (decompile writes both together, so
	// a moved pair still loads even though the stored Source path is absolute)
	const TString8 strXMLPath = pchXMLPath;
	const TINT     iXMLSlash  = TMath::Max( strXMLPath.FindReverse( '\\' ), strXMLPath.FindReverse( '/' ) );
	TString8       strDir;
	strDir.Copy( strXMLPath, iXMLSlash + 1 );

	const TCHAR*   pchSource   = pTMDL->Attribute( "Source" );
	const TString8 strSource   = pchSource ? pchSource : "";
	const TINT     iSrcSlash   = TMath::Max( strSource.FindReverse( '\\' ), strSource.FindReverse( '/' ) );
	const TString8 strSrcName  = ( iSrcSlash != -1 ) ? TString8( strSource.GetString( iSrcSlash + 1 ) ) : strSource;
	const TString8 strGltfPath = TString8::VarArgs( "%s%s", strDir.GetString(), strSrcName.GetString() );

	// Build the model's own animation and bone filters from the XML, mapping the
	// shared GLTF names (GltfName) back to this model's own sequence/bone names
	ResourceLoader::AnimationFilter oAnimFilter;
	ResourceLoader::BoneFilter      oBoneFilter;

	if ( auto pSkel = pTMDL->FirstChildElement( "TSkeleton" ) )
	{
		auto pSeqs = pSkel->FirstChildElement( "Sequences" );
		for ( auto pSeq = pSeqs ? pSeqs->FirstChildElement( "Sequence" ) : TNULL; pSeq; pSeq = pSeq->NextSiblingElement( "Sequence" ) )
		{
			const TCHAR* pchName = pSeq->Attribute( "Name" );
			if ( !pchName ) continue;

			const TCHAR* pchGltfName = pSeq->Attribute( "GltfName" );
			oAnimFilter.PushBack( { pchGltfName ? pchGltfName : pchName, pchName } );
		}

		auto pBones = pSkel->FirstChildElement( "Bones" );
		for ( auto pBone = pBones ? pBones->FirstChildElement( "Bone" ) : TNULL; pBone; pBone = pBone->NextSiblingElement( "Bone" ) )
		{
			const TCHAR* pchName = pBone->Attribute( "Name" );
			if ( !pchName ) continue;

			const TCHAR* pchGltfName = pBone->Attribute( "GltfName" );
			oBoneFilter.PushBack( { pchGltfName ? pchGltfName : pchName, pchName } );
		}
	}

	ResourceLoader::ModelLoader_SetAnimationFilter( oAnimFilter.IsEmpty() ? TNULL : &oAnimFilter );
	ResourceLoader::ModelLoader_SetBoneFilter( oBoneFilter.IsEmpty() ? TNULL : &oBoneFilter );

	ResourceLoader::Model_CreateInstance( ResourceLoader::Model_LoadSkin_GLTF( strGltfPath.GetString() ), m_ModelInstance );

	ResourceLoader::ModelLoader_SetAnimationFilter( TNULL );
	ResourceLoader::ModelLoader_SetBoneFilter( TNULL );

	DeserializeModelInformation( &oXML );
}

TBOOL ModelResourceView::CanSave()
{
	return TTRUE;
}

TBOOL ModelResourceView::OnSave( PTRB* pOutTRB )
{
	// TODO: support World mesh type
	const TBOOL bIsSkinnedMesh = TTRUE;
	if ( !bIsSkinnedMesh ) return TFALSE;

	auto pKeyLib = m_ModelInstance.pModel->pKeyLib;

	if ( m_bAutoSaveTKL )
	{
		// Save TKL
		PTRB* pTKLTRB    = new PTRB( pOutTRB->GetEndianess() );
		auto  pMemStream = pTKLTRB->GetSections()->CreateStream();

		OnSaveTKL( pTKLTRB );

		pTKLTRB->WriteToFile( TString8::VarArgs( "%s.tkl", pKeyLib->GetTRBHeader()->m_szName ).GetString(), TFALSE );
	}

	PTRBSections* pSECT = pOutTRB->GetSections();
	PTRBSymbols*  pSYMB = pOutTRB->GetSymbols();

	PTRBSections::MemoryStream* pMemStream = pSECT->GetStack( 0 );

	// Allocate FileHeader symbol
	auto pTRBFileHeader              = pMemStream->Alloc<TTMDBase::FileHeader>();
	pTRBFileHeader->m_uiMagic        = pOutTRB->ConvertEndianess( TFourCC( "TMDL" ) );
	pTRBFileHeader->m_uiZero1        = pOutTRB->ConvertEndianess( 0 );
	pTRBFileHeader->m_uiVersionMajor = pOutTRB->ConvertEndianess( TTMD_VERSION_MAJOR );
	pTRBFileHeader->m_uiVersionMinor = pOutTRB->ConvertEndianess( TTMD_VERSION_MINOR );
	pTRBFileHeader->m_uiZero2        = pOutTRB->ConvertEndianess( 0 );
	pSYMB->Add( pMemStream, "FileHeader", pTRBFileHeader.get() );

	// Allocate SkeletonHeader symbol
	auto        pTRBSkeletonHeader = pMemStream->Alloc<TTMDBase::SkeletonHeader>();
	const auto& rSkeletonHeader    = m_ModelInstance.pModel->oSkeletonHeader;
	T2String8::Copy( pTRBSkeletonHeader->m_szTKLName, rSkeletonHeader.m_szTKLName, sizeof( pTRBSkeletonHeader->m_szTKLName ) - 1 );
	pTRBSkeletonHeader->m_iTKeyCount  = pOutTRB->ConvertEndianess( rSkeletonHeader.m_iTKeyCount );
	pTRBSkeletonHeader->m_iQKeyCount  = pOutTRB->ConvertEndianess( rSkeletonHeader.m_iQKeyCount );
	pTRBSkeletonHeader->m_iSKeyCount  = pOutTRB->ConvertEndianess( rSkeletonHeader.m_iSKeyCount );
	pTRBSkeletonHeader->m_iTBaseIndex = pOutTRB->ConvertEndianess( rSkeletonHeader.m_iTBaseIndex );
	pTRBSkeletonHeader->m_iQBaseIndex = pOutTRB->ConvertEndianess( rSkeletonHeader.m_iQBaseIndex );
	pTRBSkeletonHeader->m_iSBaseIndex = pOutTRB->ConvertEndianess( rSkeletonHeader.m_iSBaseIndex );
	pSYMB->Add( pMemStream, "SkeletonHeader", pTRBSkeletonHeader.get() );

	TSkeletonInstance* pSkeletonInstance = m_ModelInstance.pSkeletonInstance;
	TSkeleton*         pSkeleton         = pSkeletonInstance->GetSkeleton();

	// Allocate Skeleton symbol
	const TINT iNumBones = pSkeleton->m_iBoneCount;
	const TINT iNumSeq   = pSkeleton->m_iSequenceCount;

	auto pTRBSkeleton                  = pMemStream->Alloc<TSkeleton>();
	pTRBSkeleton->m_iBoneCount         = pOutTRB->ConvertEndianess( iNumBones );
	pTRBSkeleton->m_iManualBoneCount   = pOutTRB->ConvertEndianess( pSkeleton->m_iManualBoneCount );
	pTRBSkeleton->m_iSequenceCount     = pOutTRB->ConvertEndianess( iNumSeq );
	pTRBSkeleton->m_iAnimationMaxCount = pOutTRB->ConvertEndianess( pSkeleton->m_iAnimationMaxCount );
	pTRBSkeleton->m_iInstanceCount     = pOutTRB->ConvertEndianess( 0 );
	pTRBSkeleton->m_eQuatLerpType      = pOutTRB->ConvertEndianess( TSkeleton::QUATINTERP_Default );

	// Copy info about the bones
	pMemStream->Alloc<TSkeletonBone>( &pTRBSkeleton->m_pBones, iNumBones );
	for ( TINT i = 0; i < iNumBones; i++ )
		TUtil::MemCopy( &pTRBSkeleton->m_pBones[ i ], &pSkeleton->m_pBones[ i ], sizeof( TSkeletonBone ) );

	// Copy info about the sequences
	auto pTRBSkeletonSeq = pMemStream->Alloc<TSkeletonSequence>( &pTRBSkeleton->m_SkeletonSequences, iNumSeq );
	for ( TINT i = 0; i < iNumSeq; i++ )
	{
		auto pSeq    = pSkeleton->GetSequence( i );
		auto pTRBSeq = pTRBSkeletonSeq + i;

		pTRBSeq->m_iNameLength = pOutTRB->ConvertEndianess( pSeq->m_iNameLength );
		T2String8::Copy( pTRBSeq->m_szName, pSeq->m_szName, sizeof( pTRBSeq->m_szName ) - 1 );

		pTRBSeq->m_eFlags        = pOutTRB->ConvertEndianess( pSeq->m_eFlags );
		pTRBSeq->m_eMode         = pOutTRB->ConvertEndianess( pSeq->m_eMode );
		pTRBSeq->m_iNumUsedBones = pOutTRB->ConvertEndianess( pSeq->m_iNumUsedBones );
		pTRBSeq->m_fDuration     = pOutTRB->ConvertEndianess( pSeq->m_fDuration );

		// Now, copy all animated bones of this sequence
		auto pTRBSeqBones = pMemStream->Alloc<TSkeletonSequenceBone>( &pTRBSeq->m_pSeqBones, iNumBones );
		for ( TINT k = 0; k < iNumBones; k++ )
		{
			auto pSeqBone    = &pSeq->m_pSeqBones[ k ];
			auto pTRBSeqBone = pTRBSeqBones + k;

			const TINT iKeySize = pSeqBone->m_iKeySize;
			const TINT iNumKeys = pSeqBone->m_iNumKeys;

			TASSERT( iKeySize == 4 || iKeySize == 6 ); // time, quaternion (+ translation sometimes)
			pTRBSeqBone->m_eFlags   = pOutTRB->ConvertEndianess( pSeqBone->m_eFlags );
			pTRBSeqBone->m_iKeySize = pOutTRB->ConvertEndianess( iKeySize );
			pTRBSeqBone->m_iNumKeys = pOutTRB->ConvertEndianess( iNumKeys );

			// Copy keyframe data
			pMemStream->Alloc<TBYTE>( &pTRBSeqBone->m_pData, iKeySize * iNumKeys );

			for ( TINT j = 0; j < iNumKeys; j++ )
			{
				TUINT16* pKeyData    = pSeqBone->GetKey( j );
				TUINT16* pTRBKeyData = pTRBSeqBone->GetKey( j );

				pTRBKeyData[ 0 ] = pOutTRB->ConvertEndianess( pKeyData[ 0 ] );
				pTRBKeyData[ 1 ] = pOutTRB->ConvertEndianess( pKeyData[ 1 ] );
				if ( iKeySize == 6 ) pTRBKeyData[ 2 ] = pOutTRB->ConvertEndianess( pKeyData[ 2 ] );
			}
		}
	}

	pSYMB->Add( pMemStream, "Skeleton", pTRBSkeleton.get() );

	// Allocate Materials symbol
	T2Map<TPString8, TString8, TPString8::Comparator> mapMaterials;
	auto                                              pTRBMaterialsHeader = pMemStream->Alloc<TTMDBase::MaterialsHeader>();

	T2SharedPtr<ResourceLoader::Model> pModel = m_ModelInstance.pModel;

	// Find all materials
	TINT iNumMaterials = 0;
	for ( TINT k = 0; k < pModel->iLODCount; k++ )
	{
		TString8 strMaterialName;
		TString8 strTextureName;

		Toshi::TModelLOD* pLOD = &pModel->aLODs[ k ];
		for ( TINT i = 0; i < pLOD->iNumMeshes; i++ )
		{
			Mesh* pMesh = TSTATICCAST( Mesh, pLOD->ppMeshes[ i ] );
			pMesh->GetMaterialInfo( strMaterialName, strTextureName );

			// Prefer the texture path from the XML when the model is compiled from one
			auto itOverride = m_mapXMLTextureOverrides.Find( TPS8D( strMaterialName ) );
			if ( itOverride != m_mapXMLTextureOverrides.End() )
				strTextureName = itOverride->second;

			if ( mapMaterials.Find( TPS8D( strMaterialName ) ) == mapMaterials.End() )
			{
				// It's the first time we encounter this material
				iNumMaterials += 1;
				mapMaterials.Insert( TPS8D( strMaterialName ), strTextureName );
			}
		}
	}

	// Write all materials
	pTRBMaterialsHeader->iNumMaterials = pOutTRB->ConvertEndianess( iNumMaterials );
	pTRBMaterialsHeader->uiSectionSize = pOutTRB->ConvertEndianess( sizeof( TTMDBase::Material ) * iNumMaterials );

	auto pTRBMaterials   = pMemStream->Alloc<TTMDBase::Material>( iNumMaterials );
	TINT iNumWrittenMats = 0;
	T2_FOREACH( mapMaterials, it )
	{
		auto pTRBMaterial = pTRBMaterials + iNumWrittenMats;

		T2String8::Copy( pTRBMaterial->szMatName, it->first.GetString(), sizeof( pTRBMaterial->szMatName ) - 1 );
		T2String8::Copy( pTRBMaterial->szTextureFile, it->second.GetString(), sizeof( pTRBMaterial->szTextureFile ) - 1 );

		iNumWrittenMats += 1;
	}

	pSYMB->Add( pMemStream, "Materials", pTRBMaterialsHeader.get() );

	// Write collision
	auto pTRBCollision          = pMemStream->Alloc<TTMDBase::CollisionHeader>();
	pTRBCollision->m_iNumMeshes = pOutTRB->ConvertEndianess( pModel->iNumCollisionMeshes );

	if ( pModel->iNumCollisionMeshes > 0 )
	{
		auto pTRBCollisionMeshes = pMemStream->Alloc<TTMDBase::CollisionMesh>( &pTRBCollision->m_pMeshes, pModel->iNumCollisionMeshes );

		for ( TINT i = 0; i < pModel->iNumCollisionMeshes; i++ )
		{
			auto& rCollisionMesh = pModel->pCollisionMeshes[ i ];
			auto  pTRBMesh       = pTRBCollisionMeshes + i;

			const TUINT uiNumVertices = rCollisionMesh.vecVertices.Size();
			const TUINT uiNumIndices  = rCollisionMesh.vecIndices.Size();
			const TUINT uiNumGroups   = rCollisionMesh.vecGroups.Size();

			pTRBMesh->m_iBoneID        = pOutTRB->ConvertEndianess( rCollisionMesh.iBoneID );
			pTRBMesh->m_uiNumVertices  = pOutTRB->ConvertEndianess( uiNumVertices );
			pTRBMesh->m_uiNumIndices   = pOutTRB->ConvertEndianess( uiNumIndices );
			pTRBMesh->m_uiNumCollTypes = pOutTRB->ConvertEndianess( uiNumGroups );

			auto pTRBVertices = pMemStream->Alloc<TVector3>( &pTRBMesh->m_pVertices, uiNumVertices );
			for ( TUINT k = 0; k < uiNumVertices; k++ )
			{
				auto pTRBVertex = pTRBVertices + k;
				pTRBVertex->x   = pOutTRB->ConvertEndianess( rCollisionMesh.vecVertices[ k ].x );
				pTRBVertex->y   = pOutTRB->ConvertEndianess( rCollisionMesh.vecVertices[ k ].y );
				pTRBVertex->z   = pOutTRB->ConvertEndianess( rCollisionMesh.vecVertices[ k ].z );
			}

			auto pTRBIndices = pMemStream->Alloc<TUINT16>( &pTRBMesh->m_pIndices, uiNumIndices );
			for ( TUINT k = 0; k < uiNumIndices; k++ )
			{
				*( pTRBIndices + k ) = pOutTRB->ConvertEndianess( rCollisionMesh.vecIndices[ k ] );
			}

			auto pTRBGroups = pMemStream->Alloc<TTMDBase::CollisionGroup>( &pTRBMesh->m_pCollGroups, uiNumGroups );
			for ( TUINT k = 0; k < uiNumGroups; k++ )
			{
				auto& rCollisionGroup = rCollisionMesh.vecGroups[ k ];
				auto  pTRBGroup       = pTRBGroups + k;

				const TUINT uiNameLength = rCollisionGroup.strName.Length() + 1;
				auto        pTRBName     = pMemStream->AllocBytes( uiNameLength );
				T2String8::Copy( pTRBName.get(), rCollisionGroup.strName.GetString(), uiNameLength );
				pMemStream->WritePointer( const_cast<TCHAR**>( &pTRBGroup->pszName ), pTRBName );

				pTRBGroup->iUnk1      = pOutTRB->ConvertEndianess( 0 );
				pTRBGroup->iUnk3      = pOutTRB->ConvertEndianess( 0 );
				pTRBGroup->uiNumFaces = pOutTRB->ConvertEndianess( rCollisionGroup.uiNumFaces );
				pTRBGroup->iSomeCount = pOutTRB->ConvertEndianess( 0 );
				pTRBGroup->pS1        = TNULL;
			}
		}
	}
	else
	{
		pTRBCollision->m_pMeshes = TNULL;
	}

	pSYMB->Add( pMemStream, "Collision", pTRBCollision.get() );

	// Bake the collision tree - the engine expects it prebaked. Models only ever
	// have a single collision mesh
	if ( pModel->iNumCollisionMeshes > 0 )
	{
		auto& rMesh = pModel->pCollisionMeshes[ 0 ];
		if ( rMesh.vecVertices.Size() > 0 && rMesh.vecIndices.Size() >= 6 )
			CollisionTreeBuilder::WriteCollisionTree(
			    pOutTRB, pMemStream, pSYMB,
			    &rMesh.vecVertices[ 0 ], rMesh.vecVertices.Size(),
			    &rMesh.vecIndices[ 0 ], rMesh.vecIndices.Size()
			);
	}

	// Write the main TTMD header (Windows) and information about the LODs
	const TINT iNumLODs = pModel->iLODCount;

	auto pTRBWinHeader            = pMemStream->Alloc<TTMDWin::TRBWinHeader>();
	pTRBWinHeader->m_iNumLODs     = pOutTRB->ConvertEndianess( iNumLODs );
	pTRBWinHeader->m_fLODDistance = pOutTRB->ConvertEndianess( pModel->fRenderDistance );

	auto pTRBLODs = pMemStream->Alloc<TTMDWin::TRBLODHeader>( iNumLODs );
	for ( TINT i = 0; i < iNumLODs; i++ )
	{
		auto pLOD    = &pModel->aLODs[ i ];
		auto pTRBLOD = pTRBLODs + i;

		pTRBLOD->m_iMeshCount1 = pOutTRB->ConvertEndianess( pLOD->iNumMeshes );
		pTRBLOD->m_iMeshCount2 = pOutTRB->ConvertEndianess( 0 );
		pTRBLOD->m_eShader     = pOutTRB->ConvertEndianess( TTMDBase::SHADERTYPE_SKIN );
		pTRBLOD->m_RenderVolume.Set(
		    pOutTRB->ConvertEndianess( pLOD->BoundingSphere.AsVector4().x ),
		    pOutTRB->ConvertEndianess( pLOD->BoundingSphere.AsVector4().y ),
		    pOutTRB->ConvertEndianess( pLOD->BoundingSphere.AsVector4().z ),
		    pOutTRB->ConvertEndianess( pLOD->BoundingSphere.AsVector4().w )
		);
	}

	pSYMB->Add( pMemStream, "Header", pTRBWinHeader.get() );

	// Write all meshes
	for ( TINT i = 0; i < iNumLODs; i++ )
	{
		auto pLOD = &pModel->aLODs[ i ];

		const TINT iMeshCount = pLOD->iNumMeshes;
		for ( TINT k = 0; k < iMeshCount; k++ )
		{
			Mesh* pMesh       = TSTATICCAST( Mesh, pLOD->ppMeshes[ k ] );
			auto  pTRBLODMesh = pMemStream->Alloc<TTMDWin::TRBMeshLODHeader>();

			// Serialize TRB Mesh
			TBOOL bSerializeResult = pMesh->SerializeTRBMesh( pOutTRB, pTRBLODMesh );
			TASSERT( bSerializeResult == TTRUE );

			// Add mesh symbol
			char szSymbolName[ 24 ];
			TStringManager::String8Format( szSymbolName, sizeof( szSymbolName ), "LOD%d_Mesh_%d", i, k );
			pSYMB->Add( pMemStream, szSymbolName, pTRBLODMesh.get() );
		}
	}

	return TTRUE;
}

void ModelResourceView::OnDestroy()
{
}

void ModelResourceView::OnRender( TFLOAT flDeltaTime )
{
	const ImVec2  vInitialPos = ImGui::GetCursorPos();
	const ImGuiID dockSpaceID = ImGui::GetID( m_strDockspaceId.Get() );

	ImGuiWindowClass windowClass;
	windowClass.ClassId = GetImGuiID();

	// Create dockspace for the resource view window
	ImGui::SetNextWindowClass( &windowClass );
	ImGui::DockSpace( dockSpaceID );

	ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 12.0f, 12.0f ) );

	if ( !m_bDockingSetUp )
	{
		// Clear just in case
		ImGui::DockBuilderRemoveNode( dockSpaceID );

		// Create root node and set initial size
		ImGuiID dockRoot = ImGui::DockBuilderAddNode( dockSpaceID, ImGuiDockNodeFlags_DockSpace );
		ImGui::DockBuilderSetNodeSize( dockSpaceID, ImGui::GetWindowSize() );

		// Start splitting the UI
		m_DockLeft = ImGui::DockBuilderSplitNode( dockSpaceID, ImGuiDir_Left, 0.5f, TNULL, &m_DockRight );
		m_DockLeft = ImGui::DockBuilderSplitNode( m_DockLeft, ImGuiDir_Up, 0.6f, TNULL, &m_DockLeftBottom );

		// Finally, dock the windows
		ImGui::DockBuilderDockWindow( m_strSequencesId.Get(), m_DockLeft );
		ImGui::DockBuilderDockWindow( m_strPreferencesId.Get(), m_DockLeftBottom );
		ImGui::DockBuilderDockWindow( m_strViewportId.Get(), m_DockRight );

		ImGui::DockBuilderFinish( dockSpaceID );
		m_bDockingSetUp = TTRUE;
	}

	ImGui::SetNextWindowClass( &windowClass );
	ImGui::Begin( m_strSequencesId.Get() );
	{
		ImGui::PushStyleColor( ImGuiCol_FrameBg, ImVec4( 0, 0, 0, 0 ) );
		if ( ImGui::BeginListBox( "##AnimationList", ImVec2( -1, -1 ) ) )
		{
			TSkeleton* pSkeleton = m_ModelInstance.pModel->pSkeleton;

			if ( pSkeleton && pSkeleton->m_SkeletonSequences )
			{
				TSkeletonSequence* pSequences = pSkeleton->m_SkeletonSequences;

				TBOOL bNoneSelected = ( m_iSelectedSequence == -1 );
				if ( ImGui::Selectable( "Base Pose", &bNoneSelected ) )
				{
					m_ModelInstance.pSkeletonInstance->RemoveAllAnimations();
					m_iSelectedSequence = -1;
				}

				for ( TINT i = 0; i < pSkeleton->m_iSequenceCount; i++ )
				{
					TBOOL bSelected = ( m_iSelectedSequence == i );
					if ( ImGui::Selectable( pSequences[ i ].GetName(), &bSelected ) )
					{
						m_ModelInstance.pSkeletonInstance->RemoveAllAnimations();
						m_iSelectedSequence = i;
					}
				}
			}

			ImGui::EndListBox();
		}
		ImGui::PopStyleColor();

		ImGui::End();
	}

	//ImGui::SameLine();
	//ImVec2 vPreviewPos = ImGui::GetCursorPos();
	//ImGui::SetCursorPos( ImVec2( vPreviewPos.x, vInitialPos.y ) );
	//ImGui::Text( "Preview" );
	//ImGui::SameLine();

	//ImGui::SetCursorPos( ImVec2( ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize( "Export" ).x - ImGui::GetStyle().FramePadding.x * 2, vInitialPos.y ) );
	//if (ImGui::SmallButton("Export"))
	//{
	//	tinygltf::Model gltfModel;
	//	ExportScene( gltfModel );

	//	// Write to the file
	//	tinygltf::TinyGLTF gltfWriter;
	//	gltfWriter.WriteGltfSceneToFile( &gltfModel, "D:\\exported.gltf", TFALSE, TTRUE, TTRUE, TFALSE );
	//}

	ImGui::SetNextWindowClass( &windowClass );
	ImGui::Begin( m_strViewportId.Get() );
	{
		// Prepare camera
		TVector3& oCamTranslation = m_oCamera->GetTranslation();

		m_fCameraDistance = TMath::LERPClamped( m_fCameraDistance, m_fCameraDistanceTarget, TMath::Max( TMath::Abs( m_fCameraDistanceTarget - m_fCameraDistance ), 8.0f ) * flDeltaTime );
		m_oCamera.SetFOV( TMath::DegToRad( m_fCameraFOV ) );
		m_oCamera.SetFarPlane( 2000.0f );

		// Arcball camera behaviour
		TMatrix44 oCameraMatrix;
		oCameraMatrix.Identity();

		TFLOAT   fCoeff      = 1.0f - TMath::Abs( TMath::Sin( m_fCameraRotY ) );
		TVector4 vecPosition = TVector4( fCoeff * TMath::Sin( m_fCameraRotX ), TMath::Sin( m_fCameraRotY ), fCoeff * TMath::Cos( m_fCameraRotX ) );
		vecPosition.Normalise();
		vecPosition.Multiply( m_fCameraDistance );

		TVector4 vecDirection = TVector4::VEC_ZERO - vecPosition;
		vecDirection.Normalise();

		oCameraMatrix.SetTranslation( vecPosition + m_vecCameraCenter );
		oCameraMatrix.LookAtDirection( vecDirection, TVector4( 0.0f, 1.0f, 0.0f ) );

		m_oCamera->SetMatrix( oCameraMatrix );

		// Update render context
		//ImGui::SetCursorPos( vPreviewPos );
		ImVec2 vPreviewPos = ImGui::GetCursorPos();
		ImVec2 oRegion     = ImGui::GetContentRegionAvail();

		g_pRenderGL->SetRenderContext( m_oRenderContext );
		m_oRenderContext.ForceRefreshFeatures();

		m_oRenderContext.GetViewport().SetWidth( oRegion.x );
		m_oRenderContext.GetViewport().SetHeight( oRegion.y );

		m_oRenderContext.SetCamera( m_oCamera );
		m_oRenderContext.UpdateCamera();

		// Render scene
		{
			m_ViewportFrameBuffer.Bind();
			m_oRenderContext.GetViewport().SetClearColor( m_vViewportColor );
			m_oRenderContext.GetViewport().Begin();

			m_ModelInstance.oTransform.GetLocalMatrixImp( m_oRenderContext.GetModelMatrix() );

			if ( m_ModelInstance.pModel )
			{
				if ( m_ModelInstance.pSkeletonInstance && ResourceLoader::Model_PrepareAnimations( m_ModelInstance.pModel.Get() ) )
				{
					m_ModelInstance.pSkeletonInstance->UpdateState( TTRUE );

					if ( m_iSelectedSequence != -1 && !m_ModelInstance.pSkeletonInstance->IsAnyAnimationPlaying() )
						m_ModelInstance.pSkeletonInstance->AddAnimationFull( m_iSelectedSequence, 1.0f, 0.0f, 0.0f, TAnimation::Flags_Managed );

					m_ModelInstance.pSkeletonInstance->UpdateTime( flDeltaTime );

					if ( m_iSelectedSequence == -1 || m_ModelInstance.pSkeletonInstance->IsAnyAnimationPlaying() )
						m_ModelInstance.pSkeletonInstance->UpdateState( TTRUE );

					m_oRenderContext.SetSkeletonInstance( m_ModelInstance.pSkeletonInstance );
				}

				if ( !m_bWireFrame || m_flWireframeThickness >= 0.5f )
					m_ModelInstance.pModel->Render();
			}

			if ( m_bWireFrame )
			{
				glLineWidth( m_flWireframeThickness );
				glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
			}
			else
			{
				glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			}

			// Update rendering settings and flush order tables to get the model drawn
			const TBOOL bDisabledTexturesOld     = g_oRendererSettings.bDisableTextures;
			g_oRendererSettings.bDisableTextures = m_bDisableTextures;
			g_pRenderGL->FlushOrderTables();
			g_oRendererSettings.bDisableTextures = bDisabledTexturesOld;

			// Restore renderer state
			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

			m_oRenderContext.GetViewport().End();
			m_ViewportFrameBuffer.Unbind();
		}

		g_pRenderGL->SetDefaultRenderContext();

		// Render to the viewport
		ImGui::Image( m_ViewportFrameBuffer.GetAttachment( 0 ), ImVec2( oRegion.x, oRegion.y ), ImVec2( 0.0f, oRegion.y / 1080.0f ), ImVec2( oRegion.x / 1920.0f, 0.0f ) );

		// Control camera
		if ( ImGui::IsWindowHovered() )
		{
			m_fCameraDistanceTarget -= ImGui::GetIO().MouseWheel * 0.25f;
			TMath::Clip( m_fCameraDistanceTarget, 0.0f, 50.0f );

			static TBOOL s_bWasDragging = TFALSE;
			TBOOL        bIsDragging    = ImGui::IsMouseDown( ImGuiMouseButton_Right );

			if ( bIsDragging )
			{
				static ImVec2 s_vLastPos  = ImGui::GetMousePos();
				ImVec2        vCurrentPos = ImGui::GetMousePos();
				ImVec2        vDrag       = ImVec2( s_vLastPos.x - vCurrentPos.x, s_vLastPos.y - vCurrentPos.y );

				if ( s_bWasDragging )
				{
					if ( !ImGui::IsKeyDown( ImGuiKey_LeftShift ) )
					{
						TVector4 vecUpAxis    = oCameraMatrix.AsBasisVector4( BASISVECTOR_UP );
						TVector4 vecRightAxis = oCameraMatrix.AsBasisVector4( BASISVECTOR_RIGHT );
						vecUpAxis.Multiply( vDrag.y * 0.0025f );
						vecRightAxis.Multiply( vDrag.x * 0.0025f );

						m_vecCameraCenter.x += vecUpAxis.x + vecRightAxis.x;
						m_vecCameraCenter.y += vecUpAxis.y + vecRightAxis.y;
						m_vecCameraCenter.z += vecUpAxis.z + vecRightAxis.z;
					}
					else
					{
						m_fCameraRotX += vDrag.x * 0.005f;
						m_fCameraRotY -= vDrag.y * 0.0025f;

						TMath::Clip( m_fCameraRotY, -TMath::HALF_PI, TMath::HALF_PI );
					}
				}

				// Don't let the event go further
				ImGui::SetActiveID( ImGui::GetID( GetImGuiID() ), ImGui::GetCurrentWindow() );

				// Save current pos for the next frame
				s_vLastPos = vCurrentPos;
			}

			s_bWasDragging = bIsDragging;
		}

		// Draw info
		TINT iNumMessages = 0;
		TINT iNumInfos    = 0;

		auto fnPrintErrorMessage = [ & ]( const TCHAR* szMessage ) {
			iNumMessages += 1;

			ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.78f, 0.0f, 0.75f ) );
			ImGui::SetCursorPos( ImVec2( vPreviewPos.x + 13.0f, vPreviewPos.y + ( oRegion.y - ImGui::GetFontSize() * iNumMessages - 8.0f ) ) );
			ImGui::Text( szMessage );
			ImGui::PopStyleColor();
		};

		auto fnPrintMessage = [ & ]( const TCHAR* szMessage ) {
			iNumMessages += 1;

			ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 1.0f, 1.0f, 0.5f ) );
			ImGui::SetCursorPos( ImVec2( vPreviewPos.x + 13.0f, vPreviewPos.y + ( oRegion.y - ImGui::GetFontSize() * iNumMessages - 8.0f ) ) );
			ImGui::Text( szMessage );
			ImGui::PopStyleColor();
		};

		auto fnPrintInfo = [ & ]( const TCHAR* szMessage ) {
			ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 1.0f, 1.0f, 0.5f ) );
			ImGui::SetCursorPos( ImVec2( vPreviewPos.x + 13.0f, vPreviewPos.y + ( ImGui::GetFontSize() * iNumInfos + 8.0f ) ) );
			ImGui::Text( szMessage );
			ImGui::PopStyleColor();

			iNumInfos += 1;
		};

		// Stats
		switch ( m_ModelInstance.pModel->eModelType )
		{
			case ResourceLoader::ModelType::Skin:
				fnPrintInfo( "Shader Type: Skin" );
				break;
			case ResourceLoader::ModelType::World:
				fnPrintInfo( "Shader Type: World" );
				break;
			case ResourceLoader::ModelType::Grass:
				fnPrintInfo( "Shader Type: Grass" );
				break;
			case ResourceLoader::ModelType::StaticInstance:
				fnPrintInfo( "Shader Type: Static Instance" );
				break;
		}

		if ( m_ModelInstance.pModel->pKeyLib )
			fnPrintInfo( TString8::VarArgs( "Keylib: %s", m_ModelInstance.pModel->pKeyLib->GetName().GetString() ) );

		fnPrintInfo( TString8::VarArgs( "Frame Time: %.2fms (%d FPS)", flDeltaTime * 1000.0f, TINT( 1.0f / flDeltaTime ) ) );

		// Hints
		fnPrintMessage( "Hold Right Mouse Button + Shift to rotate camera." );
		fnPrintMessage( "Hold Right Mouse Button to move camera center." );

		if ( m_ModelInstance.pModel->pKeyLib && m_ModelInstance.pModel->pKeyLib->IsDummy() )
		{
			T2String8::Format( T2String8::ms_aScratchMem, "Missing keyframe library '%s'", m_ModelInstance.pModel->pKeyLib->GetName().GetString() );
			fnPrintErrorMessage( T2String8::ms_aScratchMem );
		}

		T2_FOREACH( m_ModelInstance.pModel->vecUsedTextures, it )
		{
			if ( it->Get() && it->Get()->IsDummy() )
			{
				T2String8::Format( T2String8::ms_aScratchMem, "Missing texture '%s'", it->Get()->GetTexture().strName.GetString() );
				fnPrintErrorMessage( T2String8::ms_aScratchMem );
			}
		}

		ImGui::End();
	}

	ImGui::SetNextWindowClass( &windowClass );
	ImGui::Begin( m_strPreferencesId.Get() );
	{
		if ( ImGui::CollapsingHeader( "Rendering" ) )
		{
			ImGui::Text( "Polygon Mode" );
			ImGui::SetNextItemWidth( -1.0f );
			if ( ImGui::BeginCombo( "##Polygon Mode", m_bWireFrame ? "Wireframe" : "Fill" ) )
			{
				if ( ImGui::Selectable( "Fill", !m_bWireFrame ) ) m_bWireFrame = TFALSE;
				if ( ImGui::Selectable( "Wireframe", m_bWireFrame ) ) m_bWireFrame = TTRUE;
				ImGui::EndCombo();
			}

			ImGui::Text( "Wireframe Thickness" );
			ImGui::SetNextItemWidth( -1.0f );
			ImGui::SliderFloat( "##Wireframe Thickness", &m_flWireframeThickness, 0.0f, 5.0f, "%.1f" );

			ImGui::SetNextItemWidth( -1.0f );
			ImGui::Text( "Viewport Background" );
			ImGui::ColorEdit3( "##Viewport Background", &m_vViewportColor.x );

			ImGui::Checkbox( "Disable Textures", &m_bDisableTextures );
		}

		ImGui::End();
	}

	ImGui::PopStyleVar();
}

void ModelResourceView::OnSaveTKL( PTRB* pOutTRB )
{
	// Save TKL
	auto pMemStream = pOutTRB->GetSections()->GetStack( 0 );

	auto pSrcHeader = m_ModelInstance.pModel->pKeyLib->GetTRBHeader();
	auto pTKLHeader = pMemStream->Alloc<TKeyframeLibrary::TRBHeader>();

	pMemStream->Alloc<TCHAR>( &pTKLHeader->m_szName, T2String8::Length( pSrcHeader->m_szName ) + 1 );
	T2String8::Copy( pTKLHeader->m_szName, pSrcHeader->m_szName );
	pTKLHeader->m_iNumTranslations = pOutTRB->ConvertEndianess( pSrcHeader->m_iNumTranslations );
	pTKLHeader->m_iNumQuaternions  = pOutTRB->ConvertEndianess( pSrcHeader->m_iNumQuaternions );
	pTKLHeader->m_iNumScales       = pOutTRB->ConvertEndianess( pSrcHeader->m_iNumScales );
	pTKLHeader->m_iTranslationSize = pOutTRB->ConvertEndianess( pSrcHeader->m_iTranslationSize );
	pTKLHeader->m_iQuaternionSize  = pOutTRB->ConvertEndianess( pSrcHeader->m_iQuaternionSize );
	pTKLHeader->m_iScaleSize       = pOutTRB->ConvertEndianess( pSrcHeader->m_iScaleSize );
	pTKLHeader->m_SomeVector       = TVector3( 0.0f, 0.0f, 0.0f );

	pMemStream->Alloc<TAnimVector>( &pTKLHeader->m_pTranslations, pSrcHeader->m_iNumTranslations );
	pMemStream->Alloc<TAnimQuaternion>( &pTKLHeader->m_pQuaternions, pSrcHeader->m_iNumQuaternions );
	pMemStream->Alloc<TAnimScale>( &pTKLHeader->m_pScales, pSrcHeader->m_iNumScales );

	for ( TINT i = 0; i < pSrcHeader->m_iNumTranslations; i++ )
	{
		pTKLHeader->m_pTranslations[ i ] = TVector3(
		    pOutTRB->ConvertEndianess( pSrcHeader->m_pTranslations[ i ].x ),
		    pOutTRB->ConvertEndianess( pSrcHeader->m_pTranslations[ i ].y ),
		    pOutTRB->ConvertEndianess( pSrcHeader->m_pTranslations[ i ].z )
		);
	}

	for ( TINT i = 0; i < pSrcHeader->m_iNumQuaternions; i++ )
	{
		pTKLHeader->m_pQuaternions[ i ] = TQuaternion(
		    pOutTRB->ConvertEndianess( pSrcHeader->m_pQuaternions[ i ].x ),
		    pOutTRB->ConvertEndianess( pSrcHeader->m_pQuaternions[ i ].y ),
		    pOutTRB->ConvertEndianess( pSrcHeader->m_pQuaternions[ i ].z ),
		    pOutTRB->ConvertEndianess( pSrcHeader->m_pQuaternions[ i ].w )
		);
	}

	for ( TINT i = 0; i < pSrcHeader->m_iNumScales; i++ )
	{
		pTKLHeader->m_pScales[ i ] = pOutTRB->ConvertEndianess( pSrcHeader->m_pScales[ i ] );
	}

	pOutTRB->GetSymbols()->Add( pMemStream, "keylib", pTKLHeader.get() );
}

TBOOL ModelResourceView::ExportScene( tinygltf::Model& rOutModel )
{
	T2SharedPtr<ResourceLoader::Model> pModel = m_ModelInstance.pModel;
	if ( !pModel ) return TFALSE;

	tinygltf::Model& gltfModel = rOutModel;
	tinygltf::Scene  gltfScene;

	tinygltf::Node gltfRootNode;
	gltfRootNode.name       = "Model";
	TINT iCollisionRootNode = -1;
	TINT iSkeletonRootNode  = -1;

	//TQuaternion quatRotation;
	//quatRotation.SetFromEulerRollPitchYaw( TMath::DegToRad( -90.0f ), 0.0f, 0.0f );
	//gltfRootNode.rotation = { quatRotation.x, quatRotation.y, quatRotation.z, quatRotation.w };

	//-----------------------------------------------------------------------------
	// 1. Skeleton
	//-----------------------------------------------------------------------------
	if ( m_ModelInstance.pSkeletonInstance && m_ModelInstance.pSkeletonInstance->GetSkeleton() )
	{
		// Create a buffer for IBM
		tinygltf::Buffer gltfIBMBuffer;

		// Initialise Skin object
		tinygltf::Skin gltfSkin;
		gltfSkin.name = "ASkinMesh";

		TSkeleton* pSkeleton = m_ModelInstance.pSkeletonInstance->GetSkeleton();

		T2Map<TINT, TINT> mapEngineBoneToGLTF;

		// Add all of the bones as separate nodes
		const TINT iBaseBoneIndex = TINT( gltfModel.nodes.size() );
		for ( TINT i = 0; i < pSkeleton->GetBoneCount(); i++ )
		{
			TSkeletonBone* pBone       = pSkeleton->GetBone( i );
			const TINT     iParentBone = pBone->GetParentBone();

			// Copy inverse transform of the bone to the inverse bind matrix buffer
			gltfIBMBuffer.data.insert( gltfIBMBuffer.data.end(), (TBYTE*)&pBone->GetTransformInv(), (TBYTE*)( &pBone->GetTransformInv() + 1 ) );

			tinygltf::Node gltfBoneNode;
			gltfBoneNode.name = pBone->GetName();

			// Revert parent transform if needed
			TMatrix44 matBoneLocal;
			if ( iParentBone != -1 )
			{
				matBoneLocal.Multiply(
				    pSkeleton->GetBone( iParentBone )->GetTransformInv(),
				    pBone->GetTransform()
				);
			}
			else
			{
				matBoneLocal = pBone->GetTransform();
			}

			TQuaternion quatBoneRotation;
			TMatrix44::MatToQuat( quatBoneRotation, matBoneLocal );

			gltfBoneNode.translation = { matBoneLocal.m_f41, matBoneLocal.m_f42, matBoneLocal.m_f43 };
			gltfBoneNode.rotation    = { quatBoneRotation.x, quatBoneRotation.y, quatBoneRotation.z, quatBoneRotation.w };

			gltfModel.nodes.push_back( std::move( gltfBoneNode ) );
			const TINT iBoneIndex = gltfModel.nodes.size() - 1;

			// Save indices to the map
			mapEngineBoneToGLTF.Insert( i, iBoneIndex );

			gltfSkin.joints.push_back( iBoneIndex );

			// Set parenting
			if ( iParentBone == -1 )
			{
				gltfRootNode.children.push_back( gltfModel.nodes.size() - 1 );
				if ( iSkeletonRootNode == -1 )
					iSkeletonRootNode = iBoneIndex;
			}
			else
			{
				gltfModel.nodes[ iBaseBoneIndex + iParentBone ].children.push_back( iBoneIndex );
			}
		}

		// Add the IBM buffer
		gltfModel.buffers.push_back( gltfIBMBuffer );
		const TINT iIBMBufferIndex = gltfModel.buffers.size() - 1;

		// Inverse bind buffer view
		tinygltf::BufferView gltfIBMBufferView;
		gltfIBMBufferView.buffer     = iIBMBufferIndex;
		gltfIBMBufferView.byteLength = sizeof( TMatrix44 ) * pSkeleton->GetBoneCount();

		gltfModel.bufferViews.push_back( gltfIBMBufferView );
		const TINT iIBMBufferViewIndex = gltfModel.bufferViews.size() - 1;

		// Inverse bind buffer accessor
		tinygltf::Accessor gltfAccIBM;
		gltfAccIBM.bufferView    = iIBMBufferViewIndex;
		gltfAccIBM.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
		gltfAccIBM.type          = TINYGLTF_TYPE_MAT4;
		gltfAccIBM.count         = pSkeleton->GetBoneCount();

		gltfModel.accessors.push_back( std::move( gltfAccIBM ) );
		const TINT iAccIBMIndex = TINT( gltfModel.accessors.size() - 1 );

		// Finally add the Skin object
		gltfSkin.inverseBindMatrices = iAccIBMIndex;
		gltfModel.skins.push_back( std::move( gltfSkin ) );

		//-----------------------------------------------------------------------------
		// 1.2. Sequences
		//-----------------------------------------------------------------------------

		if ( pSkeleton->GetSequenceCount() > 0 )
		{
			// Create buffer for keyframes and animation data
			tinygltf::Buffer gltfAnimationTimeBuffer;
			tinygltf::Buffer gltfAnimationQuatBuffer;
			tinygltf::Buffer gltfAnimationTranBuffer;

			// Create buffer views
			tinygltf::BufferView gltfAnimationTimeBufferView;
			gltfAnimationTimeBufferView.buffer = gltfModel.buffers.size() + 0;

			tinygltf::BufferView gltfAnimationQuatBufferView;
			gltfAnimationQuatBufferView.buffer = gltfModel.buffers.size() + 1;

			tinygltf::BufferView gltfAnimationTranBufferView;
			gltfAnimationTranBufferView.buffer = gltfModel.buffers.size() + 2;

			const TINT nAnimTimeBufferViewIdx = TINT( gltfModel.bufferViews.size() + 0 );
			const TINT nAnimQuatBufferViewIdx = TINT( gltfModel.bufferViews.size() + 1 );
			const TINT nAnimTranBufferViewIdx = TINT( gltfModel.bufferViews.size() + 2 );

			for ( TINT i = 0; i < pSkeleton->GetSequenceCount(); i++ )
			{
				TSkeletonSequence*     pSeq      = pSkeleton->GetSequence( i );
				TSkeletonSequenceBone* pSeqBones = pSeq->GetBones();

				tinygltf::Animation gltfAnimation;
				gltfAnimation.name = pSeq->GetName();

				const TINT iNumAutoBones = pSkeleton->GetAutoBoneCount();
				for ( TINT k = 0; k < iNumAutoBones; k++ )
				{
					TSkeletonSequenceBone* pSeqBone = &pSeqBones[ k ];
					const TINT             iNumKeys = pSeqBone->GetKeyCount();

					// Skip bones without any keyframes
					if ( iNumKeys <= 0 ) continue;

					const TBOOL bTranslationAnimated = pSeqBone->IsTranslateAnimated();

					const TINT iTimeBufferOffset = TINT( gltfAnimationTimeBuffer.data.size() );
					const TINT iQuatBufferOffset = TINT( gltfAnimationQuatBuffer.data.size() );
					const TINT iTranBufferOffset = TINT( gltfAnimationTranBuffer.data.size() );

					TFLOAT flBoneMaxKeyTime = 0.0f;

					// Write animation data to the buffer
					TINT iNumRealKeys = 0;
					for ( TINT j = 0; j < iNumKeys; j++ )
					{
						TUINT16* pKeyData = pSeqBone->GetKey( j );

						// For some reason Barnyard models can have multiple keys happening at the same time
						// We will use only the last one, since otherwise it would cause errors
						if ( j + 1 < iNumKeys && *pKeyData == *pSeqBone->GetKey( j + 1 ) ) continue;
						iNumRealKeys += 1;

						// Write key time
						TFLOAT flKeyTime = ( *pKeyData / 65535.0f ) * pSeq->GetDuration();
						gltfAnimationTimeBuffer.data.insert(
						    gltfAnimationTimeBuffer.data.end(),
						    TREINTERPRETCAST( const TBYTE*, &flKeyTime ),
						    TREINTERPRETCAST( const TBYTE*, &flKeyTime + 1 )
						);

						flBoneMaxKeyTime = TMath::Max( flBoneMaxKeyTime, flKeyTime );

						// Write quaternion and position
						const TAnimQuaternion* pQuaternion = pSkeleton->GetKeyLibraryInstance().GetQ( pKeyData[ 1 ] );
						const TAnimVector*     pPosition   = &TVector3::VEC_ZERO;

						if ( bTranslationAnimated )
							pPosition = pSkeleton->GetKeyLibraryInstance().GetT( pKeyData[ 2 ] );

						gltfAnimationQuatBuffer.data.insert(
						    gltfAnimationQuatBuffer.data.end(),
						    TREINTERPRETCAST( const TBYTE*, pQuaternion ),
						    TREINTERPRETCAST( const TBYTE*, pQuaternion + 1 )
						);

						gltfAnimationTranBuffer.data.insert(
						    gltfAnimationTranBuffer.data.end(),
						    TREINTERPRETCAST( const TBYTE*, pPosition ),
						    TREINTERPRETCAST( const TBYTE*, pPosition + 1 )
						);
					}

					// Should never happen, but let's make sure...
					if ( iNumRealKeys == 0 ) continue;

					// Create animation channels
					TINT iBoneGLTF = mapEngineBoneToGLTF[ k ]->second;

					tinygltf::Accessor gltfTimeAccessor;
					gltfTimeAccessor.bufferView    = nAnimTimeBufferViewIdx;
					gltfTimeAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
					gltfTimeAccessor.type          = TINYGLTF_TYPE_SCALAR;
					gltfTimeAccessor.count         = iNumRealKeys;
					gltfTimeAccessor.byteOffset    = iTimeBufferOffset;
					gltfTimeAccessor.minValues     = { 0.0f };
					gltfTimeAccessor.maxValues     = { flBoneMaxKeyTime };

					gltfModel.accessors.push_back( std::move( gltfTimeAccessor ) );
					const TINT iAccTimeIndex = TINT( gltfModel.accessors.size() - 1 );

					tinygltf::Accessor gltfQuatAccessor;
					gltfQuatAccessor.bufferView    = nAnimQuatBufferViewIdx;
					gltfQuatAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
					gltfQuatAccessor.type          = TINYGLTF_TYPE_VEC4;
					gltfQuatAccessor.count         = iNumRealKeys;
					gltfQuatAccessor.byteOffset    = iQuatBufferOffset;

					gltfModel.accessors.push_back( std::move( gltfQuatAccessor ) );
					const TINT iAccQuatIndex = TINT( gltfModel.accessors.size() - 1 );

					tinygltf::AnimationSampler gltfAnimationSamplerQuat;
					gltfAnimationSamplerQuat.input  = iAccTimeIndex;
					gltfAnimationSamplerQuat.output = iAccQuatIndex;

					gltfAnimation.samplers.push_back( std::move( gltfAnimationSamplerQuat ) );
					const TINT iSamplerQuatIndex = TINT( gltfAnimation.samplers.size() - 1 );

					tinygltf::AnimationChannel gltfAnimChanQuat;
					gltfAnimChanQuat.target_node = iBoneGLTF;
					gltfAnimChanQuat.target_path = "rotation";
					gltfAnimChanQuat.sampler     = iSamplerQuatIndex;

					gltfAnimation.channels.push_back( std::move( gltfAnimChanQuat ) );

					if ( bTranslationAnimated )
					{
						tinygltf::Accessor gltfPosAccessor;
						gltfPosAccessor.bufferView    = nAnimTranBufferViewIdx;
						gltfPosAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
						gltfPosAccessor.type          = TINYGLTF_TYPE_VEC3;
						gltfPosAccessor.count         = iNumRealKeys;
						gltfPosAccessor.byteOffset    = iTranBufferOffset;

						gltfModel.accessors.push_back( std::move( gltfPosAccessor ) );
						const TINT iAccPosIndex = TINT( gltfModel.accessors.size() - 1 );

						tinygltf::AnimationSampler gltfAnimationSamplerPos;
						gltfAnimationSamplerPos.input  = iAccTimeIndex;
						gltfAnimationSamplerPos.output = iAccPosIndex;

						gltfAnimation.samplers.push_back( std::move( gltfAnimationSamplerPos ) );
						const TINT iSamplerPosIndex = TINT( gltfAnimation.samplers.size() - 1 );

						tinygltf::AnimationChannel gltfAnimChanPos;
						gltfAnimChanPos.target_node = iBoneGLTF;
						gltfAnimChanPos.target_path = "translation";
						gltfAnimChanPos.sampler     = iSamplerPosIndex;

						gltfAnimation.channels.push_back( std::move( gltfAnimChanPos ) );
					}
				}

				gltfModel.animations.push_back( std::move( gltfAnimation ) );
			}

			// Submit buffers...
			gltfAnimationTimeBufferView.byteLength = gltfAnimationTimeBuffer.data.size();
			gltfAnimationQuatBufferView.byteLength = gltfAnimationQuatBuffer.data.size();
			gltfAnimationTranBufferView.byteLength = gltfAnimationTranBuffer.data.size();

			gltfModel.buffers.push_back( std::move( gltfAnimationTimeBuffer ) );
			gltfModel.buffers.push_back( std::move( gltfAnimationQuatBuffer ) );
			gltfModel.buffers.push_back( std::move( gltfAnimationTranBuffer ) );

			gltfModel.bufferViews.push_back( std::move( gltfAnimationTimeBufferView ) );
			gltfModel.bufferViews.push_back( std::move( gltfAnimationQuatBufferView ) );
			gltfModel.bufferViews.push_back( std::move( gltfAnimationTranBufferView ) );
		}
	}

	//-----------------------------------------------------------------------------
	// 2. Serialize the meshes
	//-----------------------------------------------------------------------------

	for ( TINT k = 0; k < pModel->iLODCount; k++ )
	{
		// For each LOD...
		TSIZE uiStartMesh = gltfModel.nodes.size();

		// Serialize meshes
		Toshi::TModelLOD* pLOD = &pModel->aLODs[ k ];
		for ( TINT i = 0; i < pLOD->iNumMeshes; i++ )
		{
			Mesh* pMesh = TSTATICCAST( Mesh, pLOD->ppMeshes[ i ] );

			TBOOL bSerialized = pMesh->SerializeGLTFMesh( gltfModel, m_ModelInstance.pSkeletonInstance );
			TASSERT( bSerialized == TTRUE );
		}

		TSIZE uiEndMesh = gltfModel.nodes.size();

		for ( TSIZE i = uiStartMesh; i < uiEndMesh; i++ )
		{
			gltfRootNode.children.push_back( TINT( i ) );
		}
	}

	//-----------------------------------------------------------------------------
	// 3. Collision
	//-----------------------------------------------------------------------------

	if ( !m_bIsExternal && m_pTRB )
	{
		auto pCollisionHeader = m_pTRB->GetSymbols()->Find<TTMDBase::CollisionHeader>( m_pTRB->GetSections(), "Collision" );

		if ( pCollisionHeader )
		{
			const TINT            iNumCollisionMeshes = m_pTRB->ConvertEndianess( pCollisionHeader->m_iNumMeshes );
			T2DynamicVector<TINT> vecCollisionNodes;

			auto fnGetCollisionMaterial = [ &gltfModel ]( const TCHAR* pchGroupName ) -> TINT {
				TString8 strMaterialName = TString8::VarArgs( "Collision_%s", pchGroupName ? pchGroupName : "Default" );
				TINT     iMaterialIndex  = gltfModel.FindMaterialIndex( strMaterialName.GetString() );

				if ( iMaterialIndex == -1 )
				{
					tinygltf::Material gltfMaterial;
					gltfMaterial.name                                 = strMaterialName.GetString();
					gltfMaterial.pbrMetallicRoughness.baseColorFactor = { 0.0, 0.75, 1.0, 0.35 };
					gltfMaterial.alphaMode                            = "BLEND";
					gltfMaterial.doubleSided                          = true;

					gltfModel.materials.push_back( std::move( gltfMaterial ) );
					iMaterialIndex = TINT( gltfModel.materials.size() - 1 );
				}

				return iMaterialIndex;
			};

			for ( TINT i = 0; i < iNumCollisionMeshes; i++ )
			{
				auto& rCollisionMesh = pCollisionHeader->m_pMeshes[ i ];

				const TUINT uiNumVertices = m_pTRB->ConvertEndianess( rCollisionMesh.m_uiNumVertices );
				const TUINT uiNumIndices  = m_pTRB->ConvertEndianess( rCollisionMesh.m_uiNumIndices );
				if ( uiNumVertices == 0 || uiNumIndices == 0 ) continue;

				tinygltf::Buffer gltfBuffer;
				const TINT       iBufferIndex = TINT( gltfModel.buffers.size() );

				std::vector<double> aMinVals = {
					std::numeric_limits<double>::max(),
					std::numeric_limits<double>::max(),
					std::numeric_limits<double>::max()
				};
				std::vector<double> aMaxVals = {
					std::numeric_limits<double>::lowest(),
					std::numeric_limits<double>::lowest(),
					std::numeric_limits<double>::lowest()
				};

				for ( TUINT k = 0; k < uiNumVertices; k++ )
				{
					TVector3 vecPosition(
					    m_pTRB->ConvertEndianess( rCollisionMesh.m_pVertices[ k ].x ),
					    m_pTRB->ConvertEndianess( rCollisionMesh.m_pVertices[ k ].y ),
					    m_pTRB->ConvertEndianess( rCollisionMesh.m_pVertices[ k ].z )
					);

					aMinVals[ 0 ] = TMath::Min( aMinVals[ 0 ], TDOUBLE( vecPosition.x ) );
					aMinVals[ 1 ] = TMath::Min( aMinVals[ 1 ], TDOUBLE( vecPosition.y ) );
					aMinVals[ 2 ] = TMath::Min( aMinVals[ 2 ], TDOUBLE( vecPosition.z ) );
					aMaxVals[ 0 ] = TMath::Max( aMaxVals[ 0 ], TDOUBLE( vecPosition.x ) );
					aMaxVals[ 1 ] = TMath::Max( aMaxVals[ 1 ], TDOUBLE( vecPosition.y ) );
					aMaxVals[ 2 ] = TMath::Max( aMaxVals[ 2 ], TDOUBLE( vecPosition.z ) );

					gltfBuffer.data.insert( gltfBuffer.data.end(), TREINTERPRETCAST( const TBYTE*, &vecPosition ), TREINTERPRETCAST( const TBYTE*, &vecPosition + 1 ) );
				}

				const TSIZE uiIndexBufferOffset = gltfBuffer.data.size();
				for ( TUINT k = 0; k < uiNumIndices; k++ )
				{
					TUINT16 uiIndex = m_pTRB->ConvertEndianess( rCollisionMesh.m_pIndices[ k ] );
					gltfBuffer.data.insert( gltfBuffer.data.end(), TREINTERPRETCAST( const TBYTE*, &uiIndex ), TREINTERPRETCAST( const TBYTE*, &uiIndex + 1 ) );
				}

				tinygltf::BufferView gltfBufferViewVertex;
				gltfBufferViewVertex.buffer     = iBufferIndex;
				gltfBufferViewVertex.byteOffset = 0;
				gltfBufferViewVertex.byteLength = uiNumVertices * sizeof( TVector3 );
				gltfBufferViewVertex.byteStride = sizeof( TVector3 );
				gltfBufferViewVertex.target     = TINYGLTF_TARGET_ARRAY_BUFFER;
				gltfModel.bufferViews.push_back( std::move( gltfBufferViewVertex ) );
				const TINT iVertexBufferView = TINT( gltfModel.bufferViews.size() - 1 );

				tinygltf::Accessor gltfAccPosition;
				gltfAccPosition.bufferView    = iVertexBufferView;
				gltfAccPosition.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
				gltfAccPosition.type          = TINYGLTF_TYPE_VEC3;
				gltfAccPosition.count         = uiNumVertices;
				gltfAccPosition.minValues     = std::move( aMinVals );
				gltfAccPosition.maxValues     = std::move( aMaxVals );
				gltfModel.accessors.push_back( std::move( gltfAccPosition ) );
				const TINT iAccPositionIndex = TINT( gltfModel.accessors.size() - 1 );

				tinygltf::BufferView gltfBufferViewIndex;
				gltfBufferViewIndex.buffer     = iBufferIndex;
				gltfBufferViewIndex.byteOffset = uiIndexBufferOffset;
				gltfBufferViewIndex.byteLength = uiNumIndices * sizeof( TUINT16 );
				gltfBufferViewIndex.target     = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
				gltfModel.bufferViews.push_back( std::move( gltfBufferViewIndex ) );
				const TINT iIndexBufferView = TINT( gltfModel.bufferViews.size() - 1 );

				const TUINT uiNumCollTypes = m_pTRB->ConvertEndianess( rCollisionMesh.m_uiNumCollTypes );
				TUINT       uiFaceOffset   = 0;
				TBOOL       bCreatedNode   = TFALSE;

				auto fnCreateCollisionNode = [ &gltfModel, &vecCollisionNodes, &fnGetCollisionMaterial, iAccPositionIndex ]( const TCHAR* pchCollisionName, TINT iAccIndicesIndex ) {
					const TCHAR* pchName = pchCollisionName ? pchCollisionName : "default";

					tinygltf::Primitive gltfPrimitive;
					gltfPrimitive.attributes[ "POSITION" ] = iAccPositionIndex;
					gltfPrimitive.indices                  = iAccIndicesIndex;
					gltfPrimitive.mode                     = TINYGLTF_MODE_TRIANGLES;
					gltfPrimitive.material                 = fnGetCollisionMaterial( pchName );

					tinygltf::Mesh gltfCollisionMesh;
					gltfCollisionMesh.name = pchName;
					gltfCollisionMesh.primitives.push_back( std::move( gltfPrimitive ) );

					gltfModel.meshes.push_back( std::move( gltfCollisionMesh ) );

					tinygltf::Node gltfCollisionNode;
					gltfCollisionNode.mesh = TINT( gltfModel.meshes.size() - 1 );
					gltfCollisionNode.name = pchName;
					gltfModel.nodes.push_back( std::move( gltfCollisionNode ) );
					vecCollisionNodes.PushBack( TINT( gltfModel.nodes.size() - 1 ) );
				};

				for ( TUINT k = 0; k < uiNumCollTypes; k++ )
				{
					auto&       rCollisionGroup = rCollisionMesh.m_pCollGroups[ k ];
					const TUINT uiNumFaces      = m_pTRB->ConvertEndianess( rCollisionGroup.uiNumFaces );
					if ( uiNumFaces == 0 ) continue;
					if ( ( uiFaceOffset + uiNumFaces ) * 3 > uiNumIndices ) break;

					tinygltf::Accessor gltfAccIndex;
					gltfAccIndex.bufferView    = iIndexBufferView;
					gltfAccIndex.byteOffset    = uiFaceOffset * 3 * sizeof( TUINT16 );
					gltfAccIndex.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
					gltfAccIndex.type          = TINYGLTF_TYPE_SCALAR;
					gltfAccIndex.count         = uiNumFaces * 3;
					gltfModel.accessors.push_back( std::move( gltfAccIndex ) );
					const TINT iAccIndicesIndex = TINT( gltfModel.accessors.size() - 1 );

					fnCreateCollisionNode( rCollisionGroup.pszName, iAccIndicesIndex );
					bCreatedNode = TTRUE;

					uiFaceOffset += uiNumFaces;
				}

				if ( uiFaceOffset * 3 < uiNumIndices )
				{
					tinygltf::Accessor gltfAccIndex;
					gltfAccIndex.bufferView    = iIndexBufferView;
					gltfAccIndex.byteOffset    = uiFaceOffset * 3 * sizeof( TUINT16 );
					gltfAccIndex.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
					gltfAccIndex.type          = TINYGLTF_TYPE_SCALAR;
					gltfAccIndex.count         = uiNumIndices - ( uiFaceOffset * 3 );
					gltfModel.accessors.push_back( std::move( gltfAccIndex ) );
					const TINT iAccIndicesIndex = TINT( gltfModel.accessors.size() - 1 );

					fnCreateCollisionNode( "default", iAccIndicesIndex );
					bCreatedNode = TTRUE;
				}

				if ( !bCreatedNode )
				{
					tinygltf::Accessor gltfAccIndex;
					gltfAccIndex.bufferView    = iIndexBufferView;
					gltfAccIndex.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
					gltfAccIndex.type          = TINYGLTF_TYPE_SCALAR;
					gltfAccIndex.count         = uiNumIndices;
					gltfModel.accessors.push_back( std::move( gltfAccIndex ) );
					const TINT iAccIndicesIndex = TINT( gltfModel.accessors.size() - 1 );

					fnCreateCollisionNode( "default", iAccIndicesIndex );
				}

				gltfModel.buffers.push_back( std::move( gltfBuffer ) );
			}

			if ( !vecCollisionNodes.IsEmpty() )
			{
				tinygltf::Node gltfCollisionRootNode;
				gltfCollisionRootNode.name = "Collision";

				T2_FOREACH( vecCollisionNodes, itNode )
				{
					gltfCollisionRootNode.children.push_back( *itNode );
				}

				gltfModel.nodes.push_back( std::move( gltfCollisionRootNode ) );
				iCollisionRootNode = TINT( gltfModel.nodes.size() - 1 );
			}
		}
	}

	// Finalize
	gltfModel.nodes.push_back( std::move( gltfRootNode ) );
	const TINT iModelRootNode = TINT( gltfModel.nodes.size() - 1 );

	tinygltf::Node gltfFileRootNode;
	gltfFileRootNode.name = m_strFileName.GetString();
	gltfFileRootNode.children.push_back( iModelRootNode );

	if ( iCollisionRootNode != -1 )
		gltfFileRootNode.children.push_back( iCollisionRootNode );

	gltfModel.nodes.push_back( std::move( gltfFileRootNode ) );
	gltfScene.nodes.push_back( TINT( gltfModel.nodes.size() - 1 ) );
	gltfModel.scenes.push_back( std::move( gltfScene ) );

	if ( gltfModel.skins.size() > 0 )
		gltfModel.skins[ 0 ].skeleton = iSkeletonRootNode;

	return TTRUE;
}

TBOOL ModelResourceView::TryFixingMissingTKL()
{
	if ( m_ModelInstance.pModel->pKeyLib && m_ModelInstance.pModel->pKeyLib->IsDummy() )
	{
		PTRB oTKLFile;

		TString8   strModelDir;
		const TINT iLastSlashIndex = m_strFilePath.FindReverse( '\\' );
		strModelDir.Copy( m_strFilePath, iLastSlashIndex + 1 );

		if ( !oTKLFile.ReadFromFile( TString8::VarArgs( "%s\\%s.tkl", strModelDir.GetString(), m_ModelInstance.pModel->pKeyLib->GetName().GetString() ).GetString() ) )
			return TFALSE;

		auto pTKLHeader = oTKLFile.GetSymbols()->Find<TKeyframeLibrary::TRBHeader>( oTKLFile.GetSections(), "keylib" );
		if ( !pTKLHeader ) return TFALSE;

		return m_ModelInstance.pModel->pKeyLib->Create( pTKLHeader.get() ) && ResourceLoader::Model_PrepareAnimations( m_ModelInstance.pModel.Get() );
	}

	return TTRUE;
}

void ModelResourceView::SerializeModelInformation( tinyxml2::XMLDocument* pOutput )
{
	auto pTMDLElem = pOutput->NewElement( "TMDL" );
	pOutput->InsertEndChild( pTMDLElem );

	pTMDLElem->SetAttribute( "Target", "Win" );
	pTMDLElem->SetAttribute( "Name", m_strFileName.Mid( 0, m_strFileName.FindReverse( '.' ) ).GetString() );
	pTMDLElem->SetAttribute( "Type", ResourceLoader::GetModelTypeName( m_ModelInstance.pModel->eModelType ) );

	auto pLODsElem = pTMDLElem->InsertNewChildElement( "LODs" );

	auto pRenderDistanceElem = pLODsElem->InsertNewChildElement( "RenderDistance" );
	pRenderDistanceElem->SetText( m_ModelInstance.pModel->fRenderDistance );

	for ( TINT i = 0; i < m_ModelInstance.pModel->iLODCount; i++ )
	{
		auto pLODElem = pLODsElem->InsertNewChildElement( "LOD" );
		pLODElem->SetAttribute( "Index", i );
		pLODElem->SetAttribute( "Distance", m_ModelInstance.pModel->aLODDistances[ i ] );
		pLODElem->SetAttribute( "MeshCount", m_ModelInstance.pModel->aLODs[ i ].iNumMeshes );

		// Store the bounding sphere so a round-trip keeps the exact original
		const TSphere& rSphere = m_ModelInstance.pModel->aLODs[ i ].BoundingSphere;
		pLODElem->SetAttribute( "SphereX", rSphere.GetOrigin().x );
		pLODElem->SetAttribute( "SphereY", rSphere.GetOrigin().y );
		pLODElem->SetAttribute( "SphereZ", rSphere.GetOrigin().z );
		pLODElem->SetAttribute( "SphereRadius", rSphere.GetRadius() );
	}

	auto pMaterialsElem = pTMDLElem->InsertNewChildElement( "Materials" );

	// Serialize materials
	T2Map<TPString8, TString8, TPString8::Comparator> mapMaterials;
	for ( TINT k = 0; k < m_ModelInstance.pModel->iLODCount; k++ )
	{
		TString8 strMaterialName;
		TString8 strTextureName;

		Toshi::TModelLOD* pLOD = &m_ModelInstance.pModel->aLODs[ k ];
		for ( TINT i = 0; i < pLOD->iNumMeshes; i++ )
		{
			Mesh* pMesh = TSTATICCAST( Mesh, pLOD->ppMeshes[ i ] );
			pMesh->GetMaterialInfo( strMaterialName, strTextureName );

			if ( mapMaterials.Find( TPS8D( strMaterialName ) ) == mapMaterials.End() )
			{
				// It's the first time we encounter this material
				mapMaterials.Insert( TPS8D( strMaterialName ), strTextureName );
			}
		}
	}

	T2_FOREACH( mapMaterials, it )
	{
		auto pMaterialElem = pMaterialsElem->InsertNewChildElement( "Material" );

		pMaterialElem->SetAttribute( "Name", it->first.GetString() );
		pMaterialElem->SetAttribute( "Texture", it->second.GetString() );
	}

	auto pSkeletonElem = pTMDLElem->InsertNewChildElement( "TSkeleton" );
	if ( TSkeletonInstance* pSkeletonInstance = m_ModelInstance.pSkeletonInstance )
	{
		TSkeleton* pSkeleton = pSkeletonInstance->GetSkeleton();

		const TINT iNumBones = pSkeleton->m_iBoneCount;
		const TINT iNumSeq   = pSkeleton->m_iSequenceCount;

		// Serialize info about the bones
		auto pBonesElem = pSkeletonElem->InsertNewChildElement( "Bones" );
		for ( TINT i = 0; i < iNumBones; i++ )
		{
			auto pBone     = &pSkeleton->m_pBones[ i ];
			auto pBoneElem = pBonesElem->InsertNewChildElement( "Bone" );

			pBoneElem->SetAttribute( "Name", pBone->GetName() );
			pBoneElem->SetAttribute( "Parent", pBone->GetParentBone() );

			pBoneElem->SetAttribute( "PosX", pBone->GetPosition().x );
			pBoneElem->SetAttribute( "PosY", pBone->GetPosition().y );
			pBoneElem->SetAttribute( "PosZ", pBone->GetPosition().z );

			pBoneElem->SetAttribute( "QuatX", pBone->GetRotation().x );
			pBoneElem->SetAttribute( "QuatY", pBone->GetRotation().y );
			pBoneElem->SetAttribute( "QuatZ", pBone->GetRotation().z );
			pBoneElem->SetAttribute( "QuatW", pBone->GetRotation().w );
		}

		// Serialize info about the sequences
		auto pSequencesElem = pSkeletonElem->InsertNewChildElement( "Sequences" );
		pSequencesElem->SetAttribute( "KeyLibrary", m_ModelInstance.pModel->pKeyLib->GetName().GetString() );

		for ( TINT i = 0; i < iNumSeq; i++ )
		{
			auto pSeq     = &pSkeleton->m_SkeletonSequences[ i ];
			auto pSeqElem = pSequencesElem->InsertNewChildElement( "Sequence" );

			pSeqElem->SetAttribute( "Name", pSeq->GetName() );
			pSeqElem->SetAttribute( "Overlay", pSeq->IsOverlay() );
			pSeqElem->SetAttribute( "Looped", pSeq->GetMode() == TOSHI_NAMESPACE::TSkeletonSequence::MODE_LOOPED );
			pSeqElem->SetAttribute( "Duration", pSeq->GetDuration() );
			pSeqElem->SetAttribute( "Bones", pSeq->m_iNumUsedBones );
		}
	}

	auto pCollisionElem = pTMDLElem->InsertNewChildElement( "Collision" );
	pCollisionElem->SetAttribute( "MeshCount", m_ModelInstance.pModel->iNumCollisionMeshes );

	for ( TINT i = 0; i < m_ModelInstance.pModel->iNumCollisionMeshes; i++ )
	{
		auto& rCollisionMesh = m_ModelInstance.pModel->pCollisionMeshes[ i ];

		auto pMeshElem = pCollisionElem->InsertNewChildElement( "Mesh" );
		pMeshElem->SetAttribute( "Index", i );
		pMeshElem->SetAttribute( "Bone", rCollisionMesh.iBoneID );

		if ( TSkeletonInstance* pSkeletonInstance = m_ModelInstance.pSkeletonInstance )
		{
			TSkeleton* pSkeleton = pSkeletonInstance->GetSkeleton();
			const TINT iBoneId   = rCollisionMesh.iBoneID;

			if ( iBoneId >= 0 && iBoneId < pSkeleton->GetBoneCount() )
				pMeshElem->SetAttribute( "BoneName", pSkeleton->GetBone( iBoneId )->GetName() );
		}

		pMeshElem->SetAttribute( "Vertices", rCollisionMesh.uiNumVertices );
		pMeshElem->SetAttribute( "Indices", rCollisionMesh.uiNumIndices );

		for ( TINT k = 0; k < rCollisionMesh.vecGroups.Size(); k++ )
		{
			auto& rCollisionGroup = rCollisionMesh.vecGroups[ k ];

			auto pGroupElem = pMeshElem->InsertNewChildElement( "Group" );
			pGroupElem->SetAttribute( "Name", rCollisionGroup.strName.GetString() );
			pGroupElem->SetAttribute( "Faces", rCollisionGroup.uiNumFaces );
		}
	}
}

void ModelResourceView::DeserializeModelInformation( tinyxml2::XMLDocument* pInput )
{
	auto pTMDLElem = pInput->FirstChildElement( "TMDL" );
	if ( !pTMDLElem ) return;

	// Texture path per material, so OnSave can override the glTF texture (Blender
	// strips or drops it)
	if ( auto pMaterialsElem = pTMDLElem->FirstChildElement( "Materials" ) )
	{
		for ( auto pMatElem = pMaterialsElem->FirstChildElement( "Material" ); pMatElem != TNULL; pMatElem = pMatElem->NextSiblingElement( "Material" ) )
		{
			const TCHAR* pchName    = pMatElem->Attribute( "Name" );
			const TCHAR* pchTexture = pMatElem->Attribute( "Texture" );
			if ( pchName && pchTexture )
				m_mapXMLTextureOverrides.Insert( TPS8D( pchName ), pchTexture );
		}
	}

	if ( auto pLODsElem = pTMDLElem->FirstChildElement( "LODs" ) )
	{
		if ( auto pRenderDistanceElem = pLODsElem->FirstChildElement( "RenderDistance" ) )
			m_ModelInstance.pModel->fRenderDistance = pRenderDistanceElem->FloatText( m_ModelInstance.pModel->fRenderDistance );

		for ( auto pLODElem = pLODsElem->FirstChildElement( "LOD" ); pLODElem != TNULL; pLODElem = pLODElem->NextSiblingElement( "LOD" ) )
		{
			const TINT iLODIndex = pLODElem->IntAttribute( "Index", -1 );
			if ( iLODIndex < 0 || iLODIndex >= 5 ) continue;

			m_ModelInstance.pModel->aLODDistances[ iLODIndex ] = pLODElem->FloatAttribute( "Distance", m_ModelInstance.pModel->aLODDistances[ iLODIndex ] );

			// Override the computed fallback with the stored sphere if present
			if ( pLODElem->Attribute( "SphereRadius" ) && iLODIndex < m_ModelInstance.pModel->iLODCount )
			{
				m_ModelInstance.pModel->aLODs[ iLODIndex ].BoundingSphere = TSphere(
				    pLODElem->FloatAttribute( "SphereX", 0.0f ),
				    pLODElem->FloatAttribute( "SphereY", 0.0f ),
				    pLODElem->FloatAttribute( "SphereZ", 0.0f ),
				    pLODElem->FloatAttribute( "SphereRadius", 0.0f )
				);
			}
		}
	}
	else
	{
		m_ModelInstance.pModel->fRenderDistance = pTMDLElem->FloatAttribute( "LODDistance", m_ModelInstance.pModel->fRenderDistance );
	}

	if ( auto pCollisionElem = pTMDLElem->FirstChildElement( "Collision" ) )
	{
		for ( auto pMeshElem = pCollisionElem->FirstChildElement( "Mesh" ); pMeshElem != TNULL; pMeshElem = pMeshElem->NextSiblingElement( "Mesh" ) )
		{
			const TINT iMeshIndex = pMeshElem->IntAttribute( "Index", -1 );
			if ( iMeshIndex < 0 || iMeshIndex >= m_ModelInstance.pModel->iNumCollisionMeshes ) continue;

			auto& rCollisionMesh   = m_ModelInstance.pModel->pCollisionMeshes[ iMeshIndex ];
			rCollisionMesh.iBoneID = pMeshElem->IntAttribute( "Bone", rCollisionMesh.iBoneID );

			TINT iGroupIndex = 0;
			for ( auto pGroupElem = pMeshElem->FirstChildElement( "Group" ); pGroupElem != TNULL; pGroupElem = pGroupElem->NextSiblingElement( "Group" ) )
			{
				if ( iGroupIndex >= rCollisionMesh.vecGroups.Size() ) break;

				const TCHAR* pchName = TNULL;
				pGroupElem->QueryStringAttribute( "Name", &pchName );
				if ( pchName ) rCollisionMesh.vecGroups[ iGroupIndex ].strName = pchName;

				rCollisionMesh.vecGroups[ iGroupIndex ].uiNumFaces = pGroupElem->UnsignedAttribute( "Faces", rCollisionMesh.vecGroups[ iGroupIndex ].uiNumFaces );
				iGroupIndex += 1;
			}
		}
	}

	auto pSkeletonElem = pTMDLElem->FirstChildElement( "TSkeleton" );
	if ( TSkeletonInstance* pSkeletonInstance = m_ModelInstance.pSkeletonInstance )
	{
		TSkeleton* pSkeleton = pSkeletonInstance->GetSkeleton();

		const TINT iNumBones = pSkeleton->m_iBoneCount;
		const TINT iNumSeq   = pSkeleton->m_iSequenceCount;

		// Deserialize info about the sequences
		auto pSequencesElem = pSkeletonElem->FirstChildElement( "Sequences" );

		for ( auto pSeqElem = pSequencesElem->FirstChildElement( "Sequence" ); pSeqElem != TNULL; pSeqElem = pSeqElem->NextSiblingElement( "Sequence" ) )
		{
			const TCHAR* pSeqElemName = TNULL;
			pSeqElem->QueryStringAttribute( "Name", &pSeqElemName );
			if ( !pSeqElemName ) continue;

			const auto uiSeqNameLength = T2String8::Length( pSeqElemName );

			TSkeletonSequence* pSeq = TNULL;
			for ( TINT i = 0; i < iNumSeq; i++ )
			{
				auto pTestSeq = &pSkeleton->m_SkeletonSequences[ i ];

				if ( pTestSeq->GetNameLength() == uiSeqNameLength && T2String8::CompareNoCase( pTestSeq->GetName(), pSeqElemName ) == 0 )
				{
					pSeq = pTestSeq;
					break;
				}
			}

			if ( !pSeq ) continue;

			const TBOOL bOverlay = pSeqElem->BoolAttribute( "Overlay", TFALSE );
			const TBOOL bLooped  = pSeqElem->BoolAttribute( "Looped", TTRUE );

			pSeq->m_eFlags = bOverlay ? TSkeletonSequence::FLAG_OVERLAY : TSkeletonSequence::FLAG_NONE;
			pSeq->m_eMode  = bLooped ? TSkeletonSequence::MODE_LOOPED : TSkeletonSequence::MODE_CLAMPED;
		}
	}
}

Toshi::TPString8 ModelResourceView::GetTKLName()
{
	if ( m_ModelInstance.pModel->pKeyLib && !m_ModelInstance.pModel->pKeyLib->IsDummy() )
	{
		return m_ModelInstance.pModel->pKeyLib->GetName();
	}

	return TPS8D( "Unknown" );
}
