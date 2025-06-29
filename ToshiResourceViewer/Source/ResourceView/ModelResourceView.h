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
	Toshi::TString8               m_strTKLName;
	Toshi::T2FormatString64       m_strAnimationsId;
	ResourceLoader::ModelInstance m_ModelInstance;
	TINT                          m_iSelectedSequence;

	Toshi::T2Camera        m_oCamera;
	Toshi::T2RenderContext m_oRenderContext;
	Toshi::T2FrameBuffer   m_ViewportFrameBuffer;
	
	Toshi::TVector4 m_vecCameraCenter;
	TFLOAT          m_fCameraDistance;
	TFLOAT          m_fCameraDistanceTarget;
	TFLOAT          m_fCameraRotX;
	TFLOAT          m_fCameraRotY;
	TFLOAT          m_fCameraFOV;
};
