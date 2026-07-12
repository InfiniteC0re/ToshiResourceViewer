#include "pch.h"
#include "WorldModelBuilder.h"
#include "Shader/WorldShader.h"
#include "ResourceLoader/ModelLoader.h"
#include "MeshOptimize/MeshOptimize.h"

#include <Render/TTMDWin.h>
#include <Math/TSphere.h>

#include <vector>
#include <array>
#include <algorithm>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

namespace WorldModelBuilder
{

//-----------------------------------------------------------------------------
// World model TRB binary layout (must match the loader in ModelLoader_Win.cpp)
//-----------------------------------------------------------------------------
struct CellMesh
{
	void*    pMesh;          // runtime only, written as null
	TUINT32  uiNumIndices;
	TUINT32  uiNumVertices1;
	TUINT16  uiNumVertices2;
	TCHAR*   szMaterialName;
	void*    pVertices;      // WorldMesh::WorldVertex*
	TUINT16* pIndices;
};

struct CellMeshSphere
{
	TSphere   m_BoundingSphere;
	CellMesh* m_pCellMesh;
};

struct CellSphereTreeBranchNode
{
	TSphere                   m_BoundingSphere;
	CellSphereTreeBranchNode* m_pRight; // null marks a leaf; leaf data follows inline
};

struct Cell
{
	TUINT32                   uiFlags;
	TCHAR                     UNKNOWNDATA1[ 108 ];
	TINT                      m_iSomeCount;
	TCHAR                     UNKNOWNDATA2[ 12 ];
	void*                     pNode; // runtime only, written as null
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

// Center + radius covering all given vertices (Ritter would be tighter, but a
// simple box-center sphere is plenty for the engine's coarse cull)
static TSphere ComputeSphere( const WorldMesh::WorldVertex* a_pVertices, TUINT a_uiCount )
{
	if ( a_uiCount == 0 ) return TSphere( 0.0f, 0.0f, 0.0f, 0.0f );

	TVector3 vMin = a_pVertices[ 0 ].Position;
	TVector3 vMax = a_pVertices[ 0 ].Position;
	for ( TUINT i = 1; i < a_uiCount; i++ )
	{
		const TVector3& p = a_pVertices[ i ].Position;
		vMin.x = TMath::Min( vMin.x, p.x ); vMin.y = TMath::Min( vMin.y, p.y ); vMin.z = TMath::Min( vMin.z, p.z );
		vMax.x = TMath::Max( vMax.x, p.x ); vMax.y = TMath::Max( vMax.y, p.y ); vMax.z = TMath::Max( vMax.z, p.z );
	}

	const TVector3 vCenter( ( vMin.x + vMax.x ) * 0.5f, ( vMin.y + vMax.y ) * 0.5f, ( vMin.z + vMax.z ) * 0.5f );

	TFLOAT fRadiusSq = 0.0f;
	for ( TUINT i = 0; i < a_uiCount; i++ )
	{
		const TVector3& p = a_pVertices[ i ].Position;
		const TVector3  d( p.x - vCenter.x, p.y - vCenter.y, p.z - vCenter.z );
		fRadiusSq = TMath::Max( fRadiusSq, d.x * d.x + d.y * d.y + d.z * d.z );
	}

	return TSphere( vCenter.x, vCenter.y, vCenter.z, fRadiusSq > 0.0f ? TMath::Sqrt( fRadiusSq ) : 0.0f );
}

struct MeshData
{
	T2DynamicVector<WorldMesh::WorldVertex> vecVertices;
	T2DynamicVector<TUINT16>                vecIndices;
	TString8                                strMaterial;
	TSphere                                 oSphere;
};

using Triangle = std::array<TUINT16, 3>;

// Builds a chunk (own vertex subset + triangle strip) from a triangle set that
// references a_rSrc's vertex pool by global index
static void BuildChunk( MeshData& a_rSrc, const std::vector<Triangle>& a_rTris, T2DynamicVector<MeshData>& a_rOut )
{
	if ( a_rTris.empty() ) return;

	MeshData&                rChunk = a_rOut.PushBack();
	T2Map<TUINT16, TUINT16>  mapLocal;
	std::vector<TUINT16>     vecTriList;
	vecTriList.reserve( a_rTris.size() * 3 );

	for ( const Triangle& rTri : a_rTris )
	{
		for ( TINT c = 0; c < 3; c++ )
		{
			const TUINT16 uiGlobal = rTri[ c ];
			auto          it       = mapLocal.Find( uiGlobal );
			TUINT16       uiLocal;
			if ( it == mapLocal.End() )
			{
				uiLocal = TUINT16( mapLocal.Size() );
				mapLocal.Insert( uiGlobal, uiLocal );
				rChunk.vecVertices.PushBack( a_rSrc.vecVertices[ uiGlobal ] );
			}
			else
			{
				uiLocal = it->second;
			}
			vecTriList.push_back( uiLocal );
		}
	}

	// Stitch the chunk's (possibly disconnected) pieces into one strip. The chunk
	// owns its verts and indices, so the optimizer can reorder the verts too
	std::vector<TUINT16> vecStrip;
	MeshOptimize::StripifyTriangleList(
	    vecTriList.data(), TUINT( vecTriList.size() ),
	    rChunk.vecVertices.Begin(), rChunk.vecVertices.Size(), sizeof( WorldMesh::WorldVertex ),
	    vecStrip );

	rChunk.vecIndices.SetSize( TUINT( vecStrip.size() ) );
	if ( !vecStrip.empty() )
		TUtil::MemCopy( rChunk.vecIndices.Begin(), vecStrip.data(), vecStrip.size() * sizeof( TUINT16 ) );

	rChunk.strMaterial = a_rSrc.strMaterial;
	rChunk.oSphere     = ComputeSphere( rChunk.vecVertices.Begin(), rChunk.vecVertices.Size() );
}

// Recursively splits triangles into chunks that stay within the 16-bit vertex
// limit and (when a_fChunkSize > 0) within a_fChunkSize on the ground plane, so
// a big merged surface is tiled like the original terrain for coarse culling
static void SplitTriangles( MeshData& a_rSrc, std::vector<Triangle>& a_rTris, TFLOAT a_fChunkSize, T2DynamicVector<MeshData>& a_rOut )
{
	constexpr TUINT MAX_CHUNK_VERTS = 60000; // headroom under the 16-bit index cap

	T2Map<TUINT16, TBOOL> setUnique;
	TFLOAT                fMinX = 1e30f, fMaxX = -1e30f, fMinZ = 1e30f, fMaxZ = -1e30f;
	for ( const Triangle& rTri : a_rTris )
	{
		for ( TINT c = 0; c < 3; c++ )
		{
			if ( setUnique.Find( rTri[ c ] ) == setUnique.End() ) setUnique.Insert( rTri[ c ], TTRUE );
			const TVector3& p = a_rSrc.vecVertices[ rTri[ c ] ].Position;
			fMinX = TMath::Min( fMinX, p.x ); fMaxX = TMath::Max( fMaxX, p.x );
			fMinZ = TMath::Min( fMinZ, p.z ); fMaxZ = TMath::Max( fMaxZ, p.z );
		}
	}

	const TBOOL bTooManyVerts = setUnique.Size() > MAX_CHUNK_VERTS;
	const TBOOL bTooBig       = a_fChunkSize > 0.0f && ( fMaxX - fMinX > a_fChunkSize || fMaxZ - fMinZ > a_fChunkSize );

	if ( ( !bTooManyVerts && !bTooBig ) || a_rTris.size() <= 1 )
	{
		BuildChunk( a_rSrc, a_rTris, a_rOut );
		return;
	}

	const TINT iAxis = ( fMaxX - fMinX >= fMaxZ - fMinZ ) ? 0 : 2; // 0 = X, 2 = Z (ground plane)
	std::sort( a_rTris.begin(), a_rTris.end(), [ & ]( const Triangle& a, const Triangle& b ) {
		auto ctr = [ & ]( const Triangle& t ) {
			const auto& v0 = a_rSrc.vecVertices[ t[ 0 ] ].Position;
			const auto& v1 = a_rSrc.vecVertices[ t[ 1 ] ].Position;
			const auto& v2 = a_rSrc.vecVertices[ t[ 2 ] ].Position;
			return ( iAxis == 0 ) ? ( v0.x + v1.x + v2.x ) : ( v0.z + v1.z + v2.z );
		};
		return ctr( a ) < ctr( b );
	} );

	const TSIZE           uiMid = a_rTris.size() / 2;
	std::vector<Triangle> vecLeft( a_rTris.begin(), a_rTris.begin() + uiMid );
	std::vector<Triangle> vecRight( a_rTris.begin() + uiMid, a_rTris.end() );
	SplitTriangles( a_rSrc, vecLeft, a_fChunkSize, a_rOut );
	SplitTriangles( a_rSrc, vecRight, a_fChunkSize, a_rOut );
}

// Unrolls a mesh's strip and re-tiles it into one or more chunks
static void ChunkMesh( MeshData& a_rSrc, TFLOAT a_fChunkSize, T2DynamicVector<MeshData>& a_rOut )
{
	std::vector<Triangle> vecTris;
	const TUINT16*        pStrip = a_rSrc.vecIndices.Begin();
	const TUINT           uiNum  = a_rSrc.vecIndices.Size();

	TUINT uiRunStart = 0;
	for ( TUINT i = 0; i + 2 < uiNum; i++ )
	{
		const TUINT16 a = pStrip[ i ], b = pStrip[ i + 1 ], c = pStrip[ i + 2 ];
		if ( a == 0xFFFF ) { uiRunStart = i + 1; continue; }
		if ( b == 0xFFFF ) { uiRunStart = i + 2; continue; }
		if ( c == 0xFFFF ) { uiRunStart = i + 3; continue; }
		if ( a == b || b == c || a == c ) continue;
		vecTris.push_back( ( ( i - uiRunStart ) & 1 ) == 0 ? Triangle{ a, b, c } : Triangle{ b, a, c } );
	}

	if ( vecTris.empty() ) return;
	SplitTriangles( a_rSrc, vecTris, a_fChunkSize, a_rOut );
}

TBOOL WriteWorldModel( PTRB* a_pTRB, PTRBSections::MemoryStream* a_pStream, PTRBSymbols* a_pSymbols, ResourceLoader::Model* a_pModel, TFLOAT a_fChunkSize )
{
	// One LOD per TRB. A mesh-less world (skeleton/animation container, retail meshCount=0)
	// is still valid and handled below - don't early-out on an empty LOD
	TModelLOD& rLOD = a_pModel->aLODs[ 0 ];

	// Pull every world mesh off the GPU, then re-tile it into chunks (always within
	// the 16-bit vertex limit; into a_fChunkSize tiles when chunking is enabled)
	T2DynamicVector<MeshData> vecRaw;

	for ( TINT i = 0; i < rLOD.iNumMeshes; i++ )
	{
		WorldMesh* pMesh = TSTATICCAST( WorldMesh, rLOD.ppMeshes[ i ] );
		if ( pMesh->vecSubMeshes.IsEmpty() ) continue;

		GLint iVertexBufferSize = 0;
		pMesh->oVertexBuffer.GetParameter( GL_BUFFER_SIZE, iVertexBufferSize );
		const TUINT uiNumVertices = iVertexBufferSize / sizeof( WorldMesh::WorldVertex );
		if ( uiNumVertices == 0 ) continue;

		MeshData& rData = vecRaw.PushBack();
		rData.vecVertices.SetSize( uiNumVertices );
		pMesh->oVertexBuffer.GetSubData( rData.vecVertices.Begin(), 0, iVertexBufferSize );

		// A world mesh has one submesh sharing the whole vertex pool
		auto& rSubMesh = pMesh->vecSubMeshes[ 0 ];
		rData.vecIndices.SetSize( rSubMesh.uiNumIndices );
		rSubMesh.oIndexBuffer.GetSubData( rData.vecIndices.Begin(), 0, rSubMesh.uiNumIndices * sizeof( TUINT16 ) );

		TString8 strTexture;
		pMesh->GetMaterialInfo( rData.strMaterial, strTexture );
	}

	T2DynamicVector<MeshData> vecMeshes;
	T2_FOREACH( vecRaw, itRaw )
		ChunkMesh( *itRaw, a_fChunkSize, vecMeshes );

	const TUINT uiNumMeshes = vecMeshes.Size();

	// LOD Header: one WORLD-shaded LOD. The mesh count must match the chunk count
	// (not the pre-chunk count), or the loader truncates the tree traversal
	auto pTRBWinHeader            = a_pStream->Alloc<TTMDWin::TRBWinHeader>();
	pTRBWinHeader->m_iNumLODs     = a_pTRB->ConvertEndianess( 1 );
	pTRBWinHeader->m_fLODDistance = a_pTRB->ConvertEndianess( a_pModel->fRenderDistance );

	auto           pTRBLOD = a_pStream->Alloc<TTMDWin::TRBLODHeader>( 1 );
	const TVector4 vSphere = a_pModel->aLODs[ 0 ].BoundingSphere.AsVector4();
	// World and Grass share this layout and differ only by the shader enum
	const TTMDBase::SHADERTYPE eShader = ( a_pModel->eModelType == ResourceLoader::ModelType::Grass ) ? TTMDBase::SHADERTYPE_GRASS : TTMDBase::SHADERTYPE_WORLD;
	pTRBLOD->m_iMeshCount1 = a_pTRB->ConvertEndianess( TINT( uiNumMeshes ) );
	pTRBLOD->m_iMeshCount2 = a_pTRB->ConvertEndianess( 0 );
	pTRBLOD->m_eShader     = a_pTRB->ConvertEndianess( eShader );
	pTRBLOD->m_RenderVolume.Set(
	    a_pTRB->ConvertEndianess( vSphere.x ),
	    a_pTRB->ConvertEndianess( vSphere.y ),
	    a_pTRB->ConvertEndianess( vSphere.z ),
	    a_pTRB->ConvertEndianess( vSphere.w )
	);
	a_pSymbols->Add( a_pStream, "Header", pTRBWinHeader.get() );

	//-----------------------------------------------------------------------------
	// Database -> World[1] -> Cell[1]
	//-----------------------------------------------------------------------------
	auto pDatabase             = a_pStream->Alloc<WorldDatabase>();
	pDatabase->m_uiNumWorlds   = a_pTRB->ConvertEndianess( 1u );
	a_pStream->Alloc<World*>( &pDatabase->m_ppWorlds, 1 );

	auto pWorld = a_pStream->Alloc<World>( &pDatabase->m_ppWorlds[ 0 ] );

	// Mesh-less container: one world with zero cells (matches the retail bn_* containers).
	// The skeleton/animation the caller already wrote is the whole point of the model
	if ( uiNumMeshes == 0 )
	{
		pWorld->m_iNumCells = a_pTRB->ConvertEndianess( 0 );
		a_pStream->Alloc<Cell*>( &pWorld->m_ppCells, 1 ); // dummy slot, never read when numCells == 0
		a_pSymbols->Add( a_pStream, "Database", pDatabase.get() );
		return TTRUE;
	}

	pWorld->m_iNumCells = a_pTRB->ConvertEndianess( 1 );
	a_pStream->Alloc<Cell*>( &pWorld->m_ppCells, 1 );

	auto pCell = a_pStream->Alloc<Cell>( &pWorld->m_ppCells[ 0 ] );
	TUtil::MemClear( pCell.get(), sizeof( Cell ) ); // the metadata is opaque; zero it like TMDL does
	pCell->pNode       = TNULL;
	pCell->uiNumMeshes = a_pTRB->ConvertEndianess( uiNumMeshes );

	a_pStream->Alloc<CellMeshSphere*>( &pCell->ppCellMeshSpheres, uiNumMeshes );

	for ( TUINT i = 0; i < uiNumMeshes; i++ )
	{
		MeshData& rData = vecMeshes[ i ];

		auto pSphereEntry              = a_pStream->Alloc<CellMeshSphere>( &pCell->ppCellMeshSpheres[ i ] );
		pSphereEntry->m_BoundingSphere = rData.oSphere;

		auto pCellMesh            = a_pStream->Alloc<CellMesh>( &pSphereEntry->m_pCellMesh );
		pCellMesh->pMesh          = TNULL;
		pCellMesh->uiNumIndices   = a_pTRB->ConvertEndianess( rData.vecIndices.Size() );
		pCellMesh->uiNumVertices1 = a_pTRB->ConvertEndianess( rData.vecVertices.Size() );
		pCellMesh->uiNumVertices2 = a_pTRB->ConvertEndianess( TUINT16( rData.vecVertices.Size() ) );

		const TUINT uiNameLen = rData.strMaterial.Length() + 1;
		auto        pMatName  = a_pStream->AllocBytes( uiNameLen );
		T2String8::Copy( pMatName.get(), rData.strMaterial.GetString(), uiNameLen );
		a_pStream->WritePointer( &pCellMesh->szMaterialName, pMatName );

		auto pVertData = a_pStream->Alloc<WorldMesh::WorldVertex>( TREINTERPRETCAST( WorldMesh::WorldVertex**, &pCellMesh->pVertices ), rData.vecVertices.Size() );
		TUtil::MemCopy( pVertData.get(), rData.vecVertices.Begin(), rData.vecVertices.Size() * sizeof( WorldMesh::WorldVertex ) );

		auto pIdxData = a_pStream->Alloc<TUINT16>( &pCellMesh->pIndices, rData.vecIndices.Size() );
		TUtil::MemCopy( pIdxData.get(), rData.vecIndices.Begin(), rData.vecIndices.Size() * sizeof( TUINT16 ) );
	}

	//-----------------------------------------------------------------------------
	// Sphere tree: a top-down binary BVH. Each node holds its subtree's sphere; the left
	// child is this+1, the right is via m_pRight (null = leaf, mesh-index list inline)
	//-----------------------------------------------------------------------------
	auto fnBoundGroup = [ & ]( const std::vector<TUINT16>& rGroup ) -> TSphere {
		TVector3 mn( 1e30f, 1e30f, 1e30f ), mx( -1e30f, -1e30f, -1e30f );
		for ( TUINT16 idx : rGroup )
		{
			const TVector4 s = vecMeshes[ idx ].oSphere.AsVector4();
			mn.x = TMath::Min( mn.x, s.x - s.w ); mn.y = TMath::Min( mn.y, s.y - s.w ); mn.z = TMath::Min( mn.z, s.z - s.w );
			mx.x = TMath::Max( mx.x, s.x + s.w ); mx.y = TMath::Max( mx.y, s.y + s.w ); mx.z = TMath::Max( mx.z, s.z + s.w );
		}
		const TVector3 c( ( mn.x + mx.x ) * 0.5f, ( mn.y + mx.y ) * 0.5f, ( mn.z + mx.z ) * 0.5f );
		TFLOAT         fR = 0.0f;
		for ( TUINT16 idx : rGroup )
		{
			const TVector4 s   = vecMeshes[ idx ].oSphere.AsVector4();
			const TFLOAT   d2  = ( s.x - c.x ) * ( s.x - c.x ) + ( s.y - c.y ) * ( s.y - c.y ) + ( s.z - c.z ) * ( s.z - c.z );
			fR = TMath::Max( fR, ( d2 > 0.0f ? TMath::Sqrt( d2 ) : 0.0f ) + s.w );
		}
		return TSphere( c.x, c.y, c.z, fR );
	};

	constexpr TUINT MAX_LEAF_MESHES = 8;

	auto fnEmit = [ & ]( auto&& self, std::vector<TUINT16>& rGroup ) -> PTRBSections::MemoryStream::Ptr<CellSphereTreeBranchNode> {
		const TSphere oSphere = fnBoundGroup( rGroup );

		if ( rGroup.size() <= MAX_LEAF_MESHES )
		{
			auto pBranch              = a_pStream->Alloc<CellSphereTreeBranchNode>();
			pBranch->m_BoundingSphere = oSphere;
			pBranch->m_pRight         = TNULL;

			auto     pLeaf  = a_pStream->AllocBytes( sizeof( TUINT32 ) + TUINT( rGroup.size() ) * sizeof( TUINT16 ) );
			TUINT32* pCount = TREINTERPRETCAST( TUINT32*, pLeaf.get() );
			*pCount         = a_pTRB->ConvertEndianess( TUINT( rGroup.size() ) );
			TUINT16* pIdx   = TREINTERPRETCAST( TUINT16*, pCount + 1 );
			for ( TSIZE i = 0; i < rGroup.size(); i++ ) pIdx[ i ] = a_pTRB->ConvertEndianess( rGroup[ i ] );

			return pBranch;
		}

		// Split along the longest axis of the mesh centers, at the object median
		TVector3 cmn( 1e30f, 1e30f, 1e30f ), cmx( -1e30f, -1e30f, -1e30f );
		for ( TUINT16 idx : rGroup )
		{
			const TVector4 s = vecMeshes[ idx ].oSphere.AsVector4();
			cmn.x = TMath::Min( cmn.x, s.x ); cmn.y = TMath::Min( cmn.y, s.y ); cmn.z = TMath::Min( cmn.z, s.z );
			cmx.x = TMath::Max( cmx.x, s.x ); cmx.y = TMath::Max( cmx.y, s.y ); cmx.z = TMath::Max( cmx.z, s.z );
		}
		const TINT iAxis = ( cmx.x - cmn.x >= cmx.y - cmn.y && cmx.x - cmn.x >= cmx.z - cmn.z ) ? 0 : ( cmx.y - cmn.y >= cmx.z - cmn.z ? 1 : 2 );

		std::sort( rGroup.begin(), rGroup.end(), [ & ]( TUINT16 a, TUINT16 b ) {
			const TVector4 sa = vecMeshes[ a ].oSphere.AsVector4();
			const TVector4 sb = vecMeshes[ b ].oSphere.AsVector4();
			return ( ( iAxis == 0 ) ? sa.x : ( iAxis == 1 ? sa.y : sa.z ) ) < ( ( iAxis == 0 ) ? sb.x : ( iAxis == 1 ? sb.y : sb.z ) );
		} );

		const TSIZE          uiMid = rGroup.size() / 2;
		std::vector<TUINT16> vecLeft( rGroup.begin(), rGroup.begin() + uiMid );
		std::vector<TUINT16> vecRight( rGroup.begin() + uiMid, rGroup.end() );

		auto pBranch              = a_pStream->Alloc<CellSphereTreeBranchNode>();
		pBranch->m_BoundingSphere = oSphere;

		self( self, vecLeft ); // left child lands at this + 1
		auto pRight = self( self, vecRight );
		a_pStream->WritePointer( &pBranch->m_pRight, pRight );

		return pBranch;
	};

	std::vector<TUINT16> vecAllMeshes;
	for ( TUINT i = 0; i < uiNumMeshes; i++ ) vecAllMeshes.push_back( TUINT16( i ) );

	auto pTreeRoot = fnEmit( fnEmit, vecAllMeshes );
	a_pStream->WritePointer( &pCell->pTreeBranchNodes, pTreeRoot );

	a_pSymbols->Add( a_pStream, "Database", pDatabase.get() );
	return TTRUE;
}

} // namespace WorldModelBuilder
