#include "pch.h"
#include "AppHeadless.h"
#include "Application.h"
#include "ResourcePipeline/ModelCompiler.h"
#include "ResourcePipeline/ModelDecompiler.h"

#include <ToshiTools/T2CommandLine.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

void HeadlessMain( TINT argc, TCHAR** argv )
{
	TString8 strOutputPath    = g_pCmd->GetParameterValue( "-output" );
	TString8 strInputFilePath = g_pCmd->GetParameterValue( "-input" );

	if ( !strInputFilePath ) return;

	TString8   strInputFileDir;
	const TINT iLastSlashIndex = strInputFilePath.FindReverse( '\\' );
	strInputFileDir.Copy( strInputFilePath, iLastSlashIndex + 1 );

	if ( !strOutputPath ) strOutputPath = strInputFileDir;

	TString8 strInputFileName = ( iLastSlashIndex != -1 ) ? TString8( strInputFilePath.GetString( iLastSlashIndex + 1 ) ) : strInputFilePath;

	if ( g_pCmd->HasParameter( "-compile" ) )
	{
		ResourcePipeline::CompileOptions oOptions;
		oOptions.strInputPath  = strInputFilePath;
		oOptions.strOutputPath = strOutputPath;
		oOptions.strTKLName    = g_pCmd->GetParameterValue( "-tkl" );
		oOptions.strForcedName = g_pCmd->GetParameterValue( "-name" );
		oOptions.bCompress     = g_pCmd->HasParameter( "-compress" );
		oOptions.bList         = g_pCmd->HasParameter( "-list" );
		oOptions.bAll          = g_pCmd->HasParameter( "-all" );

		ResourcePipeline::CompileModels( oOptions );
	}
	else if ( g_pCmd->HasParameter( "-decompile" ) )
	{
		ResourcePipeline::DecompileOptions oOptions;
		oOptions.strInputPath  = strInputFilePath;
		oOptions.strOutputPath = strOutputPath;
		oOptions.strInputName  = strInputFileName;
		oOptions.bMerge        = g_pCmd->HasParameter( "-merge" );
		oOptions.bModels       = g_pCmd->HasParameter( "-models" );
		oOptions.bMatlibs      = g_pCmd->HasParameter( "-matlibs" );
		oOptions.bAll          = g_pCmd->HasParameter( "-all" );
		oOptions.bRecursive    = g_pCmd->HasParameter( "-recursive" );

		ResourcePipeline::DecompileResources( oOptions );
	}
}
