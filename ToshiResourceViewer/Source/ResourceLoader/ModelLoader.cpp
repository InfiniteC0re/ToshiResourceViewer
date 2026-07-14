#include "pch.h"
#include "ModelLoader.h"
#include "Shader/SkinShader.h"
#include "Shader/WorldShader.h"
#include "Resource/StreamedTexture.h"
#include "MeshOptimize/MeshOptimize.h"
#include "Application.h"

#include <Toshi/T2String.h>
#include <Toshi/T2Vector.h>
#include <Toshi/TBitField.h>
#include <Render/TModel.h>
#include <Render/TTMDWin.h>
#include <Render/TTMDPS2.h>
#include <Plugins/PTRB.h>

#include <Platform/GL/T2Render_GL.h>
#include <Platform/GL/T2GLTexture_GL.h>

#include <tiny_gltf.h>
#include <cstdio>
#include <map>
#include <array>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

// Specify builder to compile models with common key library
static TKLBuilder* s_pTKLBuilder = TNULL;

// When set, only the listed glTF animations are compiled (see AnimationFilter)
static const ResourceLoader::AnimationFilter* s_pAnimationFilter = TNULL;

// When set, only the listed bones are compiled (see BoneFilter)
static const ResourceLoader::BoneFilter* s_pBoneFilter = TNULL;

static void ModelLoader_InitLODDistances( TFLOAT* a_pLODDistances )
{
	a_pLODDistances[ 0 ] = 5.0f;
	a_pLODDistances[ 1 ] = 20.0f;
	a_pLODDistances[ 2 ] = a_pLODDistances[ 1 ] + a_pLODDistances[ 1 ];
	a_pLODDistances[ 3 ] = a_pLODDistances[ 2 ] + a_pLODDistances[ 2 ];
	a_pLODDistances[ 4 ] = a_pLODDistances[ 3 ] + a_pLODDistances[ 3 ];
}

static TINT ModelLoader_ParseGLTFMeshLOD( const TCHAR* pchName )
{
	TINT iLODIndex = -1;

	if ( std::sscanf( pchName, "LOD%d", &iLODIndex ) == 1 && iLODIndex >= 0 && iLODIndex < 5 )
		return iLODIndex;

	return -1;
}

// Ritter's approximate bounding sphere, used when the XML doesn't provide one
static Toshi::TSphere ModelLoader_ComputeBoundingSphere( const Toshi::T2DynamicVector<Toshi::TVector3>& rvecPoints )
{
	if ( rvecPoints.IsEmpty() )
		return TSphere( 0.0f, 0.0f, 0.0f, 0.0f );

	auto fnFarthest = []( const Toshi::T2DynamicVector<TVector3>& rPts, const TVector3& rFrom ) -> TVector3 {
		TINT   iBest  = 0;
		TFLOAT flBest = -1.0f;
		for ( TINT i = 0; i < rPts.Size(); i++ )
		{
			const TFLOAT d = TVector3::DistanceSq( rPts[ i ], rFrom );
			if ( d > flBest ) { flBest = d; iBest = i; }
		}
		return rPts[ iBest ];
	};

	// Seed with the two most distant points, then grow to fit the rest
	const TVector3 vecX = fnFarthest( rvecPoints, rvecPoints[ 0 ] );
	const TVector3 vecY = fnFarthest( rvecPoints, vecX );

	TVector3 vecCenter( ( vecX.x + vecY.x ) * 0.5f, ( vecX.y + vecY.y ) * 0.5f, ( vecX.z + vecY.z ) * 0.5f );
	TFLOAT   flRadius = TVector3::Distance( vecY, vecCenter );

	for ( TINT i = 0; i < rvecPoints.Size(); i++ )
	{
		const TFLOAT flDist = TVector3::Distance( rvecPoints[ i ], vecCenter );
		if ( flDist > flRadius )
		{
			const TFLOAT   flMove = ( flDist - flRadius ) / ( 2.0f * flDist );
			const TVector3 vecDir = rvecPoints[ i ] - vecCenter;
			vecCenter = TVector3( vecCenter.x + vecDir.x * flMove, vecCenter.y + vecDir.y * flMove, vecCenter.z + vecDir.z * flMove );
			flRadius  = ( flRadius + flDist ) * 0.5f;
		}
	}

	return TSphere( vecCenter, flRadius );
}

static void ModelLoader_MarkGLTFCollisionMeshes( const tinygltf::Model& gltfModel, TINT iNodeIndex, T2DynamicVector<TBOOL>& vecCollisionMeshes )
{
	if ( iNodeIndex < 0 || iNodeIndex >= TINT( gltfModel.nodes.size() ) ) return;

	auto& gltfNode = gltfModel.nodes[ iNodeIndex ];
	if ( gltfNode.mesh >= 0 && gltfNode.mesh < TINT( vecCollisionMeshes.Size() ) )
		vecCollisionMeshes[ gltfNode.mesh ] = TTRUE;

	for ( TSIZE i = 0; i < gltfNode.children.size(); i++ )
		ModelLoader_MarkGLTFCollisionMeshes( gltfModel, gltfNode.children[ i ], vecCollisionMeshes );
}

// Builds the per-mesh collision mask: a mesh is collision if it hangs under a "Collision"
// node or its material is a "Collision_*" material. Shared by the skin and world loaders
static void ModelLoader_BuildGLTFCollisionMask( const tinygltf::Model& gltfModel, T2DynamicVector<TBOOL>& vecCollisionMeshes )
{
	vecCollisionMeshes.SetSize( gltfModel.meshes.size() );
	for ( TSIZE i = 0; i < gltfModel.meshes.size(); i++ )
		vecCollisionMeshes[ i ] = TFALSE;

	for ( TSIZE i = 0; i < gltfModel.nodes.size(); i++ )
	{
		if ( gltfModel.nodes[ i ].name == "Collision" )
			ModelLoader_MarkGLTFCollisionMeshes( gltfModel, TINT( i ), vecCollisionMeshes );
	}

	for ( TSIZE i = 0; i < gltfModel.meshes.size(); i++ )
	{
		auto& gltfMesh = gltfModel.meshes[ i ];
		if ( gltfMesh.primitives.empty() ) continue;

		const TINT iMaterial = gltfMesh.primitives[ 0 ].material;
		if ( iMaterial >= 0 && iMaterial < TINT( gltfModel.materials.size() ) && gltfModel.materials[ iMaterial ].name.rfind( "Collision_", 0 ) == 0 )
			vecCollisionMeshes[ i ] = TTRUE;
	}
}

static TUINT16 ModelLoader_ReadGLTFIndex( const tinygltf::Buffer& gltfBuffer, const tinygltf::BufferView& gltfBufferView, const tinygltf::Accessor& gltfAccessor, TUINT iIndex )
{
	const TUINT uiStride = ( gltfBufferView.byteStride != 0 ) ? gltfBufferView.byteStride : tinygltf::GetComponentSizeInBytes( gltfAccessor.componentType );
	const TBYTE* pData   = gltfBuffer.data.data() + gltfBufferView.byteOffset + gltfAccessor.byteOffset + ( uiStride * iIndex );

	switch ( gltfAccessor.componentType )
	{
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
			return *TREINTERPRETCAST( const TUINT8*, pData );
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			return *TREINTERPRETCAST( const TUINT16*, pData );
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
			return TUINT16( *TREINTERPRETCAST( const TUINT32*, pData ) );
		default:
			TASSERT( TFALSE && "Unsupported collision index format" );
			return 0;
	}
}

