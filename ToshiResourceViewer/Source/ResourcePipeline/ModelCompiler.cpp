#include "pch.h"
#include "ResourcePipeline/ModelCompiler.h"
#include "Application.h"
#include "TRB/TRBFileWindow.h"
#include "ResourceView/ModelResourceView.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

void ResourcePipeline::CompileModels( const CompileOptions& rOptions )
{
	const TBOOL    bCompress        = rOptions.bCompress;
	const TString8 strOutputPath    = rOptions.strOutputPath;
	const TString8 strInputFilePath = rOptions.strInputPath;

	const TBOOL bCompileList  = rOptions.bList;
	const TBOOL bCompileAll   = rOptions.bAll;
	TString8    strTKLNameArg = rOptions.strTKLName;

	struct CompileInput
	{
		TString8               strFilePath;
		TString8               strXMLFilePath;
		tinyxml2::XMLDocument* pXML;
		TBOOL                  bCanCompile;
	};

	auto fnReadCompileList = []( const TString8& strCompileInputPath, T2DynamicVector<TString8>& rOutInputs ) {
		TString8   strCompileInputFileDir;
		const TINT iCompileLastSlashIndex = strCompileInputPath.FindReverse( '\\' );
		strCompileInputFileDir.Copy( strCompileInputPath, iCompileLastSlashIndex + 1 );

		TFile* pFile = TFile::Create( strCompileInputPath );
		if ( !pFile ) return;

		TSIZE  uiFileSize = pFile->GetSize();
		TCHAR* pBuffer    = new TCHAR[ uiFileSize + 1 ];
		TCHAR* pCursor    = pBuffer;
		TCHAR* pBufferEnd = pBuffer + uiFileSize;
		pFile->Read( pBuffer, uiFileSize );
		pBuffer[ uiFileSize ] = '\0';

		T2FormatString512 oFmtStr;
		while ( pCursor < pBufferEnd )
		{
			pCursor += oFmtStr.ParseLine( pCursor );

			if ( oFmtStr.Length() > 0 )
				rOutInputs.PushBack() = TString8::VarArgs( "%s\\%s", strCompileInputFileDir.GetString(), oFmtStr.Get() );
		}

		delete[] pBuffer;
		pFile->Destroy();
	};

	auto fnGetXMLKeyLibrary = []( const TString8& strFilePath ) -> TString8 {
		TString8 strXMLFilePath = strFilePath;
		if ( !strXMLFilePath.EndsWithNoCase( ".xml" ) )
			strXMLFilePath += ".xml";

		tinyxml2::XMLDocument oXML;
		if ( oXML.LoadFile( strXMLFilePath.GetString() ) != tinyxml2::XML_SUCCESS )
			return TString8();

		auto pTMDLElem     = oXML.FirstChildElement( "TMDL" );
		auto pSkeletonElem = pTMDLElem ? pTMDLElem->FirstChildElement( "TSkeleton" ) : TNULL;
		auto pSeqElem      = pSkeletonElem ? pSkeletonElem->FirstChildElement( "Sequences" ) : TNULL;
		if ( !pSeqElem || !pSeqElem->Attribute( "KeyLibrary" ) ) return TString8();

		return pSeqElem->Attribute( "KeyLibrary" );
	};

	auto fnCompileInputs = [ & ]( T2DynamicVector<TString8>& rCompileInputPaths, const TString8& strFallbackName, TBOOL bUseSourceNames ) {
		TString8 strTKLName = strTKLNameArg;

		T2DynamicVector<CompileInput> vecInputFiles;

		auto fnAddCompileInput = [ &vecInputFiles ]( const TString8& strFilePath ) {
			TString8 strInputFilePath = strFilePath;

			if ( !strInputFilePath.EndsWithNoCase( ".gltf" ) && !strInputFilePath.EndsWithNoCase( ".xml" ) )
				strInputFilePath += ".xml";

			CompileInput& rInput = vecInputFiles.PushBack();
			rInput.strFilePath   = strInputFilePath;
			rInput.pXML          = TNULL;
			rInput.bCanCompile   = TTRUE;

			if ( strInputFilePath.EndsWithNoCase( ".xml" ) )
			{
				rInput.pXML = new tinyxml2::XMLDocument;

				if ( rInput.pXML->LoadFile( strInputFilePath.GetString() ) != tinyxml2::XML_SUCCESS )
				{
					TERROR( "Failed to read model XML: %s\n", strInputFilePath.GetString() );
					delete rInput.pXML;
					rInput.pXML        = TNULL;
					rInput.bCanCompile = TFALSE;
					return;
				}

				rInput.strXMLFilePath = strInputFilePath;

				auto pTMDLElem = rInput.pXML->FirstChildElement( "TMDL" );
				if ( pTMDLElem && pTMDLElem->Attribute( "Source" ) )
					rInput.strFilePath = pTMDLElem->Attribute( "Source" );

				if ( pTMDLElem && pTMDLElem->Attribute( "Type" ) && T2String8::CompareNoCase( pTMDLElem->Attribute( "Type" ), "Skin" ) != 0 )
				{
					TERROR( "Cannot compile %s: model type '%s' is not supported yet\n", strInputFilePath.GetString(), pTMDLElem->Attribute( "Type" ) );
					rInput.bCanCompile = TFALSE;
				}
			}
		};

		T2_FOREACH( rCompileInputPaths, itCompileInputPath )
		fnAddCompileInput( *itCompileInputPath );

		if ( !strTKLName )
		{
			T2_FOREACH( vecInputFiles, itInput )
			{
				if ( !itInput->bCanCompile ) continue;
				if ( !itInput->pXML ) continue;

				auto pTMDLElem     = itInput->pXML->FirstChildElement( "TMDL" );
				auto pSkeletonElem = pTMDLElem ? pTMDLElem->FirstChildElement( "TSkeleton" ) : TNULL;
				auto pSeqElem      = pSkeletonElem ? pSkeletonElem->FirstChildElement( "Sequences" ) : TNULL;
				if ( !pSeqElem ) continue;

				strTKLName = pSeqElem->Attribute( "KeyLibrary" );
				if ( strTKLName ) break;
			}
		}

		if ( !strTKLName ) strTKLName = strFallbackName;

		// Set TKL builder so that all models will share single TKL
		TKLBuilder oTKLBuilder;
		oTKLBuilder.SetName( strTKLName.GetString() );
		ResourceLoader::ModelLoader_SetTKLBuilder( &oTKLBuilder );

		T2DynamicVector<ModelResourceView*> vecModelViews;
		T2DynamicVector<TString8>           vecOutputNames; // parallel to vecModelViews

		T2_FOREACH( vecInputFiles, itInput )
		{
			if ( !itInput->bCanCompile ) continue;

			// Restrict the load to the animations this model references, so a
			// merged model only pulls its own sequences from the shared GLTF
			ResourceLoader::AnimationFilter oAnimFilter;
			if ( itInput->pXML )
			{
				auto pTMDL = itInput->pXML->FirstChildElement( "TMDL" );
				auto pSkel = pTMDL ? pTMDL->FirstChildElement( "TSkeleton" ) : TNULL;
				auto pSeqs = pSkel ? pSkel->FirstChildElement( "Sequences" ) : TNULL;

				for ( auto pSeq = pSeqs ? pSeqs->FirstChildElement( "Sequence" ) : TNULL; pSeq; pSeq = pSeq->NextSiblingElement( "Sequence" ) )
				{
					const TCHAR* pchName = pSeq->Attribute( "Name" );
					if ( !pchName ) continue;

					const TCHAR* pchGltfName = pSeq->Attribute( "GltfName" );
					oAnimFilter.PushBack( { pchGltfName ? pchGltfName : pchName, pchName } );
				}
			}

			// Restrict the load to the bones this model lists, so a model sharing a
			// merged GLTF keeps only its own skeleton
			ResourceLoader::BoneFilter oBoneFilter;
			if ( itInput->pXML )
			{
				auto pTMDL  = itInput->pXML->FirstChildElement( "TMDL" );
				auto pSkel  = pTMDL ? pTMDL->FirstChildElement( "TSkeleton" ) : TNULL;
				auto pBones = pSkel ? pSkel->FirstChildElement( "Bones" ) : TNULL;

				for ( auto pBone = pBones ? pBones->FirstChildElement( "Bone" ) : TNULL; pBone; pBone = pBone->NextSiblingElement( "Bone" ) )
				{
					const TCHAR* pchName = pBone->Attribute( "Name" );
					if ( !pchName ) continue;

					const TCHAR* pchGltfName = pBone->Attribute( "GltfName" );
					oBoneFilter.PushBack( { pchGltfName ? pchGltfName : pchName, pchName } );
				}
			}

			ResourceLoader::ModelLoader_SetAnimationFilter( oAnimFilter.IsEmpty() ? TNULL : &oAnimFilter );
			ResourceLoader::ModelLoader_SetBoneFilter( oBoneFilter.IsEmpty() ? TNULL : &oBoneFilter );

			ModelResourceView* pModelResView = new ModelResourceView();
			pModelResView->CreateExternal( itInput->strFilePath.GetString() );

			ResourceLoader::ModelLoader_SetAnimationFilter( TNULL );
			ResourceLoader::ModelLoader_SetBoneFilter( TNULL );

			if ( itInput->pXML )
				pModelResView->DeserializeModelInformation( itInput->pXML );

			pModelResView->SetAutoSaveTKL( TFALSE ); // will save it later

			// Output name comes from the model's own XML, not its (possibly shared) GLTF
			TString8   strXml   = itInput->strXMLFilePath;
			const TINT iSlash   = TMath::Max( strXml.FindReverse( '\\' ), strXml.FindReverse( '/' ) );
			TString8   strBase  = ( iSlash != -1 ) ? TString8( strXml.GetString( iSlash + 1 ) ) : strXml;
			const TINT iDot     = strBase.FindReverse( '.' );
			TString8   strOutput = !strBase.IsEmpty() ? strBase.Mid( 0, iDot != -1 ? iDot : strBase.Length() ) : pModelResView->GetFileName().Mid( 0, pModelResView->GetFileName().FindReverse( '.' ) );

			vecModelViews.PushBack( pModelResView );
			vecOutputNames.PushBack( strOutput );
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

			TString8 strModelName = bUseSourceNames ? vecOutputNames[ it.Index() ] : ( rOptions.strForcedName.IsEmpty() ? strFallbackName : rOptions.strForcedName );
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

		T2_FOREACH( vecInputFiles, itInput )
		{
			delete itInput->pXML;
		}

		vecModelViews.Clear();
	};

	auto fnCompile = [ & ]( const TString8& strCompileInputPath, TBOOL bCompileList ) {
		const TINT iCompileLastSlashIndex       = strCompileInputPath.FindReverse( '\\' );
		TString8   strCompileInputFileName      = ( iCompileLastSlashIndex != -1 ) ? TString8( strCompileInputPath.GetString( iCompileLastSlashIndex + 1 ) ) : strCompileInputPath;
		TString8   strCompileInputFileNameNoExt = strCompileInputFileName.Mid( 0, strCompileInputFileName.FindReverse( '.' ) );

		T2DynamicVector<TString8> vecCompileInputPaths;
		if ( bCompileList )
			fnReadCompileList( strCompileInputPath, vecCompileInputPaths );
		else
			vecCompileInputPaths.PushBack() = strCompileInputPath;

		fnCompileInputs( vecCompileInputPaths, strCompileInputFileNameNoExt, bCompileList );
	};

	if ( bCompileAll )
	{
		TFileSystem* pFileSystem = TFileManager::GetSingleton()->FindFileSystem( "local" );

		TString8 strCurrentFile;
		TBOOL    bHasFile = pFileSystem->GetFirstFile( strInputFilePath, strCurrentFile );

		while ( bHasFile )
		{
			if ( strCurrentFile.EndsWithNoCase( ".txt" ) )
				fnCompile( TString8::VarArgs( "%s\\%s", strInputFilePath.GetString(), strCurrentFile.GetString() ), TTRUE );

			bHasFile = pFileSystem->GetNextFile( strCurrentFile );
		}
	}
	else
	{
		fnCompile( strInputFilePath, bCompileList );
	}
}
