#pragma once
#include "Resource/StreamedTexture.h"
#include "Mesh.h"

#include <Toshi/TSingleton.h>
#include <Render/TMesh.h>
#include <Render/TShader.h>
#include <ToshiTools/T2DynamicVector.h>

#include <Platform/GL/T2Shader_GL.h>
#include <Platform/GL/T2GLTexture_GL.h>
#include <Platform/GL/T2RenderBuffer_GL.h>

#define SKINNED_SUBMESH_MAX_BONES 28

class SkinMesh : public Mesh
{
public:
	TDECLARE_CLASS( SkinMesh, Mesh );

	struct SubMesh
	{
		Toshi::T2IndexBuffer  oIndexBuffer;
		Toshi::T2VertexArray  oVertexArray;
		TUINT32               uiNumAllocatedVertices;
		TUINT32               uiEndVertexId;
		TUINT32               uiNumIndices;
		TUINT32               uiNumBones;
		TINT                  aBones[ SKINNED_SUBMESH_MAX_BONES ];
	};

	struct SkinVertex
	{
		Toshi::TVector3 Position;
		Toshi::TVector3 Normal;
		TUINT8          Weights[ 4 ];
		TUINT8          Bones[ 4 ];
		Toshi::TVector2 UV;
	};

public:
	//-----------------------------------------------------------------------------
	// Toshi::TMesh
	//-----------------------------------------------------------------------------
	virtual TBOOL Render() OVERRIDE;
	virtual void  OnDestroy() OVERRIDE;

	//-----------------------------------------------------------------------------
	// Mesh
	//-----------------------------------------------------------------------------
	virtual TBOOL SerializeGLTFMesh( tinygltf::Model& a_rOutModel, Toshi::TSkeletonInstance* a_pSkeletonInstance ) OVERRIDE;
	virtual TBOOL SerializeTRBMesh( PTRB* a_pTRB, PTRBSections::MemoryStream::Ptr<Toshi::TTMDWin::TRBLODMesh> a_pMesh ) OVERRIDE;
	virtual void GetMaterialInfo( Toshi::TString8& a_rMatName, Toshi::TString8& a_rTexName ) OVERRIDE;

public:
	Toshi::T2VertexBuffer           oVertexBuffer;
	Toshi::T2DynamicVector<SubMesh> vecSubMeshes;
};

class SkinMaterial
    : public Toshi::TMaterial
{
public:
	virtual void PreRender() OVERRIDE;

	void SetOrderTable( Toshi::TOrderTable* pOrderTable )
	{
		if ( pOrderTable != m_pAssignedOrderTable )
		{
			if ( m_pAssignedOrderTable )
			{
				Toshi::TOrderTable::DeregisterMaterial( GetRegMaterial() );
			}

			if ( pOrderTable )
				pOrderTable->RegisterMaterial( this );

			m_pAssignedOrderTable = pOrderTable;
		}
	}

	void SetTexture( Toshi::T2SharedPtr<Resource::StreamedTexture> pTexture )
	{
		m_pTexture = pTexture;
	}

	Resource::StreamedTexture* AccessTexture() { return m_pTexture; }

private:
	Toshi::TOrderTable*                           m_pAssignedOrderTable;
	Toshi::T2SharedPtr<Resource::StreamedTexture> m_pTexture;
};

class SkinShader
    : public Toshi::TShader
    , public Toshi::TSingleton<SkinShader>
{
public:
	TDECLARE_CLASS( SkinShader, Toshi::TShader );

public:
	SkinShader();
	~SkinShader();

	// TShader
	virtual TBOOL Validate() OVERRIDE;
	virtual void  Invalidate() OVERRIDE;
	virtual void  StartFlush() OVERRIDE;
	virtual void  EndFlush() OVERRIDE;
	virtual TBOOL Create() OVERRIDE;
	virtual void  Render( Toshi::TRenderPacket* a_pRenderPacket ) OVERRIDE;

	SkinMesh*     CreateMesh();
	SkinMaterial* CreateMaterial();

private:
	Toshi::T2CompiledShader m_hVertexShader;
	Toshi::T2CompiledShader m_hFragmentShader;
	Toshi::T2Shader         m_oShaderProgram;

	Toshi::TOrderTable m_aOrderTable;

	Toshi::TMatrix44 m_WorldViewMatrix;
	Toshi::TMatrix44 m_ViewWorldMatrix;
};

TSINGLETON_DECLARE_ALIAS( SkinShader, SkinShader );
