#include "pch.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "ImGuiFileDialog.h"
#include "WindowManager.h"

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

		WindowManager::CreateSingleton();

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

		// Create main dock space
		ImGuiViewport* imViewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos( imViewport->Pos );
		ImGui::SetNextWindowSize( imViewport->Size );
		ImGui::SetNextWindowViewport( imViewport->ID );

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar;
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.0f, 0.0f ) );
		ImGui::Begin( "DockSpace", TNULL, window_flags );
		ImGui::PopStyleVar();

		ImGuiID dockspaceId = ImGui::GetID( "MainDockspace" );
		ImGui::DockSpace( dockspaceId );

		if ( ImGui::BeginMainMenuBar() )
		{
			if ( ImGui::BeginMenu( "File" ) )
			{
				if ( ImGui::MenuItem( "Open..." ) )
				{
					IGFD::FileDialogConfig config;
					config.path = ".";
					
					ImGuiFileDialog::Instance()->OpenDialog( "ChooseTRBFile", "Choose File", ".trb,.ttl,.trz", config );
				}

				ImGui::EndMenu();
			}

			if ( ImGui::BeginMenu( "Batch" ) )
			{
				if ( ImGui::MenuItem( "Decompress files..." ) )
				{
					IGFD::FileDialogConfig config;
					config.countSelectionMax = 0;
					config.path              = ".";

					ImGuiFileDialog::Instance()->OpenDialog( "ChooseTRBFiles", "Choose Files", ".trb,.ttl,.trz", config );
				}

				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}

		if ( ImGuiFileDialog::Instance()->Display( "ChooseTRBFile" ) )
		{
			if ( ImGuiFileDialog::Instance()->IsOk() )
			{
				std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
				std::string filePath     = ImGuiFileDialog::Instance()->GetCurrentPath();

				TRBFileWindow* pFileWindow = new TRBFileWindow();
				TBOOL          bLoaded     = pFileWindow->LoadFile( filePathName.c_str() );

				if ( !bLoaded )
				{
					delete pFileWindow;
					pFileWindow = TNULL;
				}

				WindowManager::GetSingleton()->AddWindow( pFileWindow );
			}

			// close
			ImGuiFileDialog::Instance()->Close();
		}


		if ( ImGuiFileDialog::Instance()->Display( "ChooseTRBFiles" ) )
		{
			if ( ImGuiFileDialog::Instance()->IsOk() )
			{
				auto selections = ImGuiFileDialog::Instance()->GetSelection();

				for ( auto& selection : selections )
				{
					PTRB file( selection.second );
					file.WriteToFile( selection.second, TFALSE );
				}
			}

			// close
			ImGuiFileDialog::Instance()->Close();
		}

		WindowManager::GetSingleton()->Render();
		
		ImGui::End();

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
	TMemory::Initialise( 64 * 1024 * 1024, 0 );

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
