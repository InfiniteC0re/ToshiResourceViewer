#pragma once

#include <Toshi/Toshi.h>

#include <vector>

namespace MeshOptimize
{

// Turns a 16-bit triangle list into a single stitched triangle strip for the
// renderer, running meshoptimizer's vertex-cache, vertex-fetch and stripify passes.
//
// When a_pVertices is set, that vertex block is reordered in place for fetch
// locality and the strip indices are remapped to match - so a_pTriIndices must be
// 0-based into the block. Pass TNULL to skip the reorder and only stripify.
void StripifyTriangleList(
    const TUINT16*        a_pTriIndices,
    TUINT                 a_uiTriIndexCount,
    void*                 a_pVertices,
    TUINT                 a_uiVertexCount,
    TUINT                 a_uiVertexStride,
    std::vector<TUINT16>& a_rOutStrip );

} // namespace MeshOptimize