static const TCHAR* ModelLoader_GetGLTFCollisionName( const tinygltf::Model& gltfModel, const tinygltf::Node* pNode, const tinygltf::Mesh& gltfMesh )
{
	if ( pNode && !pNode->name.empty() && pNode->name != "Collision" )
		return pNode->name.c_str();

	if ( !gltfMesh.name.empty() )
		return gltfMesh.name.c_str();

	if ( !gltfMesh.primitives.empty() )
	{
		const TINT iMaterial = gltfMesh.primitives[ 0 ].material;
		if ( iMaterial >= 0 && iMaterial < TINT( gltfModel.materials.size() ) )
		{
			const std::string& strMaterialName = gltfModel.materials[ iMaterial ].name;
			if ( strMaterialName.rfind( "Collision_", 0 ) == 0 )
				return strMaterialName.c_str() + 10;
		}
	}

	return "default";
}

static TBOOL ModelLoader_IsGLTFCollisionMeshValid( const tinygltf::Mesh& gltfMesh )
{
	if ( gltfMesh.primitives.size() != 1 ) return TFALSE;

	auto& gltfPrimitive = gltfMesh.primitives[ 0 ];
	return gltfPrimitive.indices >= 0 &&
	    gltfPrimitive.mode == TINYGLTF_MODE_TRIANGLES &&
	    gltfPrimitive.attributes.contains( "POSITION" );
}

static void ModelLoader_LoadGLTFCollisionMeshes( const tinygltf::Model& gltfModel, ResourceLoader::Model* pModel, T2DynamicVector<TBOOL>& vecCollisionMeshes )
{
	T2DynamicVector<TINT> vecCollisionNodes;
	vecCollisionNodes.SetSize( gltfModel.meshes.size() );

	for ( TSIZE i = 0; i < gltfModel.meshes.size(); i++ )
		vecCollisionNodes[ i ] = -1;

	for ( TSIZE i = 0; i < gltfModel.nodes.size(); i++ )
	{
		auto& gltfNode = gltfModel.nodes[ i ];
		if ( gltfNode.mesh >= 0 && gltfNode.mesh < TINT( gltfModel.meshes.size() ) && vecCollisionMeshes[ gltfNode.mesh ] )
			vecCollisionNodes[ gltfNode.mesh ] = TINT( i );
	}

	// Each valid collision mesh becomes one group, in gltf order
	T2DynamicVector<TSIZE> vecGroupMeshes;
	for ( TSIZE i = 0; i < gltfModel.meshes.size(); i++ )
	{
		if ( vecCollisionMeshes[ i ] && ModelLoader_IsGLTFCollisionMeshValid( gltfModel.meshes[ i ] ) )
			vecGroupMeshes.PushBack( i );
	}

	if ( vecGroupMeshes.IsEmpty() ) return;

	// The engine stores collision as one mesh split into groups (a run of faces sharing
	// a collision material). The decompiler exports each group as its own glTF mesh over
	// a shared vertex pool, so weld them back into a single mesh - one group per source
	pModel->iNumCollisionMeshes = 1;
	pModel->pCollisionMeshes    = new ResourceLoader::Model::CollisionMeshInfo[ 1 ];

	auto& rOutMesh   = pModel->pCollisionMeshes[ 0 ];
	rOutMesh.iBoneID = -1;

	std::map<std::array<TUINT32, 3>, TUINT16> mapVertex; // exact position bits -> merged index

	T2_FOREACH( vecGroupMeshes, itGroupMesh )
	{
		const TSIZE iMeshIdx      = *itGroupMesh;
		auto&       gltfMesh      = gltfModel.meshes[ iMeshIdx ];
		auto&       gltfPrimitive = gltfMesh.primitives[ 0 ];

		const TINT iAccPositionIndex = gltfPrimitive.attributes.at( "POSITION" );
		auto&      gltfPositionAcc   = gltfModel.accessors[ iAccPositionIndex ];
		auto&      gltfPositionView  = gltfModel.bufferViews[ gltfPositionAcc.bufferView ];
		auto&      gltfPositionBuf   = gltfModel.buffers[ gltfPositionView.buffer ];

		TASSERT( gltfPositionAcc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT );
		TASSERT( gltfPositionAcc.type == TINYGLTF_TYPE_VEC3 );

		const TUINT uiPositionStride = ( gltfPositionView.byteStride != 0 ) ? gltfPositionView.byteStride : sizeof( TVector3 );

		auto& gltfIndexAcc  = gltfModel.accessors[ gltfPrimitive.indices ];
		auto& gltfIndexView = gltfModel.bufferViews[ gltfIndexAcc.bufferView ];
		auto& gltfIndexBuf  = gltfModel.buffers[ gltfIndexView.buffer ];

		TASSERT( gltfIndexAcc.type == TINYGLTF_TYPE_SCALAR );

		const TUINT uiNumIndices = TUINT( gltfIndexAcc.count );
		for ( TUINT k = 0; k < uiNumIndices; k++ )
		{
			const TUINT16   uiSrcIndex = ModelLoader_ReadGLTFIndex( gltfIndexBuf, gltfIndexView, gltfIndexAcc, k );
			const TBYTE*    pPosition  = gltfPositionBuf.data.data() + gltfPositionView.byteOffset + gltfPositionAcc.byteOffset + ( uiPositionStride * uiSrcIndex );
			const TVector3& rPosition  = *TREINTERPRETCAST( const TVector3*, pPosition );

			const std::array<TUINT32, 3> key = {
				*TREINTERPRETCAST( const TUINT32*, &rPosition.x ),
				*TREINTERPRETCAST( const TUINT32*, &rPosition.y ),
				*TREINTERPRETCAST( const TUINT32*, &rPosition.z ),
			};

			auto    it = mapVertex.find( key );
			TUINT16 uiMergedIndex;
			if ( it == mapVertex.end() )
			{
				uiMergedIndex    = TUINT16( rOutMesh.vecVertices.Size() );
				mapVertex[ key ] = uiMergedIndex;
				rOutMesh.vecVertices.PushBack( rPosition );
			}
			else
			{
				uiMergedIndex = it->second;
			}

			rOutMesh.vecIndices.PushBack( uiMergedIndex );
		}

		const tinygltf::Node* pNode = ( vecCollisionNodes[ iMeshIdx ] != -1 ) ? &gltfModel.nodes[ vecCollisionNodes[ iMeshIdx ] ] : TNULL;

		auto& rGroup      = rOutMesh.vecGroups.PushBack();
		rGroup.strName    = ModelLoader_GetGLTFCollisionName( gltfModel, pNode, gltfMesh );
		rGroup.uiNumFaces = uiNumIndices / 3;
	}

	rOutMesh.uiNumVertices = rOutMesh.vecVertices.Size();
	rOutMesh.uiNumIndices  = rOutMesh.vecIndices.Size();
}

