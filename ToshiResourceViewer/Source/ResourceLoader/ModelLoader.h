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
	TFLOAT           fLODDistance;
	TFLOAT           aLODDistances[ 4 ];

	TINT                        iNumCollisionMeshes;
	Toshi::TModelCollisionData* pCollisionMeshes;

	ResourceLoader::Textures vecUsedTextures;
};

struct ModelInstance
{
	ModelInstance() = default;
	~ModelInstance();

	Toshi::T2SharedPtr<ResourceLoader::Model> pModel;
	Toshi::TTransformObject                   oTransform;
	Toshi::TSkeletonInstance*                 pSkeletonInstance;
};

Toshi::T2SharedPtr<ResourceLoader::Model> Model_Load_Barnyard_Windows( PTRB* pTRB, Endianess eEndianess );
Toshi::T2SharedPtr<ResourceLoader::Model> Model_LoadSkin_GLTF( Toshi::T2StringView pchFilePath );
TBOOL                                     Model_PrepareAnimations( ResourceLoader::Model* pModel );
TBOOL                                     Model_CreateInstance( Toshi::T2SharedPtr<ResourceLoader::Model> pModel, ModelInstance& rOutInstance );

}
