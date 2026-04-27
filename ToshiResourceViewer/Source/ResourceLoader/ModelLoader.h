#pragma once
#include "Resource/StreamedKeyLib.h"
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
	FOB,
};

TFORCEINLINE const TCHAR* GetModelTypeName( ModelType eModelType )
{
	switch ( eModelType )
	{
		case ModelType::Skin: return "Skin";
		case ModelType::World: return "World";
		case ModelType::Grass: return "Grass";
		case ModelType::FOB: return "FOB";
		default: return "Unknown";
	}
}

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

	ResourceLoader::Textures                    vecUsedTextures;
	Toshi::T2DynamicVector<Toshi::TMaterial*>   vecOwnedMaterials;

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
Toshi::T2SharedPtr<ResourceLoader::Model> Model_LoadSkin_GLTF( Toshi::T2StringView pchFilePath );
TBOOL                                     Model_PrepareAnimations( ResourceLoader::Model* pModel );
TBOOL                                     Model_CreateInstance( Toshi::T2SharedPtr<ResourceLoader::Model> pModel, ModelInstance& rOutInstance );

void ModelLoader_SetTKLBuilder( TKLBuilder* pTKLBuilder );

}
