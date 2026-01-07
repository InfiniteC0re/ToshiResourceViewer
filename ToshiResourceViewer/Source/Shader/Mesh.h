#pragma once
#include "tiny_gltf.h"

#include <Render/TMesh.h>
#include <Render/TTMDWin.h>
#include <Plugins/PTRB.h>

class Mesh
	: public Toshi::TMesh
{
public:
	TDECLARE_CLASS( Mesh, Toshi::TMesh );

public:
	Mesh() = default;
	virtual ~Mesh() = default;

	//-----------------------------------------------------------------------------
	// Own methods
	//-----------------------------------------------------------------------------
	virtual TBOOL SerializeGLTFMesh( tinygltf::Model& a_rOutModel, Toshi::TSkeletonInstance* a_pSkeletonInstance )      = 0;
	virtual TBOOL SerializeTRBMesh( PTRB* a_pTRB, PTRBSections::MemoryStream::Ptr<Toshi::TTMDWin::TRBLODMesh> a_pMesh ) = 0;
	virtual void  GetMaterialInfo( Toshi::TString8& a_rMatName, Toshi::TString8& a_rTexName )                           = 0;

	const char* GetName() const { return m_strName; }
	void SetName( const char* a_pchName ) { m_strName = a_pchName; }

	const char* GetMaterialName() const { return m_strMaterialName; }
	void SetMaterialName( const char* a_pchName ) { m_strMaterialName = a_pchName; }

private:
	Toshi::TString8 m_strName;
	Toshi::TString8 m_strMaterialName;
};
