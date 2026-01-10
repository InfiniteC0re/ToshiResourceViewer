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

	TString8   strInputFileDir;
	const TINT iLastSlashIndex = strInputFilePath.FindReverse( '\\' );
	strInputFileDir.Copy( strInputFilePath, iLastSlashIndex + 1 );

	if ( !strOutputPath ) strOutputPath = strInputFileDir;

	TString8 strInputFileName      = ( iLastSlashIndex != -1 ) ? TString8( strInputFilePath.GetString( iLastSlashIndex + 1 ) ) : strInputFilePath;
	TString8 strInputFileNameNoExt = strInputFileName.Mid( 0, strInputFileName.FindReverse( '.' ) );

	if ( g_pCmd->HasParameter( "-compile" ) )
	{
		const TBOOL bCompileList = g_pCmd->HasParameter( "-list" );
		TString8    strTKLName   = g_pCmd->GetParameterValue( "-tkl", strInputFileNameNoExt );

		T2DynamicVector<TString8> vecInputFiles;

		if ( bCompileList )
		{
			// Read compile list file
			TFile* pFile = TFile::Create( strInputFilePath );
			if ( !pFile ) return;

			TSIZE  uiFileSize = pFile->GetSize();
			TCHAR* pBuffer    = new TCHAR[ uiFileSize + 1 ];
			TCHAR* pBufferEnd = pBuffer + uiFileSize;
			pFile->Read( pBuffer, uiFileSize );
			pBuffer[ uiFileSize ] = '\0';

			// Parse the file content
			T2FormatString512 oFmtStr;

			while ( pBuffer < pBufferEnd )
			{
				pBuffer += oFmtStr.ParseLine( pBuffer );

				if ( oFmtStr.Length() > 5 ) // .gltf at the end
				{
					vecInputFiles.PushBack( TString8::VarArgs( "%s\\%s", strInputFileDir.GetString(), oFmtStr.Get() ) );
				}
			}
		}
		else
		{
			vecInputFiles.PushBack( strInputFilePath );
		}

		// Set TKL builder so that all models will share single TKL
		TKLBuilder oTKLBuilder;
		oTKLBuilder.SetName( strTKLName.GetString() );
		ResourceLoader::ModelLoader_SetTKLBuilder( &oTKLBuilder );

		T2DynamicVector<ModelResourceView*> vecModelViews;

		T2_FOREACH( vecInputFiles, strInputFile )
		{
			ModelResourceView* pModelResView = new ModelResourceView();
			pModelResView->CreateExternal( strInputFile->GetString() );
			pModelResView->SetAutoSaveTKL( TFALSE ); // will save it later

			vecModelViews.PushBack( pModelResView );
		}

		// Complete the keylib with builder's data
		Resource::StreamedKeyLib_FindOrCreateDummy( TPS8D( oTKLBuilder.GetName() ) )->Create( oTKLBuilder );

		// Flush all models to disk
		T2_FOREACH( vecModelViews, it )
		{
			ModelResourceView* pModelResView = *it;

			// Prepare trb for model output
			PTRB oOutModel;
			oOutModel.GetSections()->CreateStream();
			pModelResView->OnSave( &oOutModel );

			TString8 strModelName = bCompileList ? pModelResView->GetFileName().Mid( 0, pModelResView->GetFileName().FindReverse( '.' ) ) : g_pCmd->GetParameterValue( "-name", strInputFileNameNoExt );
			oOutModel.WriteToFile( TString8::VarArgs( "%s\\%s.trb", strOutputPath.GetString(), strModelName.GetString() ).GetString(), bCompress );

			if ( it.Index() + 1 == vecModelViews.Size() )
			{
				// Prepare trb for keylib output
				PTRB oOutKeylib;
				oOutKeylib.GetSections()->CreateStream();
				pModelResView->OnSaveTKL( &oOutKeylib );

				oOutKeylib.WriteToFile( TString8::VarArgs( "%s\\%s.tkl", strOutputPath.GetString(), strTKLName.GetString() ).GetString(), bCompress );
			}

			delete *it;
		}

		vecModelViews.Clear();
	}
	else if ( g_pCmd->HasParameter( "-decompile" ) )
	{
		auto fnExportModel = [ &strOutputPath ]( const TString8& strFilePath ) -> TPString8 {
			const TINT iLastSlashIndex   = strFilePath.FindReverse( '\\' );
			TString8   strInputFile      = ( iLastSlashIndex != -1 ) ? TString8( strFilePath.GetString( iLastSlashIndex + 1 ) ) : strFilePath;
			TString8   strInputFileNoExt = strInputFile.Mid( 0, strInputFile.FindReverse( '.' ) );

			// Only skinned models are supported atm
			PTRB oInTRB( strFilePath.GetString() );
			auto pTMDLHeader = oInTRB.GetSymbols()->Find<void*>( oInTRB.GetSections(), "FileHeader" );
			if ( !pTMDLHeader ) return TPS8D( "Unknown" );

			ModelResourceView oModelResView;
			oModelResView.CreateTRB( &oInTRB, pTMDLHeader.get(), "FileHeader", strFilePath.GetString() );
			if ( !oModelResView.TryFixingMissingTKL() ) return TPS8D( "Unknown" );

			oModelResView.ExportScene( TString8::VarArgs( "%s\\%s.gltf", strOutputPath.GetString(), strInputFileNoExt.GetString() ).GetString() );
			return oModelResView.GetTKLName();
		};

		const TBOOL bDecompileAll = g_pCmd->HasParameter( "-all" );

		if ( bDecompileAll )
		{
			T2Map<TPString8, T2DynamicVector<TString8>, TPString8::Comparator> mapTKLToModels;
			TFileSystem* pFileSystem = TFileManager::GetSingleton()->FindFileSystem( "local" );

			TString8 strCurrentFile;
			TBOOL    bHasFile = pFileSystem->GetFirstFile( strInputFilePath, strCurrentFile );

			while ( bHasFile )
			{
				if ( strCurrentFile.EndsWithNoCase( ".trb" ) )
				{
					TString8  strFullPath = TString8::VarArgs( "%s\\%s", strInputFilePath.GetString(), strCurrentFile.GetString() );
					TPString8 strTKLName  = fnExportModel( strFullPath );

					// Add TKL to the list
					auto itModelList = mapTKLToModels.Find( strTKLName );
					T2DynamicVector<TString8>* pModelList  = ( itModelList == mapTKLToModels.End() ) ? mapTKLToModels.Insert( strTKLName, {} ) : &itModelList->second;

					pModelList->PushBack( strCurrentFile.Mid( 0, strCurrentFile.FindReverse( '.' ) ) );
				}

				bHasFile = pFileSystem->GetNextFile( strCurrentFile );
			}

			// Save model lists
			T2_FOREACH( mapTKLToModels, it )
			{
				TFile* pFile = TFile::Create( TString8::VarArgs( "%s\\%s.txt", strOutputPath.GetString(), it->first.GetString() ), TFILEMODE_CREATENEW );
				if ( !pFile ) break;

				T2_FOREACH( it->GetSecond(), itModel )
				{
					constexpr TCHAR NEW_LINE = '\n';

					pFile->Write( itModel->GetString(), itModel->Length() );
					pFile->Write( ".gltf", 5 );
					pFile->Write( &NEW_LINE, 1 );
				}
				
				pFile->Destroy();
			}
		}
		else fnExportModel( strInputFileName );
	}
}
