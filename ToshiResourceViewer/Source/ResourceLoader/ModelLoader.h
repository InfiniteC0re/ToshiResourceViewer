#pragma once
#include <Render/TModel.h>
#include <Render/TModelCollision.h>
#include <Render/TSkeleton.h>
#include <Toshi/T2SharedPtr.h>
#include <Toshi/T2String.h>
#include <Plugins/PTRB.h>

namespace ResourceLoader
{

struct ModelLOD
{
	Toshi::TSphere BoundingSphere = Toshi::TSphere( 0.0f, 0.0f, 0.0f, 0.0f );
	TINT           iNumMeshes     = 0;
	Toshi::TMesh** ppMeshes       = TNULL;
};

class Model
{
public:
	Model();
	~Model();

	void Render();

public:
	Toshi::TSkeleton*           pSkeleton;
	TINT                        iLODCount;
	ModelLOD                    aLODs[ 5 ];
	TFLOAT                      fLODDistance;
	TFLOAT                      aLODDistances[ 4 ];
	TINT                        iNumCollisionMeshes;
	Toshi::TModelCollisionData* pCollisionMeshes;
	PTRB*                       pTRB;
};

struct ModelInstance
{
	Toshi::T2SharedPtr<ResourceLoader::Model> pModel;
	Toshi::TTransformObject                   oTransform;
};

Toshi::T2SharedPtr<ResourceLoader::Model> Model_Load_Barnyard_Windows( PTRB* pTRB, Endianess eEndianess );
TBOOL                                     Model_CreateInstance( Toshi::T2SharedPtr<ResourceLoader::Model> pModel, ModelInstance& rOutInstance );

}
