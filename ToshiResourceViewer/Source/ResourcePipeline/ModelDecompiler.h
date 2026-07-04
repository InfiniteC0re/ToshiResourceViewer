#pragma once

#include <Toshi/TString8.h>

namespace ResourcePipeline
{

// Decompiles TRB resources (models and/or matlibs) into GLTF + XML. With bMerge,
// duplicate models are folded into a shared GLTF referenced by per-model XMLs
struct DecompileOptions
{
	Toshi::TString8 strInputPath;   // .trb file, or a directory (bAll)
	Toshi::TString8 strOutputPath;
	Toshi::TString8 strInputName;   // input file name (used for the single-file path)
	TBOOL           bMerge     = TFALSE;
	TBOOL           bModels    = TFALSE;
	TBOOL           bMatlibs   = TFALSE;
	TBOOL           bAll       = TFALSE;
	TBOOL           bRecursive = TFALSE;
};

void DecompileResources( const DecompileOptions& rOptions );

} // namespace ResourcePipeline
