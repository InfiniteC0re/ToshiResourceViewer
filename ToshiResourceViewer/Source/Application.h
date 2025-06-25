#pragma once
#include <Toshi/TApplication.h>
#include <Render/T2Render.h>

enum TOSHIGAME
{
	TOSHIGAME_BARNYARD,
	TOSHIGAME_DEBLOB,
	// ...
	TOSHIGAME_NUMOF
};

enum TOSHISKU
{
	TOSHISKU_WINDOWS,
	TOSHISKU_REV, // Wii/GameCube
	// ...
	TOSHISKU_NUMOF
};

class Application
    : public Toshi::TApplication
    , public Toshi::T2Window::EventListener
{
public:
	//-----------------------------------------------------------------------------
	// T2Window::EventListener
	//-----------------------------------------------------------------------------
	virtual TBOOL OnEvent( const SDL_Event& event ) OVERRIDE;

	//-----------------------------------------------------------------------------
	// Toshi::TApplication
	//-----------------------------------------------------------------------------
	virtual TBOOL OnCreate( TINT argc, TCHAR** argv ) OVERRIDE;
	virtual TBOOL OnUpdate( TFLOAT flDeltaTime ) OVERRIDE;

	TOSHIGAME GetSelectedGame() const { return m_eSelectedGame; }
	TOSHISKU  GetSelectedPlatform() const { return m_eSelectedPlatform; }

	void SetSelectedGame( TOSHIGAME a_eValue ) { m_eSelectedGame = a_eValue; }
	void SetSelectedPlatform( TOSHISKU a_eValue ) { m_eSelectedPlatform = a_eValue; }

private:
	TOSHIGAME m_eSelectedGame     = TOSHIGAME_BARNYARD;
	TOSHISKU  m_eSelectedPlatform = TOSHISKU_WINDOWS;
};

extern Application g_oTheApp;
