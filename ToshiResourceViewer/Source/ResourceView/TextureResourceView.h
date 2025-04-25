#pragma once
#include "TRB/TRBResourceView.h"
#include "ImGuiUtils.h"

#include <ToshiTools/T2DynamicVector.h>
#include <Toshi/TDList.h>
#include <Toshi/TString8.h>

#include <Platform/GL/T2GLTexture_GL.h>

class TextureResourceView
    : public TRBResourceView
{
private:
	struct Texture
	{
		Toshi::TString8 strName;
		Toshi::T2GLTexture oTexture;
		TINT               iWidth;
		TINT               iHeight;
	};

public:
	TextureResourceView();
	~TextureResourceView();

	virtual TBOOL OnCreate() OVERRIDE;
	virtual TBOOL CanSave() OVERRIDE;
	virtual TBOOL OnSave( PTRB* pOutTRB ) OVERRIDE;
	virtual void  OnDestroy() OVERRIDE;
	virtual void  OnRender( TFLOAT flDeltaTime ) OVERRIDE;

private:
	TBOOL LoadTTL();

private:
	Toshi::TString8                 m_strPackName;
	Toshi::T2DynamicVector<Texture> m_vecTextures;
	TINT                            m_iSelectedTexture = 0;
	TBOOL                           m_bDocked          = TFALSE;
	ImGuiUtils::ImGuiComponent      m_uiDockspace;
	Toshi::T2FormatString64         m_strTexturesId;
	Toshi::T2FormatString64         m_strPreviewId;
};