// Loads the skeleton (bones + sequences + keyframe library) from the gltf skin into
// pModel, and fills mapGltfBoneToTRVBone (gltf joint node -> TRV bone) for the caller's
// skinning. Shared by the skin and world loaders - world props carry a skeleton too (a
// root/instance bone, usually 0 sequences). No-op when the gltf has no skin
static void ModelLoader_LoadGLTFSkeleton( const tinygltf::Model& gltfModel, ResourceLoader::Model* pModel, Toshi::T2Map<TINT, TINT>& mapGltfBoneToTRVBone, Toshi::T2StringView pchFilePath )
{
	if ( gltfModel.skins.size() != 1 ) return;
	{
		TINFO( "Detected skin\n" );
		auto pGLTFSkin = &gltfModel.skins[ 0 ];

		// Get animations
		static TBOOL bAllowDuplicates = g_pCmd->HasParameter( "-allow-duplicates" );

		// vecAnimations pairs a glTF animation index with the sequence name to write
		T2DynamicVector<T2Pair<TSIZE, TString8>> vecAnimations;

		if ( s_pAnimationFilter )
		{
			// Compile only the animations the model references, renaming each glTF
			// animation to its sequence name (see merged models sharing one glTF)
			for ( TINT f = 0; f < s_pAnimationFilter->Size(); f++ )
			{
				const auto& rEntry = ( *s_pAnimationFilter )[ f ];

				TSIZE iGltfAnim = TSIZE( -1 );
				for ( TSIZE i = 0; i < gltfModel.animations.size(); i++ )
					if ( rEntry.first.Compare( gltfModel.animations[ i ].name.c_str() ) == 0 ) { iGltfAnim = i; break; }

				if ( iGltfAnim == TSIZE( -1 ) )
				{
					TWARN( "Animation '%s' referenced by the model is missing from the GLTF\n", rEntry.first.GetString() );
					continue;
				}

				vecAnimations.PushBack( { iGltfAnim, rEntry.second } );
			}
		}
		else
		{
			for ( TSIZE i = 0; i < gltfModel.animations.size(); i++ )
			{
				const std::string& strAnimName = gltfModel.animations[ i ].name;
				if ( !bAllowDuplicates )
				{
					// Check ending of the name (f.e. .001)
					if ( strAnimName.size() > 4 && strAnimName[ strAnimName.size() - 4 ] == '.' && std::isdigit( strAnimName[ strAnimName.size() - 3 ] ) && std::isdigit( strAnimName[ strAnimName.size() - 2 ] ) && std::isdigit( strAnimName[ strAnimName.size() - 1 ] ) )
						continue;

					// Check the name is already in the list
					TBOOL bDuplicate = TFALSE;
					for ( TINT k = 0; !bDuplicate && k < vecAnimations.Size(); k++ )
						bDuplicate = ( vecAnimations[ k ].second.Compare( strAnimName.c_str() ) == 0 );

					if ( bDuplicate ) continue;
				}

				vecAnimations.PushBack( { i, strAnimName.c_str() } );
			}
		}

		// Initialise TSkeleton
		pModel->pSkeleton = new TSkeleton();

		// Select the joints to compile. With a bone filter (merged models sharing a
		// GLTF) only the model's own bones are kept, otherwise all of them
		T2DynamicVector<TINT>     vecIncludedJoints; // indices into pGLTFSkin->joints
		T2DynamicVector<TString8> vecBoneNames;      // compiled name per included joint
		for ( TSIZE j = 0; j < pGLTFSkin->joints.size(); j++ )
		{
			const std::string& strJointName = gltfModel.nodes[ pGLTFSkin->joints[ j ] ].name;
			TString8           strBoneName  = strJointName.c_str();

			if ( s_pBoneFilter )
			{
				TBOOL bAllowed = TFALSE;
				for ( TINT f = 0; !bAllowed && f < s_pBoneFilter->Size(); f++ )
				{
					if ( ( *s_pBoneFilter )[ f ].first.CompareNoCase( strJointName.c_str() ) != 0 ) continue;
					bAllowed    = TTRUE;
					strBoneName = ( *s_pBoneFilter )[ f ].second;
				}

				if ( !bAllowed ) continue;
			}

			vecIncludedJoints.PushBack( TINT( j ) );
			vecBoneNames.PushBack( strBoneName );
		}

		const TINT iNumBones = vecIncludedJoints.Size();
		const TINT iNumSeq   = TINT( vecAnimations.Size() );

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
			auto             gltfBoneIdx = pGLTFSkin->joints[ vecIncludedJoints[ i ] ];
			auto&            gltfBone    = gltfModel.nodes[ gltfBoneIdx ];
			const TString8&  strBoneName = vecBoneNames[ i ];

			TINFO( "Bone %d: %s\n", i, strBoneName.GetString() );

			// Copy name
			T2String8::Copy( pBones[ i ].m_szName, strBoneName.GetString(), sizeof( pBones[ i ].m_szName ) - 1 );
			pBones[ i ].m_iNameLength = TUINT8( TMath::Min<TSIZE>( strBoneName.Length(), sizeof( pBones[ i ].m_szName ) - 1 ) );

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
			auto  gltfBoneIdx = pGLTFSkin->joints[ vecIncludedJoints[ i ] ];
			auto& gltfBone    = gltfModel.nodes[ gltfBoneIdx ];

			const TINT           iThisBoneIdx = mapGltfBoneToTRVBone[ gltfBoneIdx ]->second;
			const TSkeletonBone* pThisBone    = &pBones[ iThisBoneIdx ];

			for ( TUINT k = 0; k < gltfBone.children.size(); k++ )
			{
				auto itChild = mapGltfBoneToTRVBone.Find( gltfBone.children[ k ] );
				if ( itChild == mapGltfBoneToTRVBone.End() ) continue; // child bone filtered out

				TINT iChildrenBoneIdx = itChild->second;

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
			auto& gltfAnim    = gltfModel.animations[ vecAnimations[ i ].first ];
			auto  strAnimName = vecAnimations[ i ].second;

			// Calculate animation duration and find all keys
			T2Map<TINT, T2DynamicVector<TINT>> mapBoneChannels;

			TFLOAT flAnimDuration = 0.0f;
			for ( TSIZE k = 0; k < gltfAnim.channels.size(); k++ )
			{
				auto& gltfAnimChannel = gltfAnim.channels[ k ];
				if ( gltfAnimChannel.sampler < 0 || gltfAnimChannel.sampler >= TINT( gltfAnim.samplers.size() ) ) continue;
				auto& gltfAnimSampler = gltfAnim.samplers[ gltfAnimChannel.sampler ];

				// Skip broken/foreign animations rather than crash - one bad model mustn't
				// take down the whole batch
				if ( gltfAnimSampler.input < 0 || gltfAnimSampler.input >= TINT( gltfModel.accessors.size() ) ) continue;
				if ( gltfAnimSampler.output < 0 || gltfAnimSampler.output >= TINT( gltfModel.accessors.size() ) ) continue;
				if ( gltfModel.accessors[ gltfAnimSampler.input ].maxValues.empty() ) continue;

				const TFLOAT flKeyTime = TFLOAT( gltfModel.accessors[ gltfAnimSampler.input ].maxValues[ 0 ] );
				flAnimDuration         = TMath::Max( flAnimDuration, flKeyTime );

				auto itChannelBone = mapGltfBoneToTRVBone.Find( gltfAnimChannel.target_node );
				if ( itChannelBone == mapGltfBoneToTRVBone.End() ) continue; // channel targets a filtered bone

				const TINT iTRBBone = itChannelBone->second;
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

				auto        itChannels = mapBoneChannels.Find( k );
				const TBOOL bAnimated  = itChannels != mapBoneChannels.End();

				if ( !bAnimated )
				{
					pSeqBone->m_eFlags         = 2;
					pSeqBone->m_iKeySize       = 4;
					pSeqBone->m_iNumKeys       = 0;
					pSeqBone->m_pData          = new TBYTE[ sizeof( TUINT16 ) ];
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
							TFLOAT  flTime           = *(TFLOAT*)( &*( pGltfDataTime + ( uiTimeBufferStride * j ) + gltfTimeAccessor.byteOffset ) );
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
			static TINT s_iTKLId   = 0;
			TString8    strTKLName = TString8::VarArgs( "dyn_tkl%d", s_iTKLId++ );
			TASSERT( strTKLName.Length() <= sizeof( pModel->oSkeletonHeader.m_szTKLName ) - 1 && "Curse me if this happened" );
			T2String8::Copy( pModel->oSkeletonHeader.m_szTKLName, strTKLName.GetString(), sizeof( pModel->oSkeletonHeader.m_szTKLName ) - 1 );
		}

		pModel->oSkeletonHeader.m_iTKeyCount  = pTKLBuilder->GetTranslations().Size();
		pModel->oSkeletonHeader.m_iQKeyCount  = pTKLBuilder->GetRotations().Size();
		pModel->oSkeletonHeader.m_iSKeyCount  = pTKLBuilder->GetScales().Size();
		pModel->oSkeletonHeader.m_iTBaseIndex = 0;
		pModel->oSkeletonHeader.m_iQBaseIndex = 0;
		pModel->oSkeletonHeader.m_iSBaseIndex = 0;

		// Sequence bone keyframes reference the TKL with 16-bit indices, so a model
		// can hold at most 0x10000 keys. Without compression a large model exceeds
		// this and its keyframe indices wrap around, distorting the animations
		constexpr TINT KEYFRAME_INDEX_LIMIT = 0x10000;
		if ( pModel->oSkeletonHeader.m_iTKeyCount > KEYFRAME_INDEX_LIMIT || pModel->oSkeletonHeader.m_iQKeyCount > KEYFRAME_INDEX_LIMIT )
			TERROR( "Model '%s' has too many keyframes for the 16-bit TKL index (translations=%d, rotations=%d, max=%d). Compile it with TKL compression enabled or its animations will be corrupted\n", pchFilePath.Get(), pModel->oSkeletonHeader.m_iTKeyCount, pModel->oSkeletonHeader.m_iQKeyCount, KEYFRAME_INDEX_LIMIT );

		if ( bGlobalTKLBuilder )
			pModel->pKeyLib = Resource::StreamedKeyLib_FindOrCreateDummy( TPS8D( pTKLBuilder->GetName() ) );
		else
			pModel->pKeyLib = Resource::StreamedKeyLib_Create( TPS8D( pModel->oSkeletonHeader.m_szTKLName ), *pTKLBuilder );

		Model_PrepareAnimations( pModel );
	}
}

Toshi::T2SharedPtr<ResourceLoader::Model> ResourceLoader::Model_LoadSkin_GLTF( Toshi::T2StringView pchFilePath )
{
	TINFO( "Importing skin model from GLTF file: %s\n", pchFilePath.Get() );

	T2SharedPtr<ResourceLoader::Model> pModel = T2SharedPtr<ResourceLoader::Model>::New();

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

	pModel->eModelType         = ModelType::Skin;
	pModel->pTRB               = NULL;
	pModel->iLODCount          = 1;
	pModel->fRenderDistance    = 50.0f;
	pModel->bAnimationsLoaded  = TFALSE;

	ModelLoader_InitLODDistances( pModel->aLODDistances );

	// We can't have more than 1 skins on a single model
	TASSERT( gltfModel.skins.size() <= 1 );

	T2Map<TINT, TINT> mapGltfBoneToTRVBone;
	ModelLoader_LoadGLTFSkeleton( gltfModel, pModel.Get(), mapGltfBoneToTRVBone, pchFilePath );
	const TBOOL bHasSkins = ( pModel->pSkeleton != TNULL );

	// Calculate actual number of meshes per LOD.
	// To make it clear, we count meshes only when materials differ, but Skinned models can have submeshes to fit all bones
	TINT                   iMaxLODIdx         = 0;
	TINT                   aNumMaterials[ 5 ] = {};
	T2DynamicVector<TSIZE> vecActualMeshesByLOD[ 5 ];
	T2DynamicVector<TINT>  vecMeshLODs;
	T2DynamicVector<TBOOL> vecCollisionMeshes;
	T2Map<TINT, TINT>      aMapGltfMatToTRVMat[ 5 ];

	vecMeshLODs.SetSize( gltfModel.meshes.size() );
	vecCollisionMeshes.SetSize( gltfModel.meshes.size() );
	for ( TSIZE i = 0; i < gltfModel.meshes.size(); i++ )
	{
		const TINT iLODIdx = ModelLoader_ParseGLTFMeshLOD( gltfModel.meshes[ i ].name.c_str() );
		vecMeshLODs[ i ]   = ( iLODIdx != -1 ) ? iLODIdx : 0;
		vecCollisionMeshes[ i ] = TFALSE;
	}

	for ( TSIZE i = 0; i < gltfModel.nodes.size(); i++ )
	{
		auto& gltfNode = gltfModel.nodes[ i ];

		if ( gltfNode.name == "Collision" )
		{
			ModelLoader_MarkGLTFCollisionMeshes( gltfModel, i, vecCollisionMeshes );
			continue;
		}

		if ( gltfNode.mesh < 0 || gltfNode.mesh >= TINT( gltfModel.meshes.size() ) ) continue;

		const TINT iLODIdx = ModelLoader_ParseGLTFMeshLOD( gltfNode.name.c_str() );
		if ( iLODIdx != -1 ) vecMeshLODs[ gltfNode.mesh ] = iLODIdx;
	}

	for ( TSIZE i = 0; i < gltfModel.meshes.size(); i++ )
	{
		auto& gltfMesh = gltfModel.meshes[ i ];
		if ( gltfMesh.primitives.empty() ) continue;

		const TINT iMaterial = gltfMesh.primitives[ 0 ].material;
		if ( iMaterial >= 0 && iMaterial < TINT( gltfModel.materials.size() ) && gltfModel.materials[ iMaterial ].name.rfind( "Collision_", 0 ) == 0 )
			vecCollisionMeshes[ i ] = TTRUE;
	}

	ModelLoader_LoadGLTFCollisionMeshes( gltfModel, pModel.Get(), vecCollisionMeshes );

	for ( TSIZE i = 0; i < gltfModel.meshes.size(); i++ )
	{
		auto& gltfMesh = gltfModel.meshes[ i ];
		if ( vecCollisionMeshes[ i ] ) continue;

		TASSERT( gltfMesh.primitives.size() == 1 );

		const TINT iLODIdx     = vecMeshLODs[ i ];
		const TINT iGltfMatIdx = gltfMesh.primitives[ 0 ].material;
		if ( iGltfMatIdx >= 0 && iGltfMatIdx < TINT( gltfModel.materials.size() ) && gltfModel.materials[ iGltfMatIdx ].name.rfind( "Collision_", 0 ) == 0 ) continue;

		if ( aMapGltfMatToTRVMat[ iLODIdx ].Find( iGltfMatIdx ) == aMapGltfMatToTRVMat[ iLODIdx ].End() )
		{
			aMapGltfMatToTRVMat[ iLODIdx ].Insert( iGltfMatIdx, aNumMaterials[ iLODIdx ] );

			aNumMaterials[ iLODIdx ] += 1;
			vecActualMeshesByLOD[ iLODIdx ].PushBack( i );
		}

		iMaxLODIdx = TMath::Max( iMaxLODIdx, iLODIdx );
	}

	pModel->iLODCount = iMaxLODIdx + 1;
	for ( TINT iLODIdx = 0; iLODIdx < pModel->iLODCount; iLODIdx++ )
	{
		auto& vecActualMeshes = vecActualMeshesByLOD[ iLODIdx ];

		// Setup the LOD
		pModel->aLODs[ iLODIdx ].iNumMeshes     = TINT( vecActualMeshes.Size() );
		pModel->aLODs[ iLODIdx ].ppMeshes       = new TMesh*[ vecActualMeshes.Size() ];
		pModel->aLODs[ iLODIdx ].BoundingSphere = TSphere( 0.0f, 0.0f, 0.0f, 0.0f );

		// Collected to derive a fallback bounding sphere below
		T2DynamicVector<TVector3> vecLODPositions;

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

			SkinMesh*                 pMesh     = g_pSkinShader->CreateMesh();
			T2SharedPtr<SkinMaterial> pMaterial = g_pSkinShader->CreateMaterial();

			// Find all submeshes
			T2DynamicVector<TSIZE> vecSubMeshes;
			for ( TSIZE k = 0; k < gltfModel.meshes.size(); k++ )
			{
				if ( !vecCollisionMeshes[ k ] && vecMeshLODs[ k ] == iLODIdx && gltfModel.meshes[ k ].primitives[ 0 ].material == gltfMaterialIdx )
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

			// Generate submeshes to fit the 28 bones limit
			for ( TINT k = 0; k < vecSubMeshes.Size(); k++ )
			{
				auto& gltfSubMesh          = gltfModel.meshes[ vecSubMeshes[ k ] ];
				auto& gltfSubMeshPrimitive = gltfSubMesh.primitives[ 0 ];

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

				TASSERT( gltfIndexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT && gltfIndexAccessor.type == TINYGLTF_TYPE_SCALAR );
				TASSERT( gltfPositionAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && gltfPositionAccessor.type == TINYGLTF_TYPE_VEC3 );
				TASSERT( gltfNormalAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && gltfNormalAccessor.type == TINYGLTF_TYPE_VEC3 );
				TASSERT( ( gltfWeightsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT || gltfWeightsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ) && gltfWeightsAccessor.type == TINYGLTF_TYPE_VEC4 );
				TASSERT( gltfJointsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE && gltfJointsAccessor.type == TINYGLTF_TYPE_VEC4 );
				TASSERT( gltfUVAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && gltfUVAccessor.type == TINYGLTF_TYPE_VEC2 );

				const TBYTE* pGltfDataIndex    = &*( gltfIndexBuffer.data.begin() + gltfIndexBufferView.byteOffset );
				const TBYTE* pGltfDataPosition = &*( gltfPositionBuffer.data.begin() + gltfPositionBufferView.byteOffset );
				const TBYTE* pGltfDataNormal   = &*( gltfNormalBuffer.data.begin() + gltfNormalBufferView.byteOffset );
				const TBYTE* pGltfDataWeights  = &*( gltfWeightsBuffer.data.begin() + gltfWeightsBufferView.byteOffset );
				const TBYTE* pGltfDataJoints   = &*( gltfJointsBuffer.data.begin() + gltfJointsBufferView.byteOffset );
				const TBYTE* pGltfDataUV       = &*( gltfUVBuffer.data.begin() + gltfUVBufferView.byteOffset );

				const TUINT uiIndexStride    = gltfIndexBufferView.byteStride ? gltfIndexBufferView.byteStride : sizeof( TUINT16 );
				const TUINT uiPositionStride = gltfPositionBufferView.byteStride ? gltfPositionBufferView.byteStride : sizeof( TVector3 );
				const TUINT uiNormalStride   = gltfNormalBufferView.byteStride ? gltfNormalBufferView.byteStride : sizeof( TVector3 );
				const TUINT uiWeightsStride  = gltfWeightsBufferView.byteStride ? gltfWeightsBufferView.byteStride : ( gltfWeightsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT ? sizeof( TVector4 ) : sizeof( TUINT32 ) );
				const TUINT uiJointsStride   = gltfJointsBufferView.byteStride ? gltfJointsBufferView.byteStride : sizeof( TUINT32 );
				const TUINT uiUVStride       = gltfUVBufferView.byteStride ? gltfUVBufferView.byteStride : sizeof( TVector2 );

				auto fnReadIndex = [ & ]( TUINT j ) -> TUINT16 {
					return *(const TUINT16*)( pGltfDataIndex + uiIndexStride * j + gltfIndexAccessor.byteOffset );
				};

				// Read the primitive as a triangle list, unrolling triangle strips
				// (restart markers, winding, degenerate stitch triangles)
				std::vector<TUINT16> vecTris;
				if ( gltfSubMeshPrimitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP )
				{
					TUINT uiRunStart = 0;
					for ( TUINT j = 0; j + 2 < gltfIndexAccessor.count; j++ )
					{
						const TUINT16 a = fnReadIndex( j ), b = fnReadIndex( j + 1 ), c = fnReadIndex( j + 2 );
						if ( a == 0xFFFF ) { uiRunStart = j + 1; continue; }
						if ( b == 0xFFFF ) { uiRunStart = j + 2; continue; }
						if ( c == 0xFFFF ) { uiRunStart = j + 3; continue; }
						if ( a == b || b == c || a == c ) continue;

						const TBOOL bEven = ( ( j - uiRunStart ) & 1 ) == 0;
						vecTris.push_back( bEven ? a : b );
						vecTris.push_back( bEven ? b : a );
						vecTris.push_back( c );
					}
				}
				else
				{
					for ( TUINT j = 0; j + 2 < gltfIndexAccessor.count; j += 3 )
					{
						const TUINT16 a = fnReadIndex( j ), b = fnReadIndex( j + 1 ), c = fnReadIndex( j + 2 );
						if ( a == b || b == c || a == c ) continue;
						vecTris.push_back( a );
						vecTris.push_back( b );
						vecTris.push_back( c );
					}
				}

				const TSIZE uiTriCount = vecTris.size() / 3;

				// Resolve skeleton bones
				auto fnJointToBone = [ & ]( TUINT8 uiJointSlot ) -> TINT {
					if ( !pGLTFSkin ) return 0;
					auto it = mapGltfBoneToTRVBone.Find( pGLTFSkin->joints[ uiJointSlot ] );
					return ( it != mapGltfBoneToTRVBone.End() ) ? it->second : 0;
				};

				auto fnReadWeights = [ & ]( TUINT16 m, TFLOAT aflWeights[ 4 ] ) {
					if ( gltfWeightsAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT )
					{
						const TFLOAT* p = (const TFLOAT*)( pGltfDataWeights + uiWeightsStride * m + gltfWeightsAccessor.byteOffset );
						for ( TINT s = 0; s < 4; s++ ) aflWeights[ s ] = p[ s ];
					}
					else
					{
						const TUINT8* p = (const TUINT8*)( pGltfDataWeights + uiWeightsStride * m + gltfWeightsAccessor.byteOffset );
						for ( TINT s = 0; s < 4; s++ ) aflWeights[ s ] = p[ s ] / 255.0f;
					}
				};

				auto fnVertexBones = [ & ]( TUINT16 m, TINT aiBones[ 4 ], TBOOL abAnimated[ 4 ], TFLOAT aflWeights[ 4 ] ) {
					const TUINT8* pJoints = (const TUINT8*)( pGltfDataJoints + uiJointsStride * m + gltfJointsAccessor.byteOffset );
					fnReadWeights( m, aflWeights );
					for ( TINT s = 0; s < 4; s++ )
					{
						aiBones[ s ]    = fnJointToBone( pJoints[ s ] );
						// Gate on weight, NOT joint index: excluding slot 0 (the root body bone)
						// made body verts rigid to it and swung the whole mesh around a wheel bone
						abAnimated[ s ] = aflWeights[ s ] >= ( 1.0f / 255.0f );
					}
				};

				// Builds one submesh from a set of triangles that fit the bone limit,
				// duplicating shared vertices so each submesh owns its own vertices
				auto fnBuildSubMesh = [ & ]( const std::vector<TUINT>& rTriangles ) {
					auto        pSubMesh      = &pMesh->vecSubMeshes.PushBack();
					const TUINT uiStartVertex = vecVertices.Size();

					T2Map<TUINT16, TUINT16> mapGlobalToLocal;
					T2Map<TINT, TINT>       mapUsedBones;
					std::vector<TUINT16>    vecLocalIndices;
					vecLocalIndices.reserve( rTriangles.size() * 3 );

					for ( TUINT uiTri : rTriangles )
					{
						for ( TINT c = 0; c < 3; c++ )
						{
							const TUINT16 m     = vecTris[ uiTri * 3 + c ];
							auto          itMap = mapGlobalToLocal.Find( m );

							if ( itMap != mapGlobalToLocal.End() )
							{
								vecLocalIndices.push_back( itMap->second );
								continue;
							}

							const TUINT16 uiLocal = TUINT16( mapGlobalToLocal.Size() );

							SkinMesh::SkinVertex vtx;
							vtx.Position = *(const TVector3*)( pGltfDataPosition + uiPositionStride * m + gltfPositionAccessor.byteOffset );
							vtx.Normal   = *(const TVector3*)( pGltfDataNormal + uiNormalStride * m + gltfNormalAccessor.byteOffset );
							vtx.UV       = *(const TVector2*)( pGltfDataUV + uiUVStride * m + gltfUVAccessor.byteOffset );

							TINT   aiBones[ 4 ];
							TBOOL  abAnim[ 4 ];
							TFLOAT aflWeights[ 4 ];
							fnVertexBones( m, aiBones, abAnim, aflWeights );

							for ( TINT s = 0; s < 4; s++ )
							{
								if ( abAnim[ s ] && mapUsedBones.Find( aiBones[ s ] ) == mapUsedBones.End() )
								{
									pSubMesh->aBones[ mapUsedBones.Size() ] = aiBones[ s ];
									mapUsedBones.Insert( aiBones[ s ], mapUsedBones.Size() );
								}
							}

							const TBOOL bNoBoneAnimated = !abAnim[ 0 ] && !abAnim[ 1 ] && !abAnim[ 2 ] && !abAnim[ 3 ];

							for ( TINT s = 0; s < 4; s++ )
								vtx.Bones[ s ] = abAnim[ s ] ? ( mapUsedBones[ aiBones[ s ] ]->second * 3 ) : 0;

							TUINT8 auiWeights[ 4 ];
							for ( TINT s = 0; s < 4; s++ ) auiWeights[ s ] = TUINT8( TMath::Round( aflWeights[ s ] * 255.0f ) );

							// Normalize the byte weights so they sum to 255
							TUINT16 uiWeightsSum = auiWeights[ 0 ] + auiWeights[ 1 ] + auiWeights[ 2 ] + auiWeights[ 3 ];
							if ( uiWeightsSum != 255 && uiWeightsSum != 0 )
							{
								TUINT8* pMax = &auiWeights[ 0 ];
								for ( TINT s = 1; s < 4; s++ ) if ( *pMax < auiWeights[ s ] ) pMax = &auiWeights[ s ];

								if ( uiWeightsSum > 255 ) *pMax = *pMax - ( uiWeightsSum - 255 );
								else                      *pMax = *pMax + ( 255 - uiWeightsSum );
							}

							vtx.Weights[ 0 ] = abAnim[ 0 ] ? auiWeights[ 0 ] : ( bNoBoneAnimated ? 255 : 0 );
							vtx.Weights[ 1 ] = abAnim[ 1 ] ? auiWeights[ 1 ] : 0;
							vtx.Weights[ 2 ] = abAnim[ 2 ] ? auiWeights[ 2 ] : 0;
							vtx.Weights[ 3 ] = abAnim[ 3 ] ? auiWeights[ 3 ] : 0;

							mapGlobalToLocal.Insert( m, uiLocal );
							vecVertices.PushBack( vtx );
							vecLocalIndices.push_back( uiLocal );
						}
					}

					TASSERT( mapUsedBones.Size() <= SKINNED_SUBMESH_MAX_BONES && "Bone-palette split failed" );

					// Engine draws strips. This submesh owns its slice of vecVertices, so
					// let the optimizer reorder those verts as well while stripifying.
					std::vector<TUINT16> vecStrip;
					MeshOptimize::StripifyTriangleList(
					    vecLocalIndices.data(), TUINT( vecLocalIndices.size() ),
					    &vecVertices[ uiStartVertex ], vecVertices.Size() - uiStartVertex, sizeof( SkinMesh::SkinVertex ),
					    vecStrip );

					// Indices came back local, shift them onto the shared buffer
					for ( TUINT16& rIndex : vecStrip ) rIndex = TUINT16( uiStartVertex + rIndex );

					T2VertexArray::Unbind();

					pSubMesh->uiNumIndices  = TUINT( vecStrip.size() );
					pSubMesh->oIndexBuffer  = T2Render::CreateIndexBuffer( vecStrip.data(), TUINT( vecStrip.size() ), GL_STATIC_DRAW );
					pSubMesh->uiEndVertexId = vecVertices.Size();
					pSubMesh->uiNumBones    = ( mapUsedBones.Size() == 0 ) ? TMath::Min( SKINNED_SUBMESH_MAX_BONES, TINT( mapGltfBoneToTRVBone.Size() ) ) : mapUsedBones.Size();

					pSubMesh->oVertexArray = T2Render::CreateVertexArray( pMesh->oVertexBuffer, pSubMesh->oIndexBuffer );
					pSubMesh->oVertexArray.Bind();
					pSubMesh->oVertexArray.GetVertexBuffer().SetAttribPointer( 0, 3, GL_FLOAT, sizeof( SkinMesh::SkinVertex ), offsetof( SkinMesh::SkinVertex, Position ) );
					pSubMesh->oVertexArray.GetVertexBuffer().SetAttribPointer( 1, 3, GL_FLOAT, sizeof( SkinMesh::SkinVertex ), offsetof( SkinMesh::SkinVertex, Normal ) );
					pSubMesh->oVertexArray.GetVertexBuffer().SetAttribPointer( 2, 4, GL_UNSIGNED_BYTE, sizeof( SkinMesh::SkinVertex ), offsetof( SkinMesh::SkinVertex, Weights ), GL_TRUE );
					pSubMesh->oVertexArray.GetVertexBuffer().SetAttribPointer( 3, 4, GL_UNSIGNED_BYTE, sizeof( SkinMesh::SkinVertex ), offsetof( SkinMesh::SkinVertex, Bones ), GL_TRUE );
					pSubMesh->oVertexArray.GetVertexBuffer().SetAttribPointer( 4, 2, GL_FLOAT, sizeof( SkinMesh::SkinVertex ), offsetof( SkinMesh::SkinVertex, UV ) );
				};

				// Greedily pack triangles into submeshes within the bone limit
				std::vector<TUINT> vecGroup;
				std::vector<TINT>  vecGroupBones;

				auto fnFlushGroup = [ & ]() {
					if ( vecGroup.empty() ) return;
					fnBuildSubMesh( vecGroup );
					vecGroup.clear();
					vecGroupBones.clear();
				};

				for ( TSIZE t = 0; t < uiTriCount; t++ )
				{
					TINT aiTriBones[ 12 ];
					TINT iNumTriBones = 0;

					for ( TINT c = 0; c < 3; c++ )
					{
						TINT   aiBones[ 4 ];
						TBOOL  abAnim[ 4 ];
						TFLOAT aflWeights[ 4 ];
						fnVertexBones( vecTris[ t * 3 + c ], aiBones, abAnim, aflWeights );

						for ( TINT s = 0; s < 4; s++ )
						{
							if ( !abAnim[ s ] ) continue;

							TBOOL bSeen = TFALSE;
							for ( TINT b = 0; b < iNumTriBones; b++ ) if ( aiTriBones[ b ] == aiBones[ s ] ) { bSeen = TTRUE; break; }
							if ( !bSeen ) aiTriBones[ iNumTriBones++ ] = aiBones[ s ];
						}
					}

					// How many of this triangle's bones are new to the group?
					auto fnBoneInGroup = [ & ]( TINT iBone ) -> TBOOL {
						for ( TINT g : vecGroupBones ) if ( g == iBone ) return TTRUE;
						return TFALSE;
					};

					TINT iNewBones = 0;
					for ( TINT b = 0; b < iNumTriBones; b++ ) if ( !fnBoneInGroup( aiTriBones[ b ] ) ) iNewBones++;

					if ( !vecGroup.empty() && TINT( vecGroupBones.size() ) + iNewBones > SKINNED_SUBMESH_MAX_BONES )
						fnFlushGroup();

					for ( TINT b = 0; b < iNumTriBones; b++ ) if ( !fnBoneInGroup( aiTriBones[ b ] ) ) vecGroupBones.push_back( aiTriBones[ b ] );

					vecGroup.push_back( TUINT( t ) );
				}

				fnFlushGroup();
			}

			TASSERT( pMesh->vecSubMeshes.Size() > 0 );
			auto pHeadSubMesh = pMesh->vecSubMeshes.Front();

			// Send vertex data to the GPU
			pMesh->oVertexBuffer.SetData( vecVertices.Begin(), vecVertices.Size() * sizeof( SkinMesh::SkinVertex ), GL_STATIC_DRAW );
			pHeadSubMesh->uiNumAllocatedVertices = vecVertices.Size();

			for ( TINT v = 0; v < TINT( vecVertices.Size() ); v++ )
				vecLODPositions.PushBack( vecVertices[ v ].Position );
		}

		if ( !vecLODPositions.IsEmpty() )
			pModel->aLODs[ iLODIdx ].BoundingSphere = ModelLoader_ComputeBoundingSphere( vecLODPositions );
	}

	return pModel;
}

Toshi::T2SharedPtr<ResourceLoader::Model> ResourceLoader::Model_LoadWorld_GLTF( Toshi::T2StringView pchFilePath, ModelType a_eModelType )
{
	TINFO( "Importing %s model from GLTF file: %s\n", GetModelTypeName( a_eModelType ), pchFilePath.Get() );

	T2SharedPtr<ResourceLoader::Model> pModel = T2SharedPtr<ResourceLoader::Model>::New();

	tinygltf::Model    gltfModel;
	tinygltf::TinyGLTF gltfLoader;
	std::string        strError, strWarning;

	if ( !gltfLoader.LoadASCIIFromFile( &gltfModel, &strError, &strWarning, pchFilePath.Get() ) )
	{
		TERROR( "Failed to load world GLTF: %s\n", strError.c_str() );
		return {};
	}

	pModel->eModelType        = a_eModelType;
	pModel->pTRB              = TNULL;
	pModel->iLODCount         = 1;
	pModel->fRenderDistance   = 50.0f;
	pModel->bAnimationsLoaded = TFALSE;
	pModel->pSkeleton         = TNULL;
	ModelLoader_InitLODDistances( pModel->aLODDistances );

	// Collect renderable meshes (skip collision helpers)
	T2DynamicVector<TINT> vecMeshes;
	for ( TSIZE i = 0; i < gltfModel.meshes.size(); i++ )
	{
		auto& rMesh = gltfModel.meshes[ i ];
		if ( rMesh.primitives.empty() ) continue;

		const TINT iMat = rMesh.primitives[ 0 ].material;
		if ( iMat >= 0 && iMat < TINT( gltfModel.materials.size() ) && gltfModel.materials[ iMat ].name.rfind( "Collision_", 0 ) == 0 ) continue;

		vecMeshes.PushBack( TINT( i ) );
	}

	TModelLOD& rLOD  = pModel->aLODs[ 0 ];
	rLOD.iNumMeshes  = vecMeshes.Size();
	rLOD.ppMeshes    = new TMesh*[ TMath::Max( 1, vecMeshes.Size() ) ];

	MaterialCache             matCache;
	T2DynamicVector<TVector3> vecAllPositions;

	auto fnAccessor = [ & ]( TINT iAcc, const TBYTE*& rpData, TUINT& ruiStride, TUINT& ruiCount, TINT& riCompType, TINT& riNumComp ) {
		auto& rAcc  = gltfModel.accessors[ iAcc ];
		auto& rView = gltfModel.bufferViews[ rAcc.bufferView ];
		rpData      = gltfModel.buffers[ rView.buffer ].data.data() + rView.byteOffset + rAcc.byteOffset;
		riCompType  = rAcc.componentType;
		riNumComp   = tinygltf::GetNumComponentsInType( rAcc.type );
		ruiCount    = TUINT( rAcc.count );
		const TUINT uiElem = TUINT( tinygltf::GetComponentSizeInBytes( rAcc.componentType ) ) * riNumComp;
		ruiStride   = rView.byteStride ? rView.byteStride : uiElem;
	};

	for ( TINT m = 0; m < vecMeshes.Size(); m++ )
	{
		auto& rMesh = gltfModel.meshes[ vecMeshes[ m ] ];
		auto& rPrim = rMesh.primitives[ 0 ];

		WorldMesh* pMesh = g_pWorldShader->CreateMesh();
		rLOD.ppMeshes[ m ] = pMesh;

		const TINT   iMat        = rPrim.material;
		std::string  strMatName  = ( iMat >= 0 ) ? gltfModel.materials[ iMat ].name : std::string( "NoMaterial" );
		const TCHAR* pchTexture  = "NoTexture";
		if ( iMat >= 0 && gltfModel.materials[ iMat ].pbrMetallicRoughness.baseColorTexture.index >= 0 )
			pchTexture = gltfModel.images[ gltfModel.textures[ gltfModel.materials[ iMat ].pbrMetallicRoughness.baseColorTexture.index ].source ].uri.c_str();

		T2SharedPtr<WorldMaterial> pMaterial;
		if ( auto pCached = matCache.Find( strMatName.c_str() ) )
			pMaterial = pCached;

		if ( !pMaterial )
		{
			pMaterial     = g_pWorldShader->CreateMaterial();
			auto pTexture = Resource::StreamedTexture_FindOrCreateDummy( TPS8D( pchTexture ) );
			pMaterial->SetTexture( pTexture );
			pMaterial->SetName( strMatName.c_str() );
			pModel->vecUsedTextures.PushBack( pTexture );
			matCache.Touch( strMatName.c_str(), pMaterial );
		}

		pMesh->SetName( strMatName.c_str() );
		pMesh->SetMaterialName( strMatName.c_str() );
		pMesh->SetMaterial( pMaterial );

		const TBYTE* pPos = TNULL; TUINT uiPosStride = 0, uiCount = 0; TINT iPosCT = 0, iPosNC = 0;
		fnAccessor( rPrim.attributes[ "POSITION" ], pPos, uiPosStride, uiCount, iPosCT, iPosNC );

		const TBYTE* pNrm = TNULL; TUINT uiNrmStride = 0, uiNrmCount = 0; TINT iNrmCT = 0, iNrmNC = 0;
		const TBOOL  bHasNrm = rPrim.attributes.count( "NORMAL" ) != 0;
		if ( bHasNrm ) fnAccessor( rPrim.attributes[ "NORMAL" ], pNrm, uiNrmStride, uiNrmCount, iNrmCT, iNrmNC );

		auto fnDecodeColor = [ & ]( const TCHAR* pszAttr, T2DynamicVector<TVector3>& rOut, TINT& riNumComp, TBOOL& rbNonWhite ) -> TBOOL {
			riNumComp  = 0;
			rbNonWhite = TFALSE;
			if ( rPrim.attributes.count( pszAttr ) == 0 ) return TFALSE;

			const TBYTE* pData = TNULL; TUINT uiStride = 0, uiCnt = 0; TINT iCT = 0, iNC = 0;
			fnAccessor( rPrim.attributes[ pszAttr ], pData, uiStride, uiCnt, iCT, iNC );
			riNumComp = iNC;

			rOut.SetSize( uiCnt );
			for ( TUINT v = 0; v < uiCnt; v++ )
			{
				const TBYTE* p        = pData + uiStride * v;
				TFLOAT       rgb[ 3 ] = { 1.0f, 1.0f, 1.0f };
				for ( TINT c = 0; c < 3 && c < iNC; c++ )
				{
					switch ( iCT )
					{
						case TINYGLTF_COMPONENT_TYPE_FLOAT:          rgb[ c ] = ( (const TFLOAT*)p )[ c ]; break;
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  rgb[ c ] = ( (const TUINT8*)p )[ c ] / 255.0f; break;
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: rgb[ c ] = ( (const TUINT16*)p )[ c ] / 65535.0f; break;
						default: break;
					}
				}
				rOut[ v ] = TVector3( rgb[ 0 ], rgb[ 1 ], rgb[ 2 ] );
				if ( rgb[ 0 ] < 0.999f || rgb[ 1 ] < 0.999f || rgb[ 2 ] < 0.999f ) rbNonWhite = TTRUE;
			}
			return TTRUE;
		};

		T2DynamicVector<TVector3> vecColors;
		vecColors.SetSize( uiCount );
		for ( TUINT v = 0; v < uiCount; v++ ) vecColors[ v ] = TVector3( 1.0f, 1.0f, 1.0f );

		{
			T2DynamicVector<TVector3> aCandCols[ 2 ];
			TINT                      aCandNumComp[ 2 ]  = { 0, 0 };
			TBOOL                     aCandNonWhite[ 2 ] = { TFALSE, TFALSE };
			const TBOOL               abHas[ 2 ]         = {
                fnDecodeColor( "COLOR_0", aCandCols[ 0 ], aCandNumComp[ 0 ], aCandNonWhite[ 0 ] ),
                fnDecodeColor( "COLOR_1", aCandCols[ 1 ], aCandNumComp[ 1 ], aCandNonWhite[ 1 ] ),
            };

			TINT iBest = -1, iBestScore = -1;
			for ( TINT i = 0; i < 2; i++ )
			{
				if ( !abHas[ i ] || aCandCols[ i ].Size() != uiCount ) continue;
				const TINT iScore = ( aCandNumComp[ i ] >= 4 ? 2 : 0 ) + ( aCandNonWhite[ i ] ? 1 : 0 );
				if ( iScore > iBestScore ) { iBestScore = iScore; iBest = i; }
			}

			if ( iBest != -1 )
				for ( TUINT v = 0; v < uiCount; v++ ) vecColors[ v ] = aCandCols[ iBest ][ v ];
		}

		const TBYTE* pUV = TNULL; TUINT uiUVStride = 0, uiUVCount = 0; TINT iUVCT = 0, iUVNC = 0;
		const TBOOL  bHasUV = rPrim.attributes.count( "TEXCOORD_0" ) != 0;
		if ( bHasUV ) fnAccessor( rPrim.attributes[ "TEXCOORD_0" ], pUV, uiUVStride, uiUVCount, iUVCT, iUVNC );

		T2DynamicVector<WorldMesh::WorldVertex> vecVertices;
		vecVertices.SetSize( uiCount );
		for ( TUINT v = 0; v < uiCount; v++ )
		{
			WorldMesh::WorldVertex& rVtx = vecVertices[ v ];
			rVtx.Position = *(const TVector3*)( pPos + uiPosStride * v );
			rVtx.Normal   = bHasNrm ? *(const TVector3*)( pNrm + uiNrmStride * v ) : TVector3( 0.0f, 1.0f, 0.0f );
			rVtx.UV       = bHasUV ? *(const TVector2*)( pUV + uiUVStride * v ) : TVector2( 0.0f, 0.0f );
			rVtx.Color    = vecColors[ v ];

			vecAllPositions.PushBack( rVtx.Position );
		}

		const TBYTE* pIdx = TNULL; TUINT uiIdxStride = 0, uiNumIndices = 0; TINT iIdxCT = 0, iIdxNC = 0;
		fnAccessor( rPrim.indices, pIdx, uiIdxStride, uiNumIndices, iIdxCT, iIdxNC );

		auto fnReadIndex = [ & ]( TUINT j ) -> TUINT16 {
			return ( iIdxCT == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT )
			           ? *(const TUINT16*)( pIdx + uiIdxStride * j )
			           : TUINT16( *(const TUINT32*)( pIdx + uiIdxStride * j ) );
		};

		T2DynamicVector<TUINT16> vecIndices;
		if ( rPrim.mode == TINYGLTF_MODE_TRIANGLES )
		{
			T2DynamicVector<TUINT16> vecTris;
			vecTris.SetSize( uiNumIndices );
			for ( TUINT j = 0; j < uiNumIndices; j++ ) vecTris[ j ] = fnReadIndex( j );

			// Merged world meshes can be disconnected, so stitch every piece into a
			// single strip and let the vertices get reordered too
			std::vector<TUINT16> vecStrip;
			MeshOptimize::StripifyTriangleList(
			    vecTris.Begin(), vecTris.Size(),
			    vecVertices.Begin(), vecVertices.Size(), sizeof( WorldMesh::WorldVertex ),
			    vecStrip );

			vecIndices.SetSize( TUINT( vecStrip.size() ) );
			if ( !vecStrip.empty() )
				TUtil::MemCopy( vecIndices.Begin(), vecStrip.data(), vecStrip.size() * sizeof( TUINT16 ) );
		}
		else
		{
			vecIndices.SetSize( uiNumIndices );
			for ( TUINT j = 0; j < uiNumIndices; j++ ) vecIndices[ j ] = fnReadIndex( j );
		}

		pMesh->oVertexBuffer = T2Render::CreateVertexBuffer( vecVertices.Begin(), vecVertices.Size() * sizeof( WorldMesh::WorldVertex ), GL_STATIC_DRAW );

		auto& rSubMesh        = pMesh->vecSubMeshes.PushBack();
		rSubMesh.uiNumIndices = vecIndices.Size();
		rSubMesh.uiNumVertices = vecVertices.Size();

		T2VertexArray::Unbind();
		rSubMesh.oIndexBuffer  = T2Render::CreateIndexBuffer( vecIndices.Begin(), vecIndices.Size(), GL_STATIC_DRAW );
		rSubMesh.oVertexArray  = T2Render::CreateVertexArray( pMesh->oVertexBuffer, rSubMesh.oIndexBuffer );
		rSubMesh.oVertexArray.Bind();
		rSubMesh.oVertexArray.GetVertexBuffer().SetAttribPointer( 0, 3, GL_FLOAT, sizeof( WorldMesh::WorldVertex ), offsetof( WorldMesh::WorldVertex, Position ) );
		rSubMesh.oVertexArray.GetVertexBuffer().SetAttribPointer( 1, 3, GL_FLOAT, sizeof( WorldMesh::WorldVertex ), offsetof( WorldMesh::WorldVertex, Normal ) );
		rSubMesh.oVertexArray.GetVertexBuffer().SetAttribPointer( 2, 3, GL_FLOAT, sizeof( WorldMesh::WorldVertex ), offsetof( WorldMesh::WorldVertex, Color ) );
		rSubMesh.oVertexArray.GetVertexBuffer().SetAttribPointer( 3, 2, GL_FLOAT, sizeof( WorldMesh::WorldVertex ), offsetof( WorldMesh::WorldVertex, UV ) );
	}

	pModel->aLODs[ 0 ].BoundingSphere = ModelLoader_ComputeBoundingSphere( vecAllPositions );

	// World props carry a skeleton too (rigid root/instance bone). Load it so OnSaveWorld
	// can write it back, else the game builds a null skeleton instance and crashes on render
	T2Map<TINT, TINT> mapWorldBones;
	ModelLoader_LoadGLTFSkeleton( gltfModel, pModel.Get(), mapWorldBones, pchFilePath );

	// World props (nb_/bn_ barn objects, carracefinish, gates, ...) carry collision too.
	// Load it so OnSave writes the Collision mesh + baked CollisionTree, else the re-saved
	// TRB has 0 collision meshes and the object loses in-game collision (and the game can
	// crash where it relies on it, e.g. the car race finish line)
	T2DynamicVector<TBOOL> vecCollisionMeshes;
	ModelLoader_BuildGLTFCollisionMask( gltfModel, vecCollisionMeshes );
	ModelLoader_LoadGLTFCollisionMeshes( gltfModel, pModel.Get(), vecCollisionMeshes );

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

void ResourceLoader::ModelLoader_SetAnimationFilter( const AnimationFilter* pFilter )
{
	s_pAnimationFilter = pFilter;
}

void ResourceLoader::ModelLoader_SetBoneFilter( const BoneFilter* pFilter )
{
	s_pBoneFilter = pFilter;
}

ResourceLoader::Model::Model()
{
	iLODCount           = 0;
	fRenderDistance     = 50.0f;
	iNumCollisionMeshes = 0;
	pSkeleton           = TNULL;
	pCollisionMeshes    = TNULL;
	pTRB                = TNULL;

	eModelType = ModelType::None;

	ModelLoader_InitLODDistances( aLODDistances );
}

ResourceLoader::Model::~Model()
{
	for ( TINT i = 0; i < iLODCount; i++ )
	{
		if ( aLODs[ i ].ppMeshes )
		{
			for ( TINT k = 0; k < aLODs[ i ].iNumMeshes; k++ )
			{
				if ( aLODs[ i ].ppMeshes[ k ] )
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

	delete[] pCollisionMeshes;
	pCollisionMeshes = TNULL;
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
