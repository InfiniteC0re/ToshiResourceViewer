#pragma once
#include "TRB/TRBResourceView.h"
#include "ResourceLoader/ModelLoader.h"
#include "ImGuiUtils.h"

#include <Render/TModel.h>
#include <Render/TTMDWin.h>

#include <Toshi/TDList.h>
#include <ToshiTools/tinyxml2.h>

#include <tiny_gltf.h>

class ModelResourceView
    : public TRBResourceView
{
public:
	ModelResourceView();
	~ModelResourceView();

	virtual TBOOL OnCreate( Toshi::T2StringView pchFilePath ) OVERRIDE;
	virtual TBOOL CanSave() OVERRIDE;
	virtual TBOOL OnSave( PTRB* pOutTRB ) OVERRIDE;
	TBOOL         OnSaveWorld( PTRB* pOutTRB );
	// Writes SkeletonHeader + Skeleton symbols (shared by skin and world save paths)
	void          WriteSkeletonSymbols( PTRB* pOutTRB, PTRBSections::MemoryStream* pMemStream, PTRBSymbols* pSYMB );
	// Writes the Collision header/meshes + baked CollisionTree (shared by skin and world save paths)
	void          WriteCollisionSymbols( PTRB* pOutTRB, PTRBSections::MemoryStream* pMemStream, PTRBSymbols* pSYMB );
	virtual void  OnDestroy() OVERRIDE;
	virtual void  OnRender( TFLOAT flDeltaTime ) OVERRIDE;

	void  OnSaveTKL( PTRB* pOutTRB );
	// Which loader an external .gltf uses (World/Grass -> world, else skin)
	void  SetExternalGltfType( ResourceLoader::ModelType a_eType ) { m_eExternalGltfType = a_eType; }
	TBOOL HasKeyLibrary() const { return m_ModelInstance.pModel.IsValid() && m_ModelInstance.pModel->pKeyLib.IsValid(); }
	void  SetAutoSaveTKL( TBOOL bAutoSave ) { m_bAutoSaveTKL = bAutoSave; }
	TBOOL ExportScene( tinygltf::Model& rOutModel );

	Toshi::TPString8 GetTKLName();
	TBOOL            TryFixingMissingTKL();

	void SerializeModelInformation( tinyxml2::XMLDocument* pOutput );
	void DeserializeModelInformation( tinyxml2::XMLDocument* pInput );

private:
	// Loads the model referenced by a decompiled XML, applying the animation and
	// bone filters it lists so a shared merged GLTF previews as just this model
	void LoadModelFromXML( const TCHAR* pchXMLPath );

	Toshi::T2FormatString64       m_strDockspaceId;
	Toshi::T2FormatString64       m_strSequencesId;
	Toshi::T2FormatString64       m_strViewportId;
	Toshi::T2FormatString64       m_strPreferencesId;
	ResourceLoader::ModelInstance m_ModelInstance;
	TINT                          m_iSelectedSequence;

	TBOOL  m_bAutoSaveTKL;
	ResourceLoader::ModelType m_eExternalGltfType = ResourceLoader::ModelType::Skin;
	TFLOAT m_fWorldChunkSize = 0.0f; // >0 tiles world meshes into cells of this size on compile

	// Texture path per material name from the XML, used to override the (often
	// stripped) glTF texture path on save
	Toshi::T2Map<Toshi::TPString8, Toshi::TString8, Toshi::TPString8::Comparator> m_mapXMLTextureOverrides;

	Toshi::T2Camera        m_oCamera;
	Toshi::T2RenderContext m_oRenderContext;
	Toshi::T2FrameBuffer   m_ViewportFrameBuffer;

	Toshi::TVector4 m_vecCameraCenter;
	TFLOAT          m_fCameraDistance;
	TFLOAT          m_fCameraDistanceTarget;
	TFLOAT          m_fCameraRotX;
	TFLOAT          m_fCameraRotY;
	TFLOAT          m_fCameraFOV;

	TBOOL m_bDockingSetUp;

	TBOOL  m_bWireFrame;
	TFLOAT m_flWireframeThickness;

	TBOOL m_bDisableTextures;

	Toshi::TVector4 m_vViewportColor;

	ImGuiID m_DockRight;
	ImGuiID m_DockLeftBottom;
	ImGuiID m_DockLeft;
};
