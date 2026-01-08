#pragma once
#include "TRB/TRBResourceView.h"
#include "ResourceLoader/TextureLoader.h"
#include "ImGuiUtils.h"

#include <ToshiTools/T2DynamicVector.h>
#include <Toshi/TDList.h>

class TextureResourceView
    : public TRBResourceView
{
public:
	TextureResourceView();
	~TextureResourceView();

	virtual TBOOL OnCreate( Toshi::T2StringView pchFilePath ) OVERRIDE;
	virtual TBOOL CanSave() OVERRIDE;
	virtual TBOOL OnSave( PTRB* pOutTRB ) OVERRIDE;
	virtual void  OnDestroy() OVERRIDE;
	virtual void  OnRender( TFLOAT flDeltaTime ) OVERRIDE;

private:
	Toshi::TString8          m_strPackName;
	ResourceLoader::Textures m_vecTextures;
	TINT                     m_iSelectedTexture = 0;
	Toshi::T2FormatString64  m_strTexturesId;
	Toshi::T2FormatString64  m_strPreviewId;
	TFLOAT                   m_fOffsetX = 0.0f;
	TFLOAT                   m_fOffsetY = 0.0f;
	TFLOAT                   m_fScale   = 1.0f;
};
