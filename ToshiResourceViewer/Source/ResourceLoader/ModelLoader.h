#pragma once
#include "Resource/StreamedKeyLib.h"
#include "Resource/Material.h"
#include "ResourceLoader/TextureLoader.h"

#include <Render/TModel.h>
#include <Render/TModelCollision.h>
#include <Render/TSkeleton.h>
#include <Toshi/T2SharedPtr.h>
#include <Toshi/T2String.h>
#include <Plugins/PTRB.h>

namespace ResourceLoader
{

enum class ModelType
{
	None,
	Skin,
	World,
	Grass,
	StaticInstance,
};

TFORCEINLINE const TCHAR* GetModelTypeName( ModelType eModelType )
{
	switch ( eModelType )
	{
		case ModelType::Skin: return "Skin";
		case ModelType::World: return "World";
		case ModelType::Grass: return "Grass";
		case ModelType::StaticInstance: return "StaticInstance";
		default: return "Unknown";
	}
}

class MaterialCache
{
public:
	Toshi::T2SharedPtr<Resource::Material> Find( const TCHAR* pchName )
	{
		T2_FOREACH( m_vecMaterials, it )
		{
			if ( Toshi::TStringManager::String8CompareNoCase( it->first.GetString(), pchName ) == 0 )
				return it->second;
		}

		return {};
	}

	void Touch( const TCHAR* pchName, Toshi::T2SharedPtr<Resource::Material> pMaterial )
	{
		m_vecMaterials.PushBack( { pchName, pMaterial } );

		if ( m_pClass == TNULL ) m_pClass = pMaterial->GetClass();
		else TASSERT( m_pClass == pMaterial->GetClass() );
	}

private:
	const Toshi::TClass* m_pClass = TNULL;

	Toshi::T2DynamicVector<Toshi::T2Pair<Toshi::TString8, Toshi::T2SharedPtr<Resource::Material>>> m_vecMaterials;
};

class Model
{
public:
	Model();
	~Model();

	void Render();

public:
	PTRB* pTRB;
	
	Toshi::TTMDBase::SkeletonHeader              oSkeletonHeader;
	Toshi::TSkeleton*                            pSkeleton;
	Toshi::T2SharedPtr<Resource::StreamedKeyLib> pKeyLib;
	TBOOL                                        bAnimationsLoaded;

	TINT             iLODCount;
	Toshi::TModelLOD aLODs[ 5 ];
	TFLOAT           aLODDistances[ 5 ];

	TINT                        iNumCollisionMeshes;
	Toshi::TModelCollisionData* pCollisionMeshes;

	ResourceLoader::Textures vecUsedTextures;

	ModelType eModelType;
};

struct ModelInstance
{
	ModelInstance() = default;
	~ModelInstance();

	Toshi::T2SharedPtr<ResourceLoader::Model> pModel;
	Toshi::TTransformObject                   oTransform;
	Toshi::TSkeletonInstance*                 pSkeletonInstance;
};

Toshi::T2SharedPtr<ResourceLoader::Model> Model_Load_Barnyard_Windows( PTRB* pTRB, ModelType eModelType );
Toshi::T2SharedPtr<ResourceLoader::Model> Model_Load_Barnyard_PS2( PTRB* pTRB );
Toshi::T2SharedPtr<ResourceLoader::Model> Model_LoadSkin_GLTF( Toshi::T2StringView pchFilePath );
TBOOL                                     Model_PrepareAnimations( ResourceLoader::Model* pModel );
TBOOL                                     Model_CreateInstance( Toshi::T2SharedPtr<ResourceLoader::Model> pModel, ModelInstance& rOutInstance );

void ModelLoader_RegisterMaterial();

void ModelLoader_SetTKLBuilder( TKLBuilder* pTKLBuilder );

}
