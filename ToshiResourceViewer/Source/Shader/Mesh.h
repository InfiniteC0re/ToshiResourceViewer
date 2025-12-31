#pragma once
#include "tiny_gltf.h"

#include <Render/TMesh.h>

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
	virtual TBOOL SerializeGLTFMesh( tinygltf::Model& a_rOutModel ) = 0;

	const char* GetName() const { return m_strName; }
	void SetName( const char* a_pchName ) { m_strName = a_pchName; }

private:
	Toshi::TString8 m_strName;
};
