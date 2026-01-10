#include "pch.h"
#include "ModelResourceView.h"
#include "Shader/Mesh.h"

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
	m_strAnimationsId.Format( "##Animations%u", GetImGuiID() );

	if ( m_bIsExternal )
	{
		// Create from GLTF

		// Skinned mesh
		ResourceLoader::Model_CreateInstance( ResourceLoader::Model_LoadSkin_GLTF( pchFilePath ), m_ModelInstance );
	}
	else
	{
		// Create from TRB

		// Skinned mesh
		if ( m_strSymbolName == "FileHeader" )
		{
			TTMDBase::FileHeader* pFileHeader = TSTATICCAST( TTMDBase::FileHeader, m_pData );

			if ( pFileHeader->m_uiMagic != TFourCC( "TMDL" ) &&
				 pFileHeader->m_uiMagic != TFourCC( "LDMT" ) )
				return TFALSE;

			ResourceLoader::Model_CreateInstance( ResourceLoader::Model_Load_Barnyard_Windows( m_pTRB, m_pTRB->GetEndianess() ), m_ModelInstance );
		}
	}

	// Update transform
	m_ModelInstance.oTransform.SetMatrix( TMatrix44::IDENTITY );
	m_ModelInstance.oTransform.SetEuler( TVector3( TMath::DegToRad( -90.0f ), 0.0f, 0.0f ) );
	m_ModelInstance.oTransform.SetTranslate( TVector3::VEC_ZERO );
	
	return TRBResourceView::OnCreate( pchFilePath ) && m_ModelInstance.pModel.IsValid() && TryFixingMissingTKL();
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
		PTRB* pTKLTRB       = new PTRB( pOutTRB->GetEndianess() );
		auto  pMemStream = pTKLTRB->GetSections()->CreateStream();

		OnSaveTKL( pTKLTRB );

		pTKLTRB->WriteToFile( TString8::VarArgs( "%s.tkl", pKeyLib->GetTRBHeader()->m_szName ).GetString(), TFALSE );
	}

	PTRBSections* pSECT = pOutTRB->GetSections();
	PTRBSymbols* pSYMB = pOutTRB->GetSymbols();

	PTRBSections::MemoryStream* pMemStream = pSECT->GetStack( 0 );

	// Allocate FileHeader symbol
	auto pTRBFileHeader = pMemStream->Alloc<TTMDBase::FileHeader>();
	pTRBFileHeader->m_uiMagic = pOutTRB->ConvertEndianess( TFourCC( "TMDL" ) );
	pTRBFileHeader->m_uiZero1 = pOutTRB->ConvertEndianess( 0 );
	pTRBFileHeader->m_uiVersionMajor = pOutTRB->ConvertEndianess( TTMD_VERSION_MAJOR );
	pTRBFileHeader->m_uiVersionMinor = pOutTRB->ConvertEndianess( TTMD_VERSION_MINOR );
	pTRBFileHeader->m_uiZero2 = pOutTRB->ConvertEndianess( 0 );
	pSYMB->Add( pMemStream, "FileHeader", pTRBFileHeader.get() );

	// Allocate SkeletonHeader symbol
	auto pTRBSkeletonHeader = pMemStream->Alloc<TTMDBase::SkeletonHeader>();
	T2String8::Copy( pTRBSkeletonHeader->m_szTKLName, m_ModelInstance.pModel->oSkeletonHeader.m_szTKLName, sizeof( pTRBSkeletonHeader->m_szTKLName ) - 1 );
	pTRBSkeletonHeader->m_iTKeyCount  = pOutTRB->ConvertEndianess( pKeyLib->GetNumTranslations() );
	pTRBSkeletonHeader->m_iQKeyCount  = pOutTRB->ConvertEndianess( pKeyLib->GetNumQuaternions() );
	pTRBSkeletonHeader->m_iSKeyCount  = pOutTRB->ConvertEndianess( pKeyLib->GetNumScales() ); // Barnyard does not support scale keyframes
	pTRBSkeletonHeader->m_iTBaseIndex = pOutTRB->ConvertEndianess( 0 );
	pTRBSkeletonHeader->m_iQBaseIndex = pOutTRB->ConvertEndianess( 0 );
	pTRBSkeletonHeader->m_iSBaseIndex = pOutTRB->ConvertEndianess( 0 ); // Barnyard does not support scale keyframes
	pSYMB->Add( pMemStream, "SkeletonHeader", pTRBSkeletonHeader.get() );

	TSkeletonInstance* pSkeletonInstance = m_ModelInstance.pSkeletonInstance;
	TSkeleton* pSkeleton = pSkeletonInstance->GetSkeleton();

	// Allocate Skeleton symbol
	const TINT iNumBones = pSkeleton->m_iBoneCount;
	const TINT iNumSeq = pSkeleton->m_iSequenceCount;

	auto pTRBSkeleton = pMemStream->Alloc<TSkeleton>();
	pTRBSkeleton->m_iBoneCount = pOutTRB->ConvertEndianess( iNumBones );
	pTRBSkeleton->m_iManualBoneCount = pOutTRB->ConvertEndianess( pSkeleton->m_iManualBoneCount );
	pTRBSkeleton->m_iSequenceCount = pOutTRB->ConvertEndianess( iNumSeq );
	pTRBSkeleton->m_iAnimationMaxCount = pOutTRB->ConvertEndianess( pSkeleton->m_iAnimationMaxCount );
	pTRBSkeleton->m_iInstanceCount = pOutTRB->ConvertEndianess( 0 );
	pTRBSkeleton->m_eQuatLerpType = pOutTRB->ConvertEndianess( TSkeleton::QUATINTERP_Default );
	
	// Copy info about the bones
	pMemStream->Alloc<TSkeletonBone>( &pTRBSkeleton->m_pBones, iNumBones );
	for ( TINT i = 0; i < iNumBones; i++ )
		TUtil::MemCopy( &pTRBSkeleton->m_pBones[ i ], &pSkeleton->m_pBones[ i ], sizeof( TSkeletonBone ) );

	// Copy info about the sequences
	auto pTRBSkeletonSeq = pMemStream->Alloc<TSkeletonSequence>( &pTRBSkeleton->m_SkeletonSequences, iNumSeq );
	for ( TINT i = 0; i < iNumSeq; i++ )
	{
		auto pSeq = pSkeleton->GetSequence( i );
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
			auto pSeqBone = &pSeq->m_pSeqBones[ k ];
			auto pTRBSeqBone = pTRBSeqBones + k;

			const TINT iKeySize = pSeqBone->m_iKeySize;
			const TINT iNumKeys = pSeqBone->m_iNumKeys;

			TASSERT( iKeySize == 4 || iKeySize == 6 ); // time, quaternion (+ translation sometimes)
			pTRBSeqBone->m_eFlags = pOutTRB->ConvertEndianess( pSeqBone->m_eFlags );
			pTRBSeqBone->m_iKeySize = pOutTRB->ConvertEndianess( iKeySize );
			pTRBSeqBone->m_iNumKeys = pOutTRB->ConvertEndianess( iNumKeys );
			
			// Copy keyframe data
			pMemStream->Alloc<TBYTE>( &pTRBSeqBone->m_pData, iKeySize * iNumKeys );

			for ( TINT j = 0; j < iNumKeys; j++ )
			{
				TUINT16* pKeyData = pSeqBone->GetKey( j );
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
	auto pTRBMaterialsHeader = pMemStream->Alloc<TTMDBase::MaterialsHeader>();

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

	auto pTRBMaterials = pMemStream->Alloc<TTMDBase::Material>( iNumMaterials );
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
	// NOTE: we don't support collision at the moment
	auto pTRBCollision = pMemStream->Alloc<TTMDBase::CollisionHeader>();
	pTRBCollision->m_iNumMeshes = pOutTRB->ConvertEndianess( 0 );

	pSYMB->Add( pMemStream, "Collision", pTRBCollision.get() );

	// Write the main TTMD header (Windows) and information about the LODs
	const TINT iNumLODs = pModel->iLODCount;

	auto pTRBWinHeader = pMemStream->Alloc<TTMDWin::TRBWinHeader>();
	pTRBWinHeader->m_iNumLODs     = pOutTRB->ConvertEndianess( iNumLODs );
	pTRBWinHeader->m_fLODDistance = pOutTRB->ConvertEndianess( pModel->fLODDistance );

	auto pTRBLODs = pMemStream->Alloc<TTMDWin::TRBLODHeader>( iNumLODs );
	for ( TINT i = 0; i < iNumLODs; i++ )
	{
		auto pLOD    = &pModel->aLODs[ i ];
		auto pTRBLOD = pTRBLODs + i;

		pTRBLOD->m_iMeshCount1 = pOutTRB->ConvertEndianess( pLOD->iNumMeshes );
		pTRBLOD->m_iMeshCount2 = pOutTRB->ConvertEndianess( 0 );
		pTRBLOD->m_eShader     = pOutTRB->ConvertEndianess( TTMDWin::ST_SKIN );
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
			auto  pTRBLODMesh = pMemStream->Alloc<TTMDWin::TRBLODMesh>();

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
	ImVec2 vInitialPos = ImGui::GetCursorPos();

	ImGui::Text( "Sequences" );
	ImGui::BeginChild( m_strAnimationsId.Get(), ImVec2( 200, -1 ), ImGuiChildFlags_ResizeX );
	{
		ImGui::PushStyleColor( ImGuiCol_FrameBg, ImVec4( 0, 0, 0, 0 ) );
		if ( ImGui::BeginListBox( "AnimationList", ImVec2( -1, -1 ) ) )
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

		ImGui::EndChild();
	}

	ImGui::SameLine();
	ImVec2 vPreviewPos = ImGui::GetCursorPos();
	ImGui::SetCursorPos( ImVec2( vPreviewPos.x, vInitialPos.y ) );
	ImGui::Text( "Preview" );
	ImGui::SameLine();

	ImGui::SetCursorPos( ImVec2( ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize( "Export" ).x - ImGui::GetStyle().FramePadding.x * 2, vInitialPos.y ) );
	if ( ImGui::SmallButton( "Export" ) )
		ExportScene( "D:\\exported.gltf" );

	// Prepare camera
	TVector3& oCamTranslation = m_oCamera->GetTranslation();

	m_fCameraDistance = TMath::LERPClamped( m_fCameraDistance, m_fCameraDistanceTarget, TMath::Max( TMath::Abs( m_fCameraDistanceTarget - m_fCameraDistance ), 8.0f ) * flDeltaTime );
	m_oCamera.SetFOV( TMath::DegToRad( m_fCameraFOV ) );

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
	ImGui::SetCursorPos( vPreviewPos );
	ImVec2 oRegion = ImGui::GetContentRegionAvail();
	
	g_pRenderGL->SetRenderContext( m_oRenderContext );
	m_oRenderContext.ForceRefreshFeatures();

	m_oRenderContext.GetViewport().SetWidth( oRegion.x );
	m_oRenderContext.GetViewport().SetHeight( oRegion.y );

	m_oRenderContext.SetCamera( m_oCamera );
	m_oRenderContext.UpdateCamera();

	// Render scene
	{
		m_ViewportFrameBuffer.Bind();
		m_oRenderContext.GetViewport().Begin();

		m_ModelInstance.oTransform.GetLocalMatrixImp( m_oRenderContext.GetModelMatrix() );

		if ( m_ModelInstance.pModel )
		{
			if ( m_ModelInstance.pSkeletonInstance && ResourceLoader::Model_PrepareAnimations( m_ModelInstance.pModel ) )
			{
				m_ModelInstance.pSkeletonInstance->UpdateState( TTRUE );
				
				if ( m_iSelectedSequence != -1 && !m_ModelInstance.pSkeletonInstance->IsAnyAnimationPlaying() )
					m_ModelInstance.pSkeletonInstance->AddAnimationFull( m_iSelectedSequence, 1.0f, 0.0f, 0.0f, TAnimation::Flags_Managed );

				m_ModelInstance.pSkeletonInstance->UpdateTime( flDeltaTime );

				if ( m_iSelectedSequence == -1 || m_ModelInstance.pSkeletonInstance->IsAnyAnimationPlaying() )
					m_ModelInstance.pSkeletonInstance->UpdateState( TTRUE );

				m_oRenderContext.SetSkeletonInstance( m_ModelInstance.pSkeletonInstance );
			}

			m_ModelInstance.pModel->Render();
		}
	
		g_pRenderGL->FlushOrderTables();

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
					TVector4 vecUpAxis = oCameraMatrix.AsBasisVector4( BASISVECTOR_UP );
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

	auto fnPrintErrorMessage = [ & ]( const TCHAR* szMessage ) {
		iNumMessages += 1;

		ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.78f, 0.0f, 0.75f ) );
		ImGui::SetCursorPos( ImVec2( vPreviewPos.x + 8.0f, vPreviewPos.y + ( oRegion.y - ImGui::GetFontSize() * iNumMessages - 4.0f ) ) );
		ImGui::Text( szMessage );
		ImGui::PopStyleColor();
	};

	auto fnPrintMessage = [ & ]( const TCHAR* szMessage ) {
		iNumMessages += 1;

		ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 1.0f, 1.0f, 0.5f ) );
		ImGui::SetCursorPos( ImVec2( vPreviewPos.x + 8.0f, vPreviewPos.y + ( oRegion.y - ImGui::GetFontSize() * iNumMessages - 4.0f ) ) );
		ImGui::Text( szMessage );
		ImGui::PopStyleColor();
	};

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

