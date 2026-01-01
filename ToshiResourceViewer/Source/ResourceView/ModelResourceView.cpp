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
{
	m_ViewportFrameBuffer.Create();
	m_ViewportFrameBuffer.CreateDepthTexture( 1920, 1080 );
	m_ViewportFrameBuffer.CreateAttachment( 0, 1920, 1080, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE );

	m_oCamera.SetNearPlane( 0.1f );
}

ModelResourceView::~ModelResourceView()
{
}

TBOOL ModelResourceView::OnCreate()
{
	// Create unique IDs
	m_strAnimationsId.Format( "##Animations%u", GetImGuiID() );

	if ( m_strSymbolName == "FileHeader" )
	{
		TTMDBase::FileHeader* pFileHeader = TSTATICCAST( TTMDBase::FileHeader, m_pData );

		if ( pFileHeader->m_uiMagic != TFourCC( "TMDL" ) &&
		     pFileHeader->m_uiMagic != TFourCC( "LDMT" ) )
			return TFALSE;

		ResourceLoader::Model_CreateInstance( ResourceLoader::Model_Load_Barnyard_Windows( m_pTRB, m_pTRB->GetEndianess() ), m_ModelInstance );
		m_ModelInstance.oTransform.SetMatrix( TMatrix44::IDENTITY );
		m_ModelInstance.oTransform.SetEuler( TVector3( TMath::DegToRad( -90.0f ), 0.0f, 0.0f ) );
		m_ModelInstance.oTransform.SetTranslate( TVector3::VEC_ZERO );
	}
	
	return TRBResourceView::OnCreate() && m_ModelInstance.pModel.IsValid();
}

TBOOL ModelResourceView::CanSave()
{
	return TFALSE;
}

TBOOL ModelResourceView::OnSave( PTRB* pOutTRB )
{
	return TFALSE;
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

			if ( pSkeleton )
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
		ExportScene();

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
	
	if ( m_ModelInstance.pModel->pKeyLib->IsDummy() )
	{
		T2String8::Format( T2String8::ms_aScratchMem, "Missing keyframe library '%s'", m_ModelInstance.pModel->pKeyLib->GetName().GetString() );
		fnPrintErrorMessage( T2String8::ms_aScratchMem );
	}

	T2_FOREACH( m_ModelInstance.pModel->vecUsedTextures, it )
	{
		if ( it->Get()->IsDummy() )
		{
			T2String8::Format( T2String8::ms_aScratchMem, "Missing texture '%s'", it->Get()->GetTexture().strName.GetString() );
			fnPrintErrorMessage( T2String8::ms_aScratchMem );
		}
	}
}

void ModelResourceView::ExportScene()
{
	T2SharedPtr<ResourceLoader::Model> pModel = m_ModelInstance.pModel;
	if ( !pModel ) return;

	tinygltf::Model gltfModel;
	tinygltf::Scene gltfScene;

	tinygltf::Node gltfRootNode;
	gltfRootNode.name = m_strFileName.GetString();
	
	TQuaternion quatRotation;
	quatRotation.SetFromEulerRollPitchYaw( TMath::DegToRad( -90.0f ), 0.0f, 0.0f );
	gltfRootNode.rotation = { quatRotation.x, quatRotation.y, quatRotation.z, quatRotation.w };

	//-----------------------------------------------------------------------------
	// 2. Skeleton
	//-----------------------------------------------------------------------------
	if ( m_ModelInstance.pSkeletonInstance && m_ModelInstance.pSkeletonInstance->GetSkeleton() )
	{
		// Create a buffer for IBM
		tinygltf::Buffer gltfIBMBuffer;

		// Initialise Skin object
		tinygltf::Skin gltfSkin;
		gltfSkin.name = "ASkinMesh";

		TSkeleton* pSkeleton = m_ModelInstance.pSkeletonInstance->GetSkeleton();

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

			gltfBoneNode.matrix = {
				matBoneLocal.m_f11, matBoneLocal.m_f12, matBoneLocal.m_f13, matBoneLocal.m_f14,
				matBoneLocal.m_f21, matBoneLocal.m_f22, matBoneLocal.m_f23, matBoneLocal.m_f24,
				matBoneLocal.m_f31, matBoneLocal.m_f32, matBoneLocal.m_f33, matBoneLocal.m_f34,
				matBoneLocal.m_f41, matBoneLocal.m_f42, matBoneLocal.m_f43, matBoneLocal.m_f44,
			};

			gltfModel.nodes.push_back( std::move( gltfBoneNode ) );
			const TINT iBoneIndex = gltfModel.nodes.size() - 1;

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
	}

	//-----------------------------------------------------------------------------
	// 1. Serialize the meshes
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
	gltfWriter.WriteGltfSceneToFile( &gltfModel, "D:\\exported.gltf", TFALSE, TTRUE, TTRUE, TFALSE );
}
