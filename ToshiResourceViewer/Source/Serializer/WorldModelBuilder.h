#pragma once

#include <Plugins/PTRB.h>

namespace ResourceLoader
{
class Model;
}

//-----------------------------------------------------------------------------
// Serializes a world model (ModelType::World) into the TRB "Database" structure
// the engine expects: one World -> one Cell holding every mesh, with a single
// leaf sphere-tree. The Cell metadata is zeroed (matching the reference TMDL
// compiler) and per-mesh/whole-cell bounding spheres are computed here
//-----------------------------------------------------------------------------
namespace WorldModelBuilder
{

// Writes the "Database" symbol and the world geometry it points at (materials and the
// LOD Header are the caller's job). Meshes are split to stay under the 16-bit vertex
// limit, and tiled into ~a_fChunkSize ground cells when a_fChunkSize > 0. Returns TFALSE
// if the model has no world meshes
TBOOL WriteWorldModel( PTRB* a_pTRB, PTRBSections::MemoryStream* a_pStream, PTRBSymbols* a_pSymbols, ResourceLoader::Model* a_pModel, TFLOAT a_fChunkSize );

} // namespace WorldModelBuilder
