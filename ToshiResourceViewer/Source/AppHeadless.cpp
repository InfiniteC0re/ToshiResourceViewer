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
		struct DecompiledModel
		{
			tinygltf::Model*       pGLTFModel;
			tinyxml2::XMLDocument* pXML;
			TBOOL                  bValid;
			TBOOL                  bMerged;
		};

		T2DynamicVector<DecompiledModel> vecDecompiled;
		vecDecompiled.Reserve( 256 );

		DecompiledModel* pModelCursor = TNULL;
		auto             fnMoveCursor = [ & ]() {
            pModelCursor = &vecDecompiled.PushBack();

			pModelCursor->pGLTFModel = new tinygltf::Model;
			pModelCursor->pXML       = new tinyxml2::XMLDocument;
			pModelCursor->bValid     = TFALSE;
			pModelCursor->bMerged    = TFALSE;

            pModelCursor->pXML->InsertEndChild( pModelCursor->pXML->NewDeclaration() );
            pModelCursor->pXML->InsertEndChild( pModelCursor->pXML->NewComment( "Decompiled with Toshi Resource Viewer" ) );
		};

		tinygltf::TinyGLTF gltfWriter;

		auto fnExportModel = [ &strOutputPath, &pModelCursor, &fnMoveCursor, &gltfWriter ]( const TString8& strFilePath ) -> TPString8 {
			const TINT iLastSlashIndex   = strFilePath.FindReverse( '\\' );
			TString8   strInputFile      = ( iLastSlashIndex != -1 ) ? TString8( strFilePath.GetString( iLastSlashIndex + 1 ) ) : strFilePath;
			TString8   strInputFileNoExt = strInputFile.Mid( 0, strInputFile.FindReverse( '.' ) );

			// Only skinned models are supported atm
			PTRB oInTRB( strFilePath.GetString() );
			auto pTMDLHeader = oInTRB.GetSymbols()->Find<void*>( oInTRB.GetSections(), "FileHeader" );
			if ( !pTMDLHeader ) return TPString8();

			ModelResourceView oModelResView;
			oModelResView.CreateTRB( &oInTRB, pTMDLHeader.get(), "FileHeader", strFilePath.GetString() );
			if ( !oModelResView.TryFixingMissingTKL() ) return TPString8();

			oModelResView.SerializeModelInformation( pModelCursor->pXML );
			pModelCursor->bValid = oModelResView.ExportScene( *pModelCursor->pGLTFModel );

			if ( pModelCursor->bValid )
			{
				// Write model to the file
				gltfWriter.WriteGltfSceneToFile( pModelCursor->pGLTFModel, TString8::VarArgs( "%s\\%s.gltf", strOutputPath.GetString(), strInputFileNoExt.GetString() ).GetString(), TFALSE, TTRUE, TTRUE, TFALSE );

				fnMoveCursor();
				return oModelResView.GetTKLName();
			}

			return TPString8();
		};

		const TBOOL bDecompileAll = g_pCmd->HasParameter( "-all" );

		if ( bDecompileAll )
		{
			T2Map<TPString8, T2DynamicVector<TString8>, TPString8::Comparator> mapTKLToModels;
			TFileSystem* pFileSystem = TFileManager::GetSingleton()->FindFileSystem( "local" );

			TString8 strCurrentFile;
			TBOOL    bHasFile = pFileSystem->GetFirstFile( strInputFilePath, strCurrentFile );

			fnMoveCursor();
			while ( bHasFile )
			{
				if ( strCurrentFile.EndsWithNoCase( ".trb" ) )
				{
					TString8  strFullPath = TString8::VarArgs( "%s\\%s", strInputFilePath.GetString(), strCurrentFile.GetString() );
					TPString8 strTKLName  = fnExportModel( strFullPath );

					if ( !strTKLName.IsEmpty() )
					{
						// Add TKL to the list
						auto itModelList = mapTKLToModels.Find( strTKLName );
						T2DynamicVector<TString8>* pModelList  = ( itModelList == mapTKLToModels.End() ) ? mapTKLToModels.Insert( strTKLName, {} ) : &itModelList->second;

						TString8 strModelName = strCurrentFile.Mid( 0, strCurrentFile.FindReverse( '.' ) );
						pModelList->PushBack( strModelName );

						//pXMLInfo->SaveFile( TString8::VarArgs( "%s\\%s.xml", strOutputPath.GetString(), strModelName.GetString() ) );
					}
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

			// Try to merge models... Fuck it...
			// We need to find all equal models and merge them into a single one, because the only difference is animations
			T2_FOREACH( vecDecompiled, it )
			{
				// Skip invalid models or the ones already merged
				if ( !it->bValid || it->bMerged ) continue;

				const TSIZE iNumMats   = it->pGLTFModel->materials.size();
				const TSIZE iNumMeshes = it->pGLTFModel->meshes.size();
				const TSIZE iNumSkins  = it->pGLTFModel->skins.size();
				if ( iNumMats == 0 || iNumMeshes == 0 || iNumSkins == 0 )
				{
					it->bMerged = TTRUE;
					continue;
				}

				const TSIZE iNumBones = it->pGLTFModel->skins[ 0 ].joints.size();

				T2_FOREACH( vecDecompiled, itOther )
				{
					// Skip models we don't want to compare to
					if ( !itOther->bValid || itOther->bMerged || it == itOther ) continue;

					// Do fast comparisons first
					const TSIZE iNumMatsOther   = itOther->pGLTFModel->materials.size();
					const TSIZE iNumMeshesOther = itOther->pGLTFModel->meshes.size();
					const TSIZE iNumSkinsOther  = itOther->pGLTFModel->skins.size();
					if ( iNumMatsOther != iNumMats || iNumMeshesOther != iNumMeshes || iNumSkinsOther == 0 ) continue;

					const TSIZE iNumBonesOther = itOther->pGLTFModel->skins[ 0 ].joints.size();
					if ( iNumBonesOther != iNumBones ) continue;

					// At this stage we are sure number of various elements is the same, so need to make more deeper comparison
					auto itTMDL      = it->pXML->FirstChildElement( "TMDL" );
					auto itOtherTMDL = itOther->pXML->FirstChildElement( "TMDL" );

					// First of all, compare materials
					TBOOL bSame            = TTRUE;
					auto  itMaterials      = itTMDL->FirstChildElement( "Materials" );
					auto  itOtherMaterials = itOtherTMDL->FirstChildElement( "Materials" );
					auto  itMat            = itMaterials->FirstChildElement( "Material" );
					auto  itOtherMat       = itOtherMaterials->FirstChildElement( "Material" );
					for ( TSIZE i = 0; bSame && i < iNumMats; i++ )
					{
						bSame &= T2String8::CompareNoCase( itMat->Attribute( "Name" ), itOtherMat->Attribute( "Name" ) ) == 0;
						if ( !bSame ) break;
						bSame &= T2String8::CompareNoCase( itMat->Attribute( "Texture" ), itOtherMat->Attribute( "Texture" ) ) == 0;
						if ( !bSame ) break;

						itMat      = itMat->NextSiblingElement( "Material" );
						itOtherMat = itOtherMat->NextSiblingElement( "Material" );
					}
					if ( !bSame ) continue;

					// Compare keylib name
					if ( 0 == T2String8::CompareNoCase( itTMDL->FirstChildElement( "TSkeleton" )->FirstChildElement( "Sequences" )->Attribute( "KeyLibrary" ), itOtherTMDL->FirstChildElement( "TSkeleton" )->FirstChildElement( "Sequences" )->Attribute( "KeyLibrary" ) ) )
					{
						// The models are using the same keylib, so skip them...
						continue;
					}

					// Compare bones
					auto itBones      = itTMDL->FirstChildElement( "TSkeleton" )->FirstChildElement( "Bones" );
					auto itOtherBones = itOtherTMDL->FirstChildElement( "TSkeleton" )->FirstChildElement( "Bones" );

					auto itBone       = itBones->FirstChildElement( "Bone" );
					for ( TSIZE i = 0; bSame && i < iNumBones; i++ )
					{
						TBOOL bFoundBone  = TFALSE;
						auto  itOtherBone = itOtherBones->FirstChildElement( "Bone" );

						const TCHAR* pchItBoneName  = itBone->Attribute( "Name" );
						const TCHAR* pchItBonePosX  = itBone->Attribute( "PosX" );
						const TCHAR* pchItBonePosY  = itBone->Attribute( "PosY" );
						const TCHAR* pchItBonePosZ  = itBone->Attribute( "PosZ" );
						const TCHAR* pchItBoneQuatX = itBone->Attribute( "QuatX" );
						const TCHAR* pchItBoneQuatY = itBone->Attribute( "QuatY" );
						const TCHAR* pchItBoneQuatZ = itBone->Attribute( "QuatZ" );
						const TCHAR* pchItBoneQuatW = itBone->Attribute( "QuatW" );

						while ( itOtherBone && !bFoundBone )
						{
							bFoundBone |= T2String8::Compare( pchItBoneName, itOtherBone->Attribute( "Name" ) ) == 0;
							if ( bFoundBone ) break;

							bFoundBone |= T2String8::Compare( pchItBonePosX, itOtherBone->Attribute( "PosX" ) ) == 0;
							if ( bFoundBone ) break;

							bFoundBone |= T2String8::Compare( pchItBonePosY, itOtherBone->Attribute( "PosY" ) ) == 0;
							if ( bFoundBone ) break;

							bFoundBone |= T2String8::Compare( pchItBonePosZ, itOtherBone->Attribute( "PosZ" ) ) == 0;
							if ( bFoundBone ) break;

							bFoundBone |= T2String8::Compare( pchItBoneQuatX, itOtherBone->Attribute( "QuatX" ) ) == 0;
							if ( bFoundBone ) break;

							bFoundBone |= T2String8::Compare( pchItBoneQuatY, itOtherBone->Attribute( "QuatY" ) ) == 0;
							if ( bFoundBone ) break;

							bFoundBone |= T2String8::Compare( pchItBoneQuatZ, itOtherBone->Attribute( "QuatZ" ) ) == 0;
							if ( bFoundBone ) break;

							bFoundBone |= T2String8::Compare( pchItBoneQuatW, itOtherBone->Attribute( "QuatW" ) ) == 0;
							if ( bFoundBone ) break;
							
							itOtherBone = itOtherBone->NextSiblingElement( "Bone" );
						}

						bSame &= bFoundBone;
						if ( !bSame ) break;

						itBone = itBone->NextSiblingElement( "Bone" );
					}
					if ( !bSame ) break;

					// Compare meshes, kind of
					for ( TSIZE i = 0; i < iNumMeshes; i++ )
					{
						auto pItMesh      = &it->pGLTFModel->meshes[ i ];
						auto pItOtherMesh = &itOther->pGLTFModel->meshes[ i ];

						bSame &= pItMesh->name == pItOtherMesh->name;
						bSame &= pItMesh->primitives.size() == pItOtherMesh->primitives.size();
						if ( !bSame ) break;

						for ( TSIZE k = 0; k < pItMesh->primitives.size(); k++ )
						{
							TBOOL bFound      = TFALSE;
							TINT  iItIndices  = pItMesh->primitives[ k ].indices;
							TINT  iItVertices = pItMesh->primitives[ k ].attributes[ "POSITION" ];

							// Compare by indices and vertices
							for ( TSIZE j = 0; !bFound && j < pItOtherMesh->primitives.size(); j++ )
							{
								TINT iItOtherIndices = pItOtherMesh->primitives[ k ].indices;
								TINT iItOtherVertices = pItOtherMesh->primitives[ k ].attributes[ "POSITION" ];

								bFound = it->pGLTFModel->accessors[ iItIndices ].count == itOther->pGLTFModel->accessors[ iItOtherIndices ].count;
								bFound = it->pGLTFModel->accessors[ iItVertices ].count == itOther->pGLTFModel->accessors[ iItOtherVertices ].count;
								if ( !bFound ) break;

								// TODO?: compare UV
							}

							bSame &= bFound;
							if ( !bSame ) break;
						}
						if ( !bSame ) break;
					}

					// Okay... It reached the end and even though bSame might be TFALSE, one more critical check is required
					// Some models are compiled with different settings (or manually edited), so the previous check might give
					// false negatives sometimes
					TString8 strItModelName      = itTMDL->Attribute( "Name" );
					TString8 strItOtherModelName = itOtherTMDL->Attribute( "Name" );

					if ( !bSame && ( !strItOtherModelName.EndsWithNoCase( "_a2" ) && !strItOtherModelName.EndsWithNoCase( "_a3" ) ) && ( !strItModelName.EndsWithNoCase( "_a2" ) && !strItModelName.EndsWithNoCase( "_a3" ) ) ) break;

					TINFO( "Found duplicates: %s and %s\n", strItModelName.GetString(), strItOtherModelName.GetString() );
					itOther->bMerged = TTRUE;
				}

				// Don't process it anymore
				it->bMerged = TTRUE;
			}
		}
		else fnExportModel( strInputFileName );
	}
}
