#pragma once
#include "TRB/TRBResourceView.h"
#include "ResourceLoader/ModelLoader.h"
#include "ImGuiUtils.h"

#include <Render/TModel.h>
#include <Render/TTMDWin.h>

#include <Toshi/TDList.h>

class ModelResourceView
    : public TRBResourceView
{
public:
	ModelResourceView();
	~ModelResourceView();

	virtual TBOOL OnCreate() OVERRIDE;
	virtual TBOOL CanSave() OVERRIDE;
	virtual TBOOL OnSave( PTRB* pOutTRB ) OVERRIDE;
	virtual void  OnDestroy() OVERRIDE;
	virtual void  OnRender( TFLOAT flDeltaTime ) OVERRIDE;

private:
	Toshi::TString8                           m_strTKLName;
	Toshi::T2SharedPtr<ResourceLoader::Model> m_pModel;

	Toshi::T2Camera        m_oCamera;
	Toshi::T2RenderContext m_oRenderContext;
	Toshi::T2FrameBuffer   m_ViewportFrameBuffer;
	
	Toshi::TVector3 m_vecCameraPosition;
	TFLOAT          m_fCameraFOV;
};
