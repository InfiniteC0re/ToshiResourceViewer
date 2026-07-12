#include "pch.h"
#include "MeshOptimize.h"

#include <meshoptimizer.h>

#include <cstring>

namespace MeshOptimize
{

void StripifyTriangleList(
    const TUINT16*        a_pTriIndices,
    TUINT                 a_uiTriIndexCount,
    void*                 a_pVertices,
    TUINT                 a_uiVertexCount,
    TUINT                 a_uiVertexStride,
    std::vector<TUINT16>& a_rOutStrip )
{
	a_rOutStrip.clear();

	if ( a_uiTriIndexCount == 0 )
		return;

	// meshoptimizer wants 32-bit indices; our meshes stay under the 16-bit cap so
	// this round-trips cleanly
	std::vector<unsigned int> vecList( a_uiTriIndexCount );
	for ( TUINT i = 0; i < a_uiTriIndexCount; i++ )
		vecList[ i ] = a_pTriIndices[ i ];

	std::vector<unsigned int> vecOptimized( a_uiTriIndexCount );
	meshopt_optimizeVertexCacheStrip( vecOptimized.data(), vecList.data(), a_uiTriIndexCount, a_uiVertexCount );

	// Reorder the vertex block for fetch locality while we still have a triangle
	// list - it renumbers the vertices but keeps the triangle order, so the cache
	// pass above survives into the strip
	if ( a_pVertices && a_uiVertexCount != 0 && a_uiVertexStride != 0 )
	{
		std::vector<char> vecReordered( (size_t)a_uiVertexCount * a_uiVertexStride );
		const size_t      uiUnique = meshopt_optimizeVertexFetch(
            vecReordered.data(), vecOptimized.data(), a_uiTriIndexCount, a_pVertices, a_uiVertexCount, a_uiVertexStride );

		std::memcpy( a_pVertices, vecReordered.data(), uiUnique * a_uiVertexStride );
	}

	// restart 0 = stitch everything into one strip with degenerate triangles
	std::vector<unsigned int> vecStrip( meshopt_stripifyBound( a_uiTriIndexCount ) );
	const size_t              uiStripCount = meshopt_stripify( vecStrip.data(), vecOptimized.data(), a_uiTriIndexCount, a_uiVertexCount, 0u );

	a_rOutStrip.resize( uiStripCount );
	for ( size_t i = 0; i < uiStripCount; i++ )
		a_rOutStrip[ i ] = TUINT16( vecStrip[ i ] );
}

} // namespace MeshOptimize