void ModelResourceView::ExportScene( Toshi::T2StringView pchOutFileName )
{
	T2SharedPtr<ResourceLoader::Model> pModel = m_ModelInstance.pModel;
	if ( !pModel ) return;

	tinygltf::Model gltfModel;
	tinygltf::Scene gltfScene;

	tinygltf::Node gltfRootNode;
	gltfRootNode.name = m_strFileName.GetString();
	
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
			TSkeletonBone* pBone = pSkeleton->GetBone( i );
			const TINT iParentBone = pBone->GetParentBone();

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
			gltfBoneNode.rotation = { quatBoneRotation.x, quatBoneRotation.y, quatBoneRotation.z, quatBoneRotation.w };

			gltfModel.nodes.push_back( std::move( gltfBoneNode ) );
			const TINT iBoneIndex = gltfModel.nodes.size() - 1;

			// Save indices to the map
			mapEngineBoneToGLTF.Insert( i, iBoneIndex );

			gltfSkin.joints.push_back( iBoneIndex );

			// Set parenting
			if ( iParentBone == -1 )
				gltfRootNode.children.push_back( gltfModel.nodes.size() - 1 );
			else
				gltfModel.nodes[ iBaseBoneIndex + iParentBone ].children.push_back( iBoneIndex );
		}

		// Add the IBM buffer
		gltfModel.buffers.push_back( gltfIBMBuffer );
		const TINT iIBMBufferIndex = gltfModel.buffers.size() - 1;

		// Inverse bind buffer view
		tinygltf::BufferView gltfIBMBufferView;
		gltfIBMBufferView.buffer = iIBMBufferIndex;
		gltfIBMBufferView.byteLength = sizeof( TMatrix44 ) * pSkeleton->GetBoneCount();

		gltfModel.bufferViews.push_back( gltfIBMBufferView );
		const TINT iIBMBufferViewIndex = gltfModel.bufferViews.size() - 1;

		// Inverse bind buffer accessor
		tinygltf::Accessor gltfAccIBM;
		gltfAccIBM.bufferView = iIBMBufferViewIndex;
		gltfAccIBM.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
		gltfAccIBM.type = TINYGLTF_TYPE_MAT4;
		gltfAccIBM.count = pSkeleton->GetBoneCount();

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

	const TBOOL bHasFewLODs = pModel->iLODCount != 1;

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

		if ( bHasFewLODs )
		{
			// Setup LOD node
			tinygltf::Node gltfLODNode;
			for ( TSIZE i = uiStartMesh; i < uiEndMesh; i++ )
				gltfLODNode.children.push_back( i );

			gltfLODNode.name = TString8::VarArgs( "LOD%d", k ).GetString();

			// Add LOD node
			gltfModel.nodes.push_back( std::move( gltfLODNode ) );
			gltfRootNode.children.push_back( gltfModel.nodes.size() - 1 );
		}
		else
		{
			// Add straight to the root
			for ( TSIZE i = uiStartMesh; i < uiEndMesh; i++ )
			{
				gltfRootNode.children.push_back( i );
			}
		}
		
	}

	// Finalize
	gltfModel.nodes.push_back( std::move( gltfRootNode ) );
	gltfScene.nodes.push_back( gltfModel.nodes.size() - 1 );
	gltfModel.scenes.push_back( std::move( gltfScene ) );

	gltfModel.skins[ 0 ].skeleton = gltfModel.nodes.size() - 1;

	// Write to the file
	tinygltf::TinyGLTF gltfWriter;
	gltfWriter.WriteGltfSceneToFile( &gltfModel, pchOutFileName.Get(), TFALSE, TTRUE, TTRUE, TFALSE );
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

		return m_ModelInstance.pModel->pKeyLib->Create( pTKLHeader.get() ) && ResourceLoader::Model_PrepareAnimations( m_ModelInstance.pModel );
	}

	return TTRUE;
}

Toshi::TPString8 ModelResourceView::GetTKLName()
{
	if ( m_ModelInstance.pModel->pKeyLib && !m_ModelInstance.pModel->pKeyLib->IsDummy() )
	{
		return m_ModelInstance.pModel->pKeyLib->GetName();
	}

	return TPS8D( "Unknown" );
}
