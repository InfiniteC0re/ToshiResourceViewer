#include "pch.h"
#include "AppHeadless.h"
#include "Application.h"
#include "TRB/TRBFileWindow.h"
#include "ResourceView/ModelResourceView.h"

#include <ToshiTools/T2CommandLine.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

void HeadlessMain( TINT argc, TCHAR** argv )
{
	const TBOOL bCompress        = g_pCmd->HasParameter( "-compress" );
	const TBOOL bSkinned         = g_pCmd->HasParameter( "-skinned" );
	TString8    strOutputPath    = g_pCmd->GetParameterValue( "-output" );
	TString8    strInputFilePath = g_pCmd->GetParameterValue( "-input" );

	if ( !strInputFilePath ) return;

	const TINT iLastSlashIndex = strInputFilePath.FindReverse( '\\' );
	if ( !strOutputPath ) strOutputPath.Copy( strInputFilePath, iLastSlashIndex + 1 );

	if ( g_pCmd->HasParameter( "-compile" ) )
	{
		TString8 strInputFile      = ( iLastSlashIndex != -1 ) ? TString8( strInputFilePath.GetString( iLastSlashIndex + 1 ) ) : strInputFilePath;
		TString8 strInputFileNoExt = strInputFile.Mid( 0, strInputFile.FindReverse( '.' ) );

		TString8 strTKLName = g_pCmd->GetParameterValue( "-tkl", strInputFileNoExt );

		if ( !strInputFilePath ) return;

		// Set TKL builder so that all models will share single TKL
		TKLBuilder oTKLBuilder;
		oTKLBuilder.SetName( strTKLName.GetString() );
		ResourceLoader::ModelLoader_SetTKLBuilder( &oTKLBuilder );

		ModelResourceView* pModelResView = new ModelResourceView();
		pModelResView->CreateExternal( strInputFilePath.GetString(), strInputFile.GetString() );
		pModelResView->SetAutoSaveTKL( TFALSE ); // will save it later

		// Complete the keylib with builder's data
		Resource::StreamedKeyLib_FindOrCreateDummy( TPS8D( oTKLBuilder.GetName() ) )->Create( oTKLBuilder );

		// Prepare trb for model output
		PTRB oOutModel;
		oOutModel.GetSections()->CreateStream();
		pModelResView->OnSave( &oOutModel );

		// Prepare trb for keylib output
		PTRB oOutKeylib;
		oOutKeylib.GetSections()->CreateStream();
		pModelResView->OnSaveTKL( &oOutKeylib );

		// Write both model and keylib
		TString8 strModelName = g_pCmd->GetParameterValue( "-name", strInputFileNoExt );
		oOutModel.WriteToFile( TString8::VarArgs( "%s\\%s.trb", strOutputPath.GetString(), strModelName.GetString() ).GetString(), bCompress );
		oOutKeylib.WriteToFile( TString8::VarArgs( "%s\\%s.tkl", strOutputPath.GetString(), strTKLName.GetString() ).GetString(), bCompress );

		delete pModelResView;
	}
}
