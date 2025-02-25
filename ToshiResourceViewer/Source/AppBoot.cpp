#include "pch.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <Toshi/Toshi.h>
#include <Toshi/TApplication.h>
#include <Render/T2Render.h>
#include <ToshiTools/T2CommandLine.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

class Application
    : public TApplication
    , public T2Window::EventListener
{
public:
	//-----------------------------------------------------------------------------
	// T2Window::EventListener
	//-----------------------------------------------------------------------------
	virtual TBOOL OnEvent( const SDL_Event& event ) OVERRIDE
	{
		ImGui_ImplSDL2_ProcessEvent( &event );

		if ( event.type == SDL_QUIT )
			g_oTheApp.Destroy();

		return TTRUE;
	}

	//-----------------------------------------------------------------------------
	// Toshi::TApplication
	//-----------------------------------------------------------------------------
	virtual TBOOL OnCreate( TINT argc, TCHAR** argv ) OVERRIDE
	{
		TApplication::OnCreate( argc, argv );

		T2Render::WindowParams windowParams;
		windowParams.pchTitle    = "Toshi Resource Viewer";
		windowParams.uiWidth     = 800;
		windowParams.uiHeight    = 600;
		windowParams.bIsWindowed = TTRUE;

		T2Render* pRender        = T2Render::CreateSingleton();
		TBOOL     bWindowCreated = pRender->Create( windowParams );
		TASSERT( TTRUE == bWindowCreated );

		T2Window* pWindow = pRender->GetWindow();
		pWindow->SetListener( this );

		// Initialise ImGui
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// Setup Platform/Renderer backends
		ImGui_ImplSDL2_InitForOpenGL( pWindow->GetNativeWindow(), pRender->GetGLContext() );
		ImGui_ImplOpenGL3_Init( "#version 130" );

		return bWindowCreated;
	}

	virtual TBOOL OnUpdate( TFLOAT flDeltaTime ) OVERRIDE
	{
		T2Render* pRender = T2Render::GetSingleton();
		T2Window* pWindow = pRender->GetWindow();

		// Update window to get new events
		pRender->Update( flDeltaTime );

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		TBOOL bShowDemoWindow = TTRUE;
		ImGui::ShowDemoWindow( &bShowDemoWindow );

		// Render to the window
		pRender->BeginScene();

		SDL_Window*   backup_current_window  = SDL_GL_GetCurrentWindow();
		SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
		
		ImGui::Render();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();

		ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
		SDL_GL_MakeCurrent( backup_current_window, backup_current_context );

		pRender->EndScene();

		return TTRUE;
	}

} g_oTheApp;

int main( int argc, char** argv )
{
	// Allocate memory for the allocator
	TMemory::Initialise( 8 * 1024 * 1024, 0 );

	// Initialise engine
	TUtil::TOSHIParams engineParams;
	engineParams.szLogAppName  = "TRV";
	engineParams.szLogFileName = "tresourceviewer";
	engineParams.szCommandLine = GetCommandLineA();

	TUtil::ToshiCreate( engineParams );
	g_oTheApp.Create( "Toshi Resource Viewer", argc, argv );
	g_oTheApp.Execute();

	//TUtil::ToshiDestroy();

	return 0;
}
