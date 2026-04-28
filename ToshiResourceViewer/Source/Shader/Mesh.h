#pragma once
#include "tiny_gltf.h"
#include "Resource/Material.h"

#include <Render/TMesh.h>
#include <Render/TTMDWin.h>
#include <Plugins/PTRB.h>

class Mesh
	: public Toshi::TMesh
{
public:
	TDECLARE_CLASS( Mesh, Toshi::TMesh );

private:
	using Toshi::TMesh::SetMaterial;
	using Toshi::TMesh::GetMaterial;

public:
	Mesh() = default;
	virtual ~Mesh() = default;

	//-----------------------------------------------------------------------------
	// Own methods
	//-----------------------------------------------------------------------------
	virtual TBOOL SerializeGLTFMesh( tinygltf::Model& a_rOutModel, Toshi::TSkeletonInstance* a_pSkeletonInstance )            = 0;
	virtual TBOOL SerializeTRBMesh( PTRB* a_pTRB, PTRBSections::MemoryStream::Ptr<Toshi::TTMDWin::TRBMeshLODHeader> a_pMesh ) = 0;
	virtual void  GetMaterialInfo( Toshi::TString8& a_rMatName, Toshi::TString8& a_rTexName )                                 = 0;

	const char* GetName() const { return m_strName; }
	void SetName( const char* a_pchName ) { m_strName = a_pchName; }

	const char* GetMaterialName() const { return m_strMaterialName; }
	void SetMaterialName( const char* a_pchName ) { m_strMaterialName = a_pchName; }

	Toshi::T2SharedPtr<Resource::Material> GetMaterial() { return m_pMaterial; }
	void                                   SetMaterial( Toshi::T2SharedPtr<Resource::Material> a_pMaterial ) { m_pMaterial = a_pMaterial; }

private:
	Toshi::TString8 m_strName;
	Toshi::TString8 m_strMaterialName;

	Toshi::T2SharedPtr<Resource::Material> m_pMaterial;
};
