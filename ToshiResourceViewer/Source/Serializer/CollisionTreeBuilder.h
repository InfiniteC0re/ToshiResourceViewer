#pragma once
#include <Plugins/PTRB.h>
#include <Math/TVector3.h>

namespace CollisionTreeBuilder
{
	void WriteCollisionTree(
	    PTRB*                       a_pOutTRB,
	    PTRBSections::MemoryStream* a_pMemStream,
	    PTRBSymbols*                a_pSYMB,
	    const Toshi::TVector3*      a_pVertices,
	    TUINT                       a_uiNumVertices,
	    const TUINT16*              a_pIndices,
	    TUINT                       a_uiNumIndices );
}
