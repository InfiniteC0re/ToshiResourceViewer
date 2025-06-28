#pragma once
#include "Resource/StreamedKeyLib.h"

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
	Toshi::TSkeleton*                            pSkeleton;
	TINT                                         iLODCount;
	Toshi::TModelLOD                             aLODs[ 5 ];
	TFLOAT                                       fLODDistance;
	TFLOAT                                       aLODDistances[ 4 ];
	TINT                                         iNumCollisionMeshes;
	Toshi::TModelCollisionData*                  pCollisionMeshes;
	PTRB*                                        pTRB;
	Toshi::T2SharedPtr<Resource::StreamedKeyLib> pKeyLib;
};

struct ModelInstance
{
	Toshi::T2SharedPtr<ResourceLoader::Model> pModel;
	Toshi::TTransformObject                   oTransform;
};

Toshi::T2SharedPtr<ResourceLoader::Model> Model_Load_Barnyard_Windows( PTRB* pTRB, Endianess eEndianess );
TBOOL                                     Model_CreateInstance( Toshi::T2SharedPtr<ResourceLoader::Model> pModel, ModelInstance& rOutInstance );

}
