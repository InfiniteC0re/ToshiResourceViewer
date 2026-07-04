#include "pch.h"
#include "ResourcePipeline/ModelDecompiler.h"
#include "Application.h"
#include "TRB/TRBFileWindow.h"
#include "ResourceView/ModelResourceView.h"

#include <deque>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

void ResourcePipeline::DecompileResources( const DecompileOptions& rOptions )
{
	const TBOOL    bMerge           = rOptions.bMerge;
	const TBOOL    bModels          = rOptions.bModels;
	const TBOOL    bMatlibs         = rOptions.bMatlibs;
	const TString8 strOutputPath    = rOptions.strOutputPath;
	const TString8 strInputFilePath = rOptions.strInputPath;
	const TString8 strInputFileName = rOptions.strInputName;

	struct DecompiledModel
	{
		tinygltf::Model*       pGLTFModel;
		tinyxml2::XMLDocument* pXML;
		TString8               strFileName; // output base name (no extension)
		TBOOL                  bValid;
		TBOOL                  bMerged;
		TINT                   iMergeTarget; // index of the primary this folds into, or -1
	};

	T2DynamicVector<DecompiledModel> vecDecompiled;
	vecDecompiled.Reserve( 256 );

	DecompiledModel* pModelCursor = TNULL;
	auto             fnMoveCursor = [ & ]() {
		pModelCursor = &vecDecompiled.PushBack();

		pModelCursor->pGLTFModel   = new tinygltf::Model;
		pModelCursor->pXML         = new tinyxml2::XMLDocument;
		pModelCursor->bValid       = TFALSE;
		pModelCursor->bMerged      = TFALSE;
		pModelCursor->iMergeTarget = -1;

		pModelCursor->pXML->InsertEndChild( pModelCursor->pXML->NewDeclaration() );
		pModelCursor->pXML->InsertEndChild( pModelCursor->pXML->NewComment( "Decompiled with Toshi Resource Viewer" ) );
	};

	tinygltf::TinyGLTF gltfWriter;

	auto fnExportModel = [ &strOutputPath, &pModelCursor, &fnMoveCursor, &gltfWriter, bMerge ]( PTRB& oInTRB, const TString8& strFilePath ) -> TPString8 {
		const TINT iLastSlashIndex   = strFilePath.FindReverse( '\\' );
		TString8   strInputFile      = ( iLastSlashIndex != -1 ) ? TString8( strFilePath.GetString( iLastSlashIndex + 1 ) ) : strFilePath;
		TString8   strInputFileNoExt = strInputFile.Mid( 0, strInputFile.FindReverse( '.' ) );

		auto pTMDLHeader     = oInTRB.GetSymbols()->Find<void*>( oInTRB.GetSections(), "FileHeader" );
		auto pDatabaseHeader = oInTRB.GetSymbols()->Find<void*>( oInTRB.GetSections(), "Database" );
		if ( !pTMDLHeader && !pDatabaseHeader ) return TPString8();

		ModelResourceView oModelResView;
		oModelResView.CreateTRB( &oInTRB, pTMDLHeader ? pTMDLHeader.get() : pDatabaseHeader.get(), pTMDLHeader ? "FileHeader" : "Database", strFilePath.GetString() );
		oModelResView.TryFixingMissingTKL();

		oModelResView.SerializeModelInformation( pModelCursor->pXML );
		pModelCursor->bValid       = oModelResView.ExportScene( *pModelCursor->pGLTFModel );
		pModelCursor->strFileName  = strInputFileNoExt;

		if ( pModelCursor->bValid )
		{
			pModelCursor->pXML->FirstChildElement( "TMDL" )->SetAttribute( "Output", strFilePath.GetString() );

			// When merging, GLTFs and XMLs are written in a final pass so that
			// duplicates can be folded into their primary first
			if ( !bMerge )
			{
				TString8 strOutGLTFPath = TString8::VarArgs( "%s\\%s.gltf", strOutputPath.GetString(), strInputFileNoExt.GetString() );

				gltfWriter.WriteGltfSceneToFile( pModelCursor->pGLTFModel, strOutGLTFPath.GetString(), TFALSE, TTRUE, TTRUE, TFALSE );

				pModelCursor->pXML->FirstChildElement( "TMDL" )->SetAttribute( "Source", strOutGLTFPath.GetString() );
				pModelCursor->pXML->SaveFile( TString8::VarArgs( "%s\\%s.xml", strOutputPath.GetString(), strInputFileNoExt.GetString() ) );
			}

			fnMoveCursor();
			return oModelResView.GetTKLName();
		}

		return TPString8();
	};

	enum class ResourceType
	{
		Unknown,
		Model,
		Matlib
	};

	auto fnExportMatlib = [ &strOutputPath, &pModelCursor, &fnMoveCursor, &gltfWriter ]( PTRB& oInTRB, const TString8& strFilePath ) {
		PTRBSymbols* pSymbols = oInTRB.GetSymbols();

		for ( TUINT i = 0; i < pSymbols->GetCount(); i++ )
		{
			TString8 strSymbolName = pSymbols->GetName( i ).Get();

			if ( !strSymbolName.CompareNoCase( "ttl" ) || strSymbolName.EndsWithNoCase( "_ttl" ) )
			{
				// Load textures and unpack them
				ResourceLoader::Textures vecTextures;

				if ( ResourceLoader::TTL_Load( pSymbols->GetByIndex<TBYTE>( oInTRB.GetSections(), i ).get(), oInTRB.GetEndianess(), TFALSE, TFALSE, vecTextures, TNULL ) )
				{
					ResourceLoader::TTL_UnpackTextures( vecTextures, strOutputPath, ResourceLoader::TextureFileFormat::TGA );
				}
			}
		}
	};

	auto fnGetResourceType = [ bModels, bMatlibs ]( PTRB& oInTRB ) -> ResourceType {
		auto pTMDLHeader     = oInTRB.GetSymbols()->Find<void*>( oInTRB.GetSections(), "FileHeader" );
		auto pDatabaseHeader = oInTRB.GetSymbols()->Find<void*>( oInTRB.GetSections(), "Database" );

		if ( bModels && ( oInTRB.GetSymbols()->Find<void*>( oInTRB.GetSections(), "FileHeader" ) || oInTRB.GetSymbols()->Find<void*>( oInTRB.GetSections(), "Database" ) ) )
			return ResourceType::Model;

		if ( bMatlibs && ( oInTRB.GetSymbols()->Find<void*>( oInTRB.GetSections(), "TTL" ) ) )
			return ResourceType::Matlib;

		return ResourceType::Unknown;
	};

	auto fnExportResource = [ &fnGetResourceType, &fnExportModel, &fnExportMatlib ]( PTRB& oInTRB, const TString8& strFilePath ) {
		auto pTMDLHeader     = oInTRB.GetSymbols()->Find<void*>( oInTRB.GetSections(), "FileHeader" );
		auto pDatabaseHeader = oInTRB.GetSymbols()->Find<void*>( oInTRB.GetSections(), "Database" );

		auto eResType = fnGetResourceType( oInTRB );

		switch ( eResType )
		{
			case ResourceType::Model:
				fnExportModel( oInTRB, strFilePath );
				break;
			case ResourceType::Matlib:
				fnExportMatlib( oInTRB, strFilePath );
				break;
		}
	};

	const TBOOL bDecompileAll = rOptions.bAll;
	const TBOOL bRecursive    = rOptions.bRecursive;

	if ( bDecompileAll )
	{
		T2Map<TPString8, T2DynamicVector<TString8>, TPString8::Comparator> mapTKLToModels;
		TFileSystem*                                                       pFileSystem = TFileManager::GetSingleton()->FindFileSystem( "local" );

		auto fnScanDir = [ & ]( auto&& self, const TString8& strDir ) -> void {
			T2DynamicVector<TString8> vecSubDirs;

			TString8 strCurrentFile;
			TBOOL    bHasFile = pFileSystem->GetFirstFile( strDir, strCurrentFile );

			while ( bHasFile )
			{
				if ( strCurrentFile.EndsWithNoCase( ".trb" ) || strCurrentFile.EndsWithNoCase( ".trz" ) || strCurrentFile.EndsWithNoCase( ".ttl" ) )
				{
					TString8 strFullPath = TString8::VarArgs( "%s\\%s", strDir.GetString(), strCurrentFile.GetString() );

					PTRB oInTRB;
					if ( oInTRB.ReadFromFile( strFullPath.GetString() ) )
					{
						ResourceType eResType = fnGetResourceType( oInTRB );

						// Special case when handling model resource type!
						if ( eResType == ResourceType::Model )
						{
							TPString8 strTKLName = fnExportModel( oInTRB, strFullPath );

							if ( !strTKLName.IsEmpty() )
							{
								// Add TKL to the list
								auto                       itModelList = mapTKLToModels.Find( strTKLName );
								T2DynamicVector<TString8>* pModelList  = ( itModelList == mapTKLToModels.End() ) ? mapTKLToModels.Insert( strTKLName, {} ) : &itModelList->second;

								TString8 strModelName = strCurrentFile.Mid( 0, strCurrentFile.FindReverse( '.' ) );
								pModelList->PushBack( strModelName );
							}
						}
						else if ( eResType != ResourceType::Unknown )
						{
							// Handle any other case
							fnExportResource( oInTRB, strFullPath );
						}
					}
				}
				else if ( bRecursive && strCurrentFile.Compare( "." ) != 0 && strCurrentFile.Compare( ".." ) != 0 )
				{
					vecSubDirs.PushBack( TString8::VarArgs( "%s\\%s", strDir.GetString(), strCurrentFile.GetString() ) );
				}

				bHasFile = pFileSystem->GetNextFile( strCurrentFile );
			}

			T2_FOREACH( vecSubDirs, itSubDir )
			{
				self( self, *itSubDir );
			}
		};

		fnMoveCursor();
		fnScanDir( fnScanDir, strInputFilePath );

		// Save model lists
		T2_FOREACH( mapTKLToModels, it )
		{
			TFile* pFile = TFile::Create( TString8::VarArgs( "%s\\%s.txt", strOutputPath.GetString(), it->first.GetString() ), TFILEMODE_CREATENEW );
			if ( !pFile ) break;

			T2_FOREACH( it->GetSecond(), itModel )
			{
				constexpr TCHAR NEW_LINE = '\n';

				TString8 strModelXML = TString8::VarArgs( "%s.xml", itModel->GetString() );

				pFile->Write( strModelXML.GetString(), strModelXML.Length() );
				pFile->Write( &NEW_LINE, 1 );
			}

			pFile->Destroy();
		}

		if ( bMerge )
		{
			// Collects the unique vertices (position + normal + uv + skinning),
			// quantizing floats so that decompile noise and different vertex
			// splitting/ordering don't matter, while staying discriminating enough
			// to tell unrelated models apart
			auto fnUniqueVertices = []( const tinygltf::Model& rModel ) -> std::vector<std::string> {
				static const char* s_apAttrs[] = { "POSITION", "NORMAL", "TEXCOORD_0", "JOINTS_0", "WEIGHTS_0" };

				std::vector<std::string> vecVertices;
				for ( const auto& rMesh : rModel.meshes )
				{
					for ( const auto& rPrim : rMesh.primitives )
					{
						auto itPos = rPrim.attributes.find( "POSITION" );
						if ( itPos == rPrim.attributes.end() ) continue;

						const TSIZE uiCount = rModel.accessors[ itPos->second ].count;
						for ( TSIZE k = 0; k < uiCount; k++ )
						{
							std::string strVertex;
							for ( const char* pchAttr : s_apAttrs )
							{
								auto itAttr = rPrim.attributes.find( pchAttr );
								if ( itAttr == rPrim.attributes.end() ) continue;

								const auto&  rAcc    = rModel.accessors[ itAttr->second ];
								const auto&  rView   = rModel.bufferViews[ rAcc.bufferView ];
								const auto&  rBuf    = rModel.buffers[ rView.buffer ];
								const TSIZE  uiComp  = TSIZE( tinygltf::GetNumComponentsInType( rAcc.type ) );
								const TSIZE  uiElem  = TSIZE( tinygltf::GetComponentSizeInBytes( rAcc.componentType ) ) * uiComp;
								const TSIZE  uiStride = rView.byteStride ? rView.byteStride : uiElem;
								const TBYTE* pData   = rBuf.data.data() + rView.byteOffset + rAcc.byteOffset + k * uiStride;

								if ( rAcc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT )
								{
									for ( TSIZE c = 0; c < uiComp; c++ )
									{
										const TINT32 iQuant = TINT32( std::lround( TREINTERPRETCAST( const TFLOAT*, pData )[ c ] * 10000.0f ) );
										strVertex.append( TREINTERPRETCAST( const char*, &iQuant ), sizeof( iQuant ) );
									}
								}
								else
								{
									strVertex.append( TREINTERPRETCAST( const char*, pData ), uiElem );
								}
							}
							vecVertices.push_back( std::move( strVertex ) );
						}
					}
				}
				std::sort( vecVertices.begin(), vecVertices.end() );
				vecVertices.erase( std::unique( vecVertices.begin(), vecVertices.end() ), vecVertices.end() );
				return vecVertices;
			};

			// Two models can be merged when they share skeleton, geometry and
			// materials and only differ by their animations (different keylib)
			auto fnModelsAreDuplicates = [ &fnUniqueVertices ]( const DecompiledModel& rA, const DecompiledModel& rB ) -> TBOOL {
				tinygltf::Model& rGA = *rA.pGLTFModel;
				tinygltf::Model& rGB = *rB.pGLTFModel;

				if ( rGA.materials.size() != rGB.materials.size() ) return TFALSE;
				if ( rGA.meshes.size() != rGB.meshes.size() || rGA.meshes.empty() ) return TFALSE;
				if ( rGA.skins.size() != 1 || rGB.skins.size() != 1 ) return TFALSE;

				auto pTMDLA = rA.pXML->FirstChildElement( "TMDL" );
				auto pTMDLB = rB.pXML->FirstChildElement( "TMDL" );
				if ( !pTMDLA || !pTMDLB ) return TFALSE;

				auto pSkelA = pTMDLA->FirstChildElement( "TSkeleton" );
				auto pSkelB = pTMDLB->FirstChildElement( "TSkeleton" );
				if ( !pSkelA || !pSkelB ) return TFALSE;

				// Only worth merging when the animation sets differ
				auto         pSeqA = pSkelA->FirstChildElement( "Sequences" );
				auto         pSeqB = pSkelB->FirstChildElement( "Sequences" );
				const TCHAR* pchKLA = pSeqA ? pSeqA->Attribute( "KeyLibrary" ) : TNULL;
				const TCHAR* pchKLB = pSeqB ? pSeqB->Attribute( "KeyLibrary" ) : TNULL;
				if ( pchKLA && pchKLB && T2String8::CompareNoCase( pchKLA, pchKLB ) == 0 ) return TFALSE;

				// Materials must match by name and texture, in order
				auto pMatA = pTMDLA->FirstChildElement( "Materials" )->FirstChildElement( "Material" );
				auto pMatB = pTMDLB->FirstChildElement( "Materials" )->FirstChildElement( "Material" );
				for ( ; pMatA && pMatB; pMatA = pMatA->NextSiblingElement( "Material" ), pMatB = pMatB->NextSiblingElement( "Material" ) )
				{
					if ( T2String8::CompareNoCase( pMatA->Attribute( "Name" ), pMatB->Attribute( "Name" ) ) != 0 ) return TFALSE;
					if ( T2String8::CompareNoCase( pMatA->Attribute( "Texture" ), pMatB->Attribute( "Texture" ) ) != 0 ) return TFALSE;
				}
				if ( pMatA || pMatB ) return TFALSE;

				// Skeletons must share a common prefix (matching name, parent and
				// transform within tolerance). Beyond it each may append its own bones,
				// as long as the extra names don't collide - they get unioned on merge.
				// The shared prefix covers every weighted bone (guaranteed by the vertex
				// check below, which includes joints), so the shared mesh stays valid
				static const TCHAR* s_apBoneFloats[] = { "PosX", "PosY", "PosZ", "QuatX", "QuatY", "QuatZ", "QuatW" };
				auto fnBonesMatch = []( tinyxml2::XMLElement* pA, tinyxml2::XMLElement* pB ) -> TBOOL {
					const TCHAR* a = pA->Attribute( "Name" );
					const TCHAR* b = pB->Attribute( "Name" );
					if ( T2String8::Compare( a ? a : "", b ? b : "" ) != 0 ) return TFALSE;
					if ( pA->IntAttribute( "Parent", -2 ) != pB->IntAttribute( "Parent", -2 ) ) return TFALSE;
					for ( const TCHAR* attr : s_apBoneFloats )
						if ( TMath::Abs( pA->FloatAttribute( attr ) - pB->FloatAttribute( attr ) ) > 0.0001f ) return TFALSE;
					return TTRUE;
				};

				auto pBonesA = pSkelA->FirstChildElement( "Bones" );
				auto pBonesB = pSkelB->FirstChildElement( "Bones" );
				if ( !pBonesA || !pBonesB ) return TFALSE;

				auto pBoneA = pBonesA->FirstChildElement( "Bone" );
				auto pBoneB = pBonesB->FirstChildElement( "Bone" );
				while ( pBoneA && pBoneB && fnBonesMatch( pBoneA, pBoneB ) )
				{
					pBoneA = pBoneA->NextSiblingElement( "Bone" );
					pBoneB = pBoneB->NextSiblingElement( "Bone" );
				}

				// The remaining bones are each skeleton's extras; require disjoint names
				std::vector<std::string> vecExtraA;
				for ( auto p = pBoneA; p; p = p->NextSiblingElement( "Bone" ) )
					if ( const TCHAR* n = p->Attribute( "Name" ) ) vecExtraA.push_back( n );
				for ( auto p = pBoneB; p; p = p->NextSiblingElement( "Bone" ) )
				{
					const TCHAR* n = p->Attribute( "Name" );
					if ( n && std::find( vecExtraA.begin(), vecExtraA.end(), n ) != vecExtraA.end() ) return TFALSE;
				}

				// Geometry must cover the same set of vertices, regardless of ordering,
				// splitting or decompile float noise
				if ( fnUniqueVertices( rGA ) != fnUniqueVertices( rGB ) ) return TFALSE;

				return TTRUE;
			};

			// Group duplicates: each surviving model becomes a primary and folds
			// matching models into itself
			T2_FOREACH( vecDecompiled, it )
			{
				if ( !it->bValid || it->bMerged ) continue;

				T2_FOREACH( vecDecompiled, itOther )
				{
					if ( !itOther->bValid || itOther->bMerged || it == itOther ) continue;
					if ( !fnModelsAreDuplicates( *it, *itOther ) ) continue;

					// The primary must hold the superset skeleton; if itOther has more
					// bones let it become the primary when the loop reaches it
					if ( itOther->pGLTFModel->skins[ 0 ].joints.size() > it->pGLTFModel->skins[ 0 ].joints.size() ) continue;

					const TCHAR* pchPrimary = it->pXML->FirstChildElement( "TMDL" )->Attribute( "Name" );
					const TCHAR* pchDup     = itOther->pXML->FirstChildElement( "TMDL" )->Attribute( "Name" );
					TINFO( "Merging %s into %s\n", pchDup, pchPrimary );

					itOther->bMerged      = TTRUE;
					itOther->iMergeTarget = it.Index();
				}
			}

			// Flatten merge chains (a primary can later become a duplicate itself):
			// point every duplicate at its ultimate, non-merged primary
			T2_FOREACH( vecDecompiled, itChain )
			{
				if ( !itChain->bValid || itChain->iMergeTarget < 0 ) continue;

				TINT iTarget = itChain->iMergeTarget;
				while ( vecDecompiled[ iTarget ].bMerged && vecDecompiled[ iTarget ].iMergeTarget >= 0 )
					iTarget = vecDecompiled[ iTarget ].iMergeTarget;

				itChain->iMergeTarget = iTarget;
			}

			auto fnFindNodeByName = []( const tinygltf::Model& rModel, const std::string& rName ) -> TINT {
				for ( TSIZE i = 0; i < rModel.nodes.size(); i++ )
					if ( rModel.nodes[ i ].name == rName ) return TINT( i );
				return -1;
			};

			// Copies an accessor and the bytes it spans into another model
			auto fnImportAccessor = []( tinygltf::Model& rDst, const tinygltf::Model& rSrc, TINT iAcc ) -> TINT {
				const auto& rAcc  = rSrc.accessors[ iAcc ];
				const auto& rView = rSrc.bufferViews[ rAcc.bufferView ];
				const auto& rBuf  = rSrc.buffers[ rView.buffer ];

				const TSIZE uiElemSize = TSIZE( tinygltf::GetComponentSizeInBytes( rAcc.componentType ) * tinygltf::GetNumComponentsInType( rAcc.type ) );
				const TSIZE uiStride   = rView.byteStride ? rView.byteStride : uiElemSize;
				const TSIZE uiStart    = rView.byteOffset + rAcc.byteOffset;
				const TSIZE uiLength   = rAcc.count ? ( uiStride * ( rAcc.count - 1 ) + uiElemSize ) : 0;

				tinygltf::Buffer oBuffer;
				oBuffer.data.assign( rBuf.data.begin() + uiStart, rBuf.data.begin() + uiStart + uiLength );
				rDst.buffers.push_back( std::move( oBuffer ) );

				tinygltf::BufferView oView;
				oView.buffer     = TINT( rDst.buffers.size() - 1 );
				oView.byteOffset = 0;
				oView.byteLength = uiLength;
				oView.byteStride = rView.byteStride;
				rDst.bufferViews.push_back( std::move( oView ) );

				tinygltf::Accessor oAccessor = rAcc;
				oAccessor.bufferView         = TINT( rDst.bufferViews.size() - 1 );
				oAccessor.byteOffset         = 0;
				rDst.accessors.push_back( std::move( oAccessor ) );

				return TINT( rDst.accessors.size() - 1 );
			};

			// Float accessors are equal within a tolerance, since the same animation
			// can quantize slightly differently across keylibs
			constexpr TFLOAT ANIM_MERGE_EPSILON = 0.01f;
			auto             fnAccessorFloatsEqual = []( const tinygltf::Model& rMA, TINT iAccA, const tinygltf::Model& rMB, TINT iAccB ) -> TBOOL {
				const auto& rAccA = rMA.accessors[ iAccA ];
				const auto& rAccB = rMB.accessors[ iAccB ];
				if ( rAccA.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || rAccB.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ) return TFALSE;
				if ( rAccA.type != rAccB.type || rAccA.count != rAccB.count ) return TFALSE;

				const auto& rViewA = rMA.bufferViews[ rAccA.bufferView ];
				const auto& rViewB = rMB.bufferViews[ rAccB.bufferView ];
				const auto& rBufA  = rMA.buffers[ rViewA.buffer ];
				const auto& rBufB  = rMB.buffers[ rViewB.buffer ];

				const TSIZE  uiNumComp  = TSIZE( tinygltf::GetNumComponentsInType( rAccA.type ) );
				const TSIZE  uiElemSize = uiNumComp * sizeof( TFLOAT );
				const TSIZE  uiStrideA  = rViewA.byteStride ? rViewA.byteStride : uiElemSize;
				const TSIZE  uiStrideB  = rViewB.byteStride ? rViewB.byteStride : uiElemSize;
				const TBYTE* pDataA     = rBufA.data.data() + rViewA.byteOffset + rAccA.byteOffset;
				const TBYTE* pDataB     = rBufB.data.data() + rViewB.byteOffset + rAccB.byteOffset;

				for ( TSIZE i = 0; i < rAccA.count; i++ )
				{
					const TFLOAT* pA = TREINTERPRETCAST( const TFLOAT*, pDataA + i * uiStrideA );
					const TFLOAT* pB = TREINTERPRETCAST( const TFLOAT*, pDataB + i * uiStrideB );
					for ( TSIZE c = 0; c < uiNumComp; c++ )
						if ( TMath::Abs( pA[ c ] - pB[ c ] ) > ANIM_MERGE_EPSILON ) return TFALSE;
				}
				return TTRUE;
			};

			// Two animations are equal when every channel (matched by target bone
			// and path) has sampler data equal within tolerance
			auto fnAnimationsEqual = [ &fnAccessorFloatsEqual ]( const tinygltf::Model& rMA, const tinygltf::Animation& rA, const tinygltf::Model& rMB, const tinygltf::Animation& rB ) -> TBOOL {
				if ( rA.channels.size() != rB.channels.size() ) return TFALSE;

				for ( const auto& rChA : rA.channels )
				{
					const std::string& rBoneA = rMA.nodes[ rChA.target_node ].name;
					const auto&        rSampA = rA.samplers[ rChA.sampler ];

					TBOOL bFound = TFALSE;
					for ( const auto& rChB : rB.channels )
					{
						if ( rChA.target_path != rChB.target_path || rMB.nodes[ rChB.target_node ].name != rBoneA ) continue;

						const auto& rSampB = rB.samplers[ rChB.sampler ];
						bFound = fnAccessorFloatsEqual( rMA, rSampA.input, rMB, rSampB.input ) && fnAccessorFloatsEqual( rMA, rSampA.output, rMB, rSampB.output );
						break;
					}
					if ( !bFound ) return TFALSE;
				}
				return TTRUE;
			};

			// Fold each duplicate's animations into its primary, then point the
			// duplicate's XML at the primary GLTF
			T2_FOREACH( vecDecompiled, itDup )
			{
				if ( !itDup->bValid || itDup->iMergeTarget < 0 ) continue;

				DecompiledModel& rPrimary = vecDecompiled[ itDup->iMergeTarget ];
				tinygltf::Model& rGDst    = *rPrimary.pGLTFModel;
				tinygltf::Model& rGSrc    = *itDup->pGLTFModel;

				// Deduplicate bones
				std::map<std::string, std::string> mapBoneToGltf;

				if ( !rGDst.skins.empty() && !rGSrc.skins.empty() && rGDst.skins[ 0 ].inverseBindMatrices >= 0 && rGSrc.skins[ 0 ].inverseBindMatrices >= 0 )
				{
					tinygltf::Skin& rSkinDst = rGDst.skins[ 0 ];
					tinygltf::Skin& rSkinSrc = rGSrc.skins[ 0 ];

					auto fnBuildParents = []( const tinygltf::Model& rModel ) {
						std::vector<int> vecParent( rModel.nodes.size(), -1 );
						for ( int p = 0; p < int( rModel.nodes.size() ); p++ )
							for ( int c : rModel.nodes[ p ].children )
								if ( c >= 0 && c < int( vecParent.size() ) ) vecParent[ c ] = p;
						return vecParent;
					};

					auto fnTransformKey = []( const tinygltf::Node& rNode ) -> std::string {
						auto fnAt = []( const std::vector<double>& v, TSIZE i, double def ) { return v.size() > i ? v[ i ] : def; };
						TCHAR szBuf[ 160 ];
						snprintf( szBuf, sizeof( szBuf ), "%.4f,%.4f,%.4f|%.5f,%.5f,%.5f,%.5f",
						    fnAt( rNode.translation, 0, 0.0 ), fnAt( rNode.translation, 1, 0.0 ), fnAt( rNode.translation, 2, 0.0 ),
						    fnAt( rNode.rotation, 0, 0.0 ), fnAt( rNode.rotation, 1, 0.0 ), fnAt( rNode.rotation, 2, 0.0 ), fnAt( rNode.rotation, 3, 1.0 ) );
						return szBuf;
					};

					const std::vector<int> vecDstParent = fnBuildParents( rGDst );
					const std::vector<int> vecSrcParent = fnBuildParents( rGSrc );

					std::map<std::string, std::deque<std::string>> mapPrimaryStruct;
					for ( int iJoint : rSkinDst.joints )
					{
						const int   iParent   = vecDstParent[ iJoint ];
						std::string strParent = ( iParent >= 0 ) ? rGDst.nodes[ iParent ].name : std::string();
						mapPrimaryStruct[ strParent + "#" + fnTransformKey( rGDst.nodes[ iJoint ] ) ].push_back( rGDst.nodes[ iJoint ].name );
					}

					// Reads MAT4 (64-byte) elements from an inverse-bind-matrix accessor
					auto fnAppendMatrices = []( std::vector<TBYTE>& rOut, const tinygltf::Model& rModel, TINT iAcc, TINT iStart, TINT iCount ) {
						const auto&  rAcc    = rModel.accessors[ iAcc ];
						const auto&  rView   = rModel.bufferViews[ rAcc.bufferView ];
						const auto&  rBuf    = rModel.buffers[ rView.buffer ];
						const TSIZE  uiStride = rView.byteStride ? rView.byteStride : 64;
						const TBYTE* pData   = rBuf.data.data() + rView.byteOffset + rAcc.byteOffset;
						for ( TINT i = iStart; i < iStart + iCount; i++ )
							rOut.insert( rOut.end(), pData + i * uiStride, pData + i * uiStride + 64 );
					};

					std::vector<TBYTE> vecIBM;
					fnAppendMatrices( vecIBM, rGDst, rSkinDst.inverseBindMatrices, 0, TINT( rSkinDst.joints.size() ) );

					TBOOL bAppended = TFALSE;
					for ( TSIZE js = 0; js < rSkinSrc.joints.size(); js++ )
					{
						const int             iSrcJoint = rSkinSrc.joints[ js ];
						const tinygltf::Node& rSrcNode  = rGSrc.nodes[ iSrcJoint ];

						const int   iSrcParent      = vecSrcParent[ iSrcJoint ];
						std::string strParentMapped = ( iSrcParent >= 0 ) ? rGSrc.nodes[ iSrcParent ].name : std::string();
						if ( !strParentMapped.empty() )
						{
							auto itParent = mapBoneToGltf.find( strParentMapped );
							if ( itParent != mapBoneToGltf.end() ) strParentMapped = itParent->second;
						}

						const std::string strKey    = strParentMapped + "#" + fnTransformKey( rSrcNode );
						auto              itStruct  = mapPrimaryStruct.find( strKey );
						if ( itStruct != mapPrimaryStruct.end() && !itStruct->second.empty() )
						{
							// Same bone under a different name - reuse the primary's node
							mapBoneToGltf[ rSrcNode.name ] = itStruct->second.front();
							itStruct->second.pop_front();
							continue;
						}

						// Found a new bone
						tinygltf::Node oNode;
						oNode.name        = rSrcNode.name;
						oNode.translation = rSrcNode.translation;
						oNode.rotation    = rSrcNode.rotation;
						rGDst.nodes.push_back( oNode );
						const int iNewNode = int( rGDst.nodes.size() - 1 );

						rSkinDst.joints.push_back( iNewNode );
						fnAppendMatrices( vecIBM, rGSrc, rSkinSrc.inverseBindMatrices, TINT( js ), 1 );

						const int iDstParent = strParentMapped.empty() ? -1 : fnFindNodeByName( rGDst, strParentMapped );
						if ( iDstParent >= 0 ) rGDst.nodes[ iDstParent ].children.push_back( iNewNode );

						mapBoneToGltf[ rSrcNode.name ] = rSrcNode.name;
						bAppended = TTRUE;
					}

					// Rebuild the inverse-bind-matrices accessor with the extended data
					if ( bAppended )
					{
						const TSIZE uiIBMBytes = vecIBM.size();
						tinygltf::Buffer oBuffer;
						oBuffer.data = std::move( vecIBM );
						rGDst.buffers.push_back( std::move( oBuffer ) );

						tinygltf::BufferView oView;
						oView.buffer     = int( rGDst.buffers.size() - 1 );
						oView.byteOffset = 0;
						oView.byteLength = uiIBMBytes;
						rGDst.bufferViews.push_back( oView );

						tinygltf::Accessor oAccessor;
						oAccessor.bufferView    = int( rGDst.bufferViews.size() - 1 );
						oAccessor.byteOffset    = 0;
						oAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
						oAccessor.type          = TINYGLTF_TYPE_MAT4;
						oAccessor.count         = rSkinDst.joints.size();
						rGDst.accessors.push_back( oAccessor );

						rSkinDst.inverseBindMatrices = int( rGDst.accessors.size() - 1 );
					}
				}

				std::map<std::string, std::string> mapSeqToGltf; // duplicate sequence name -> primary glTF animation name

				for ( auto& rSrcAnim : rGSrc.animations )
				{
					std::string strGltfName;

					// Reuse an identical animation already present in the primary
					for ( auto& rDstAnim : rGDst.animations )
					{
						if ( rDstAnim.name == rSrcAnim.name && fnAnimationsEqual( rGDst, rDstAnim, rGSrc, rSrcAnim ) )
						{
							strGltfName = rDstAnim.name;
							break;
						}
					}

					if ( strGltfName.empty() )
					{
						// Namespace the name if it is taken by a different animation
						std::string strName    = rSrcAnim.name;
						TBOOL       bNameTaken = TFALSE;
						for ( auto& rDstAnim : rGDst.animations )
							if ( rDstAnim.name == strName ) { bNameTaken = TTRUE; break; }

						if ( bNameTaken )
						{
							const TCHAR* pchKL = itDup->pXML->FirstChildElement( "TMDL" )->FirstChildElement( "TSkeleton" )->FirstChildElement( "Sequences" )->Attribute( "KeyLibrary" );
							strName += "@";
							strName += pchKL ? pchKL : itDup->strFileName.GetString();
						}

						tinygltf::Animation oAnim;
						oAnim.name     = strName;
						oAnim.samplers = rSrcAnim.samplers;
						for ( auto& rSamp : oAnim.samplers )
						{
							rSamp.input  = fnImportAccessor( rGDst, rGSrc, rSamp.input );
							rSamp.output = fnImportAccessor( rGDst, rGSrc, rSamp.output );
						}
						for ( const auto& rChSrc : rSrcAnim.channels )
						{
							const std::string& strSrcNode = rGSrc.nodes[ rChSrc.target_node ].name;
							auto               itBone     = mapBoneToGltf.find( strSrcNode );
							const std::string& strDstNode = ( itBone != mapBoneToGltf.end() ) ? itBone->second : strSrcNode;

							const TINT iNode = fnFindNodeByName( rGDst, strDstNode );
							if ( iNode < 0 ) continue;

							tinygltf::AnimationChannel oChannel = rChSrc;
							oChannel.target_node                = iNode;
							oAnim.channels.push_back( oChannel );
						}

						rGDst.animations.push_back( std::move( oAnim ) );
						strGltfName = strName;
					}

					mapSeqToGltf[ rSrcAnim.name ] = strGltfName;
				}

				auto pTMDL = itDup->pXML->FirstChildElement( "TMDL" );
				pTMDL->SetAttribute( "Source", TString8::VarArgs( "%s\\%s.gltf", strOutputPath.GetString(), rPrimary.strFileName.GetString() ).GetString() );

				auto pSeqRoot = pTMDL->FirstChildElement( "TSkeleton" )->FirstChildElement( "Sequences" );
				for ( auto pSeqElem = pSeqRoot->FirstChildElement( "Sequence" ); pSeqElem; pSeqElem = pSeqElem->NextSiblingElement( "Sequence" ) )
				{
					auto itMap = mapSeqToGltf.find( pSeqElem->Attribute( "Name" ) );
					if ( itMap != mapSeqToGltf.end() && itMap->second != pSeqElem->Attribute( "Name" ) )
						pSeqElem->SetAttribute( "GltfName", itMap->second.c_str() );
				}

				// Point each bone at its shared GLTF node when the merge renamed it
				auto pBonesRoot = pTMDL->FirstChildElement( "TSkeleton" )->FirstChildElement( "Bones" );
				for ( auto pBoneElem = pBonesRoot ? pBonesRoot->FirstChildElement( "Bone" ) : TNULL; pBoneElem; pBoneElem = pBoneElem->NextSiblingElement( "Bone" ) )
				{
					const TCHAR* pchBoneName = pBoneElem->Attribute( "Name" );
					if ( !pchBoneName ) continue;

					auto itMap = mapBoneToGltf.find( pchBoneName );
					if ( itMap != mapBoneToGltf.end() && itMap->second != pchBoneName )
						pBoneElem->SetAttribute( "GltfName", itMap->second.c_str() );
				}

				// The duplicate's GLTF is fully consumed now; free it to keep memory
				// bounded across a large batch
				delete itDup->pGLTFModel;
				itDup->pGLTFModel = TNULL;
			}

			// Final write: each primary/standalone owns a GLTF; merged duplicates
			// only save their XML (already pointed at the primary)
			T2_FOREACH( vecDecompiled, itOut )
			{
				if ( !itOut->bValid ) continue;

				if ( !itOut->bMerged )
				{
					TString8 strGLTFPath = TString8::VarArgs( "%s\\%s.gltf", strOutputPath.GetString(), itOut->strFileName.GetString() );
					gltfWriter.WriteGltfSceneToFile( itOut->pGLTFModel, strGLTFPath.GetString(), TFALSE, TTRUE, TTRUE, TFALSE );
					itOut->pXML->FirstChildElement( "TMDL" )->SetAttribute( "Source", strGLTFPath.GetString() );

					// Release the GLTF once written to keep peak memory down
					delete itOut->pGLTFModel;
					itOut->pGLTFModel = TNULL;
				}

				itOut->pXML->SaveFile( TString8::VarArgs( "%s\\%s.xml", strOutputPath.GetString(), itOut->strFileName.GetString() ) );
			}
		}
	}
	else
	{
		PTRB oInTRB;

		if ( oInTRB.ReadFromFile( strInputFileName.GetString() ) )
			fnExportResource( oInTRB, strInputFileName );
	}
}
