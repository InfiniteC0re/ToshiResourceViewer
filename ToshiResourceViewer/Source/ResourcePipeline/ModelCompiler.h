#pragma once

#include <Toshi/TString8.h>

namespace ResourcePipeline
{

// Compiles model XML/GLTF into TRB. Models sharing a keylib (a .txt list, or all
// lists in a directory) are compiled together so they share one TKL
struct CompileOptions
{
	Toshi::TString8 strInputPath;   // .xml/.gltf file, a .txt list (bList), or a directory of lists (bAll)
	Toshi::TString8 strOutputPath;
	Toshi::TString8 strTKLName;     // overrides the keylib name when set
	Toshi::TString8 strForcedName;  // overrides the output file name for a single compile
	TBOOL           bCompress = TFALSE;
	TBOOL           bList     = TFALSE;
	TBOOL           bAll      = TFALSE;
};

void CompileModels( const CompileOptions& rOptions );

} // namespace ResourcePipeline
