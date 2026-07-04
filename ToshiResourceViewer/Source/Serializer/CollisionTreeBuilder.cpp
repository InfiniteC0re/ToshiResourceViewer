#include "pch.h"
#include "CollisionTreeBuilder.h"

#include <Opcode.h>

TOSHI_NAMESPACE_USING

namespace CollisionTreeBuilder
{

struct TRBNoLeafNode
{
	TVector3       m_Center;
	TVector3       m_Extents;
	TRBNoLeafNode* m_pPos;
	TRBNoLeafNode* m_pNeg;
};

struct TRBNoLeafTree
{
	TUINT32        m_uiNbNodes;
	TRBNoLeafNode* m_pNodes;
};

void WriteCollisionTree(
    PTRB*                       a_pOutTRB,
    PTRBSections::MemoryStream* a_pMemStream,
    PTRBSymbols*                a_pSYMB,
    const TVector3*             a_pVertices,
    TUINT                       a_uiNumVertices,
    const TUINT16*              a_pIndices,
    TUINT                       a_uiNumIndices )
{
	if ( a_uiNumVertices == 0 || a_uiNumIndices < 6 ) return;

	Opcode::MeshInterface meshInterface;
	meshInterface.SetNbTriangles( a_uiNumIndices / 3 );
	meshInterface.SetNbVertices( a_uiNumVertices );
	meshInterface.SetPointers(
	    TREINTERPRETCAST( const IceMaths::IndexedTriangle*, a_pIndices ),
	    TREINTERPRETCAST( const IceMaths::Point*, a_pVertices )
	);

	Opcode::OPCODECREATE oCreate;
	oCreate.mIMesh        = &meshInterface;
	oCreate.mNoLeaf       = TTRUE;
	oCreate.mQuantized    = TFALSE;
	oCreate.mKeepOriginal = TFALSE;
	oCreate.mCanRemap     = TFALSE;

	Opcode::Model oModel;
	if ( !oModel.Build( oCreate ) ) return;

	const Opcode::AABBNoLeafTree* pTree = oModel.GetTree();
	if ( !pTree ) return;

	const Opcode::AABBNoLeafNode* pNodes    = pTree->GetNodes();
	const udword                  uiNbNodes = pTree->GetNbNodes();
	if ( !pNodes || uiNbNodes == 0 ) return;

	auto pTRBTree         = a_pMemStream->Alloc<TRBNoLeafTree>();
	pTRBTree->m_uiNbNodes = a_pOutTRB->ConvertEndianess( uiNbNodes );

	auto pTRBNodes = a_pMemStream->Alloc<TRBNoLeafNode>( &pTRBTree->m_pNodes, uiNbNodes );

	for ( udword i = 0; i < uiNbNodes; i++ )
	{
		const Opcode::AABBNoLeafNode& rSrc = pNodes[ i ];
		auto                          pDst = pTRBNodes + i;

		pDst->m_Center  = a_pOutTRB->ConvertEndianess( TVector3( rSrc.mAABB.mCenter.x, rSrc.mAABB.mCenter.y, rSrc.mAABB.mCenter.z ) );
		pDst->m_Extents = a_pOutTRB->ConvertEndianess( TVector3( rSrc.mAABB.mExtents.x, rSrc.mAABB.mExtents.y, rSrc.mAABB.mExtents.z ) );

		const udword    aData[ 2 ]   = { rSrc.mPosData, rSrc.mNegData };
		TRBNoLeafNode** apField[ 2 ] = { &pDst->m_pPos, &pDst->m_pNeg };

		for ( TINT c = 0; c < 2; c++ )
		{
			if ( aData[ c ] & 1 )
			{
				// Leaf value, written inline with no relocation
				*TREINTERPRETCAST( TUINT32*, apField[ c ] ) = a_pOutTRB->ConvertEndianess( TUINT32( aData[ c ] ) );
			}
			else
			{
				const Opcode::AABBNoLeafNode* pChild  = TREINTERPRETCAST( const Opcode::AABBNoLeafNode*, aData[ c ] );
				const udword                  uiChild = udword( pChild - pNodes );
				a_pMemStream->WritePointer( apField[ c ], pTRBNodes + uiChild );
			}
		}
	}

	a_pSYMB->Add( a_pMemStream, "CollisionTree", pTRBTree.get() );
}

} // namespace CollisionTreeBuilder
