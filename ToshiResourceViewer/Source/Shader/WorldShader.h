#pragma once
#include "Resource/StreamedTexture.h"
#include "Resource/Material.h"
#include "Mesh.h"

#include <Toshi/TSingleton.h>
#include <Render/TMesh.h>
#include <Render/TShader.h>
#include <ToshiTools/T2DynamicVector.h>

#include <Platform/GL/T2Shader_GL.h>
#include <Platform/GL/T2GLTexture_GL.h>
#include <Platform/GL/T2RenderBuffer_GL.h>

class WorldMesh : public Mesh
{
public:
	TDECLARE_CLASS( WorldMesh, Mesh );

	struct SubMesh
	{
		Toshi::T2IndexBuffer oIndexBuffer;
		Toshi::T2VertexArray oVertexArray;
		TUINT32              uiNumIndices;
		TUINT32              uiNumVertices;
	};

	struct WorldVertex
	{
		Toshi::TVector3 Position;
		Toshi::TVector3 Normal;
		Toshi::TVector3 Color;
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
	virtual TBOOL SerializeTRBMesh( PTRB* a_pTRB, PTRBSections::MemoryStream::Ptr<Toshi::TTMDWin::TRBMeshLODHeader> a_pMesh ) OVERRIDE;
	virtual void  GetMaterialInfo( Toshi::TString8& a_rMatName, Toshi::TString8& a_rTexName ) OVERRIDE;

	TBOOL IsWater() const { return m_bIsWater; }
	void  SetIsWater( TBOOL a_bIsWater ) { m_bIsWater = a_bIsWater; }

public:
	Toshi::T2VertexBuffer           oVertexBuffer;
	Toshi::T2DynamicVector<SubMesh> vecSubMeshes;

private:
	TBOOL m_bIsWater = TFALSE;
};

class WorldMaterial
	: public Resource::Material
{
public:
	TDECLARE_CLASS( WorldMaterial, Resource::Material );

public:
	static constexpr TUINT MAX_TEXTURES = 4;
	using BLENDMODE = TINT;

public:
	WorldMaterial();

	virtual void PreRender() OVERRIDE;
	virtual void PostRender() OVERRIDE;

	void      SetOrderTable( Toshi::TOrderTable* a_pOrderTable );
	void      SetBlendMode( BLENDMODE a_eBlendMode );
	BLENDMODE GetBlendMode() const { return m_eBlendMode; }

	void SetTexture( TUINT a_uiIndex, Toshi::T2SharedPtr<Resource::StreamedTexture> a_pTexture )
	{
		TASSERT( a_uiIndex < MAX_TEXTURES );
		m_aTextures[ a_uiIndex ] = a_pTexture;
	}

	void SetTexture( Toshi::T2SharedPtr<Resource::StreamedTexture> a_pTexture )
	{
		SetTexture( 0, a_pTexture );
	}

	Resource::StreamedTexture* AccessTexture( TUINT a_uiIndex = 0 )
	{
		TASSERT( a_uiIndex < MAX_TEXTURES );
		return m_aTextures[ a_uiIndex ].Get();
	}

	WorldMaterial* GetAlphaBlendMaterial() const { return m_pAlphaBlendMaterial; }
	void           SetAlphaBlendMaterial( WorldMaterial* a_pMaterial ) { m_pAlphaBlendMaterial = a_pMaterial; }

	TBOOL IsAlphaBlend() const { return m_bIsAlphaBlend; }
	void  SetIsAlphaBlend( TBOOL a_bEnable ) { m_bIsAlphaBlend = a_bEnable; }

	void SetUVAnimSpeed( TFLOAT a_fSpeedX, TFLOAT a_fSpeedY )
	{
		m_fUVAnimSpeedX = a_fSpeedX;
		m_fUVAnimSpeedY = a_fSpeedY;
	}

	TFLOAT GetUVOffsetX() const { return m_fUVAnimX; }
	TFLOAT GetUVOffsetY() const { return m_fUVAnimY; }

private:
	WorldMaterial*                                m_pAlphaBlendMaterial;
	Toshi::TOrderTable*                           m_pAssignedOrderTable;
	Toshi::T2SharedPtr<Resource::StreamedTexture> m_aTextures[ MAX_TEXTURES ];
	BLENDMODE                                     m_eBlendMode;
	TBOOL                                         m_bIsAlphaBlend;
	TFLOAT                                        m_fUVAnimX;
	TFLOAT                                        m_fUVAnimY;
	TFLOAT                                        m_fUVAnimSpeedX;
	TFLOAT                                        m_fUVAnimSpeedY;
};

class WorldShader
    : public Toshi::TShader
    , public Toshi::TSingleton<WorldShader>
{
public:
	TDECLARE_CLASS( WorldShader, Toshi::TShader );

	static constexpr TUINT NUM_ORDER_TABLES = 9;

public:
	WorldShader();
	~WorldShader();

	// TShader
	virtual TBOOL Validate() OVERRIDE;
	virtual void  Invalidate() OVERRIDE;
	virtual void  StartFlush() OVERRIDE;
	virtual void  EndFlush() OVERRIDE;
	virtual TBOOL Create() OVERRIDE;
	virtual void  Render( Toshi::TRenderPacket* a_pRenderPacket ) OVERRIDE;

	WorldMesh*                        CreateMesh();
	Toshi::T2SharedPtr<WorldMaterial> CreateMaterial();

	void  EnableRenderEnvMap( TBOOL a_bEnable ) { m_bRenderEnvMap = a_bEnable; }
	TBOOL IsRenderEnvMapEnabled() const { return m_bRenderEnvMap; }

	TBOOL IsAlphaBlendMaterial() const { return m_bAlphaBlendMaterial; }
	void  SetAlphaBlendMaterial( TBOOL a_bEnable ) { m_bAlphaBlendMaterial = a_bEnable; }

	void SetColours( const Toshi::TVector4& a_rShadowColour, const Toshi::TVector4& a_rAmbientColour );

	const Toshi::TVector4& GetShadowColour() const { return m_ShadowColour; }
	const Toshi::TVector4& GetAmbientColour() const { return m_AmbientColour; }

	TUINT GetAlphaRef() const { return m_iAlphaRef; }
	void  SetAlphaRef( TUINT a_uiAlphaRef ) { m_iAlphaRef = a_uiAlphaRef; }

	Toshi::TOrderTable* GetOrderTable( TUINT a_uiIndex )
	{
		TASSERT( a_uiIndex < NUM_ORDER_TABLES );
		return &m_aOrderTables[ a_uiIndex ];
	}

	Toshi::T2Shader& GetShaderProgram() { return m_oShaderProgram; }

	inline static TUINT s_RenderStateFlags = 27;

private:
	Toshi::T2CompiledShader m_hVertexShader;
	Toshi::T2CompiledShader m_hFragmentShader;
	Toshi::T2Shader         m_oShaderProgram;

	Toshi::TOrderTable m_aOrderTables[ NUM_ORDER_TABLES ];
	Toshi::TVector4    m_ShadowColour;
	Toshi::TVector4    m_AmbientColour;

	TUINT m_iAlphaRef;
	TBOOL m_bRenderEnvMap;
	TBOOL m_bAlphaBlendMaterial;
};

TSINGLETON_DECLARE_ALIAS( WorldShader, WorldShader );
