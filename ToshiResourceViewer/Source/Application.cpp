#include "pch.h"
#include "Application.h"

#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "ImGuiFileDialog.h"
#include "TRB/TRBWindowManager.h"

#include <Toshi/Toshi.h>
#include <ToshiTools/T2CommandLine.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

static struct MemoryInitialiser
{
	MemoryInitialiser()
	{
		TMemory::Initialise( 0, 0, TMemoryDL::Flags_NativeMethods );
	}
} g_oMemInitialiser;

Application g_oTheApp;

TBOOL Application::OnEvent( const SDL_Event& event )
{
	ImGui_ImplSDL2_ProcessEvent( &event );

	if ( event.type == SDL_QUIT )
		g_oTheApp.Destroy();

	return TTRUE;
}

TBOOL Application::OnCreate( TINT argc, TCHAR** argv )
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
	SDL_SetWindowResizable( pWindow->GetNativeWindow(), SDL_TRUE );
	pWindow->SetListener( this );

	TRBWindowManager::CreateSingleton();

	// Initialise ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	//io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	io.Fonts->AddFontFromFileTTF( "Resources/Fonts/OpenSans-SemiBold.ttf", 19.0f, TNULL, io.Fonts->GetGlyphRangesCyrillic() );

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// TRV style
	ImGuiStyle& style  = ImGui::GetStyle();
	ImVec4*     colors = style.Colors;

	// Corners
	style.WindowPadding     = ImVec2( 15.0f, 15.0f );
	style.WindowRounding    = 10.0f;
	style.ChildRounding     = 6.0f;
	style.FramePadding      = ImVec2( 8.0f, 7.0f );
	style.FrameRounding     = 8.0f;
	style.ItemSpacing       = ImVec2( 8.0f, 8.0f );
	style.ItemInnerSpacing  = ImVec2( 10.0f, 6.0f );
	style.IndentSpacing     = 25.0f;
	style.ScrollbarSize     = 13.0f;
	style.ScrollbarRounding = 12.0f;
	style.GrabMinSize       = 10.0f;
	style.GrabRounding      = 6.0f;
	style.PopupRounding     = 8.0f;
	style.WindowTitleAlign  = ImVec2( 0.5f, 0.5f );
	style.ButtonTextAlign   = ImVec2( 0.5f, 0.5f );

	style.Colors[ ImGuiCol_Text ]                 = ImVec4( 0.90f, 0.90f, 0.93f, 1.00f );
	style.Colors[ ImGuiCol_TextDisabled ]         = ImVec4( 0.40f, 0.40f, 0.45f, 1.00f );
	style.Colors[ ImGuiCol_WindowBg ]             = ImVec4( 0.12f, 0.12f, 0.14f, 1.00f );
	style.Colors[ ImGuiCol_ChildBg ]              = ImVec4( 0.18f, 0.20f, 0.22f, 0.30f );
	style.Colors[ ImGuiCol_PopupBg ]              = ImVec4( 0.13f, 0.13f, 0.15f, 1.00f );
	style.Colors[ ImGuiCol_Border ]               = ImVec4( 0.30f, 0.30f, 0.35f, 1.00f );
	style.Colors[ ImGuiCol_BorderShadow ]         = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
	style.Colors[ ImGuiCol_FrameBg ]              = ImVec4( 0.18f, 0.18f, 0.20f, 1.00f );
	style.Colors[ ImGuiCol_FrameBgHovered ]       = ImVec4( 0.25f, 0.25f, 0.28f, 1.00f );
	style.Colors[ ImGuiCol_FrameBgActive ]        = ImVec4( 0.30f, 0.30f, 0.34f, 1.00f );
	style.Colors[ ImGuiCol_TitleBg ]              = ImVec4( 0.15f, 0.15f, 0.17f, 1.00f );
	style.Colors[ ImGuiCol_TitleBgCollapsed ]     = ImVec4( 0.10f, 0.10f, 0.12f, 1.00f );
	style.Colors[ ImGuiCol_TitleBgActive ]        = ImVec4( 0.15f, 0.15f, 0.17f, 1.00f );
	style.Colors[ ImGuiCol_MenuBarBg ]            = ImVec4( 0.12f, 0.12f, 0.14f, 1.00f );
	style.Colors[ ImGuiCol_ScrollbarBg ]          = ImVec4( 0.12f, 0.12f, 0.14f, 1.00f );
	style.Colors[ ImGuiCol_ScrollbarGrab ]        = ImVec4( 0.30f, 0.30f, 0.35f, 1.00f );
	style.Colors[ ImGuiCol_ScrollbarGrabHovered ] = ImVec4( 0.40f, 0.40f, 0.45f, 1.00f );
	style.Colors[ ImGuiCol_ScrollbarGrabActive ]  = ImVec4( 0.50f, 0.50f, 0.55f, 1.00f );
	style.Colors[ ImGuiCol_CheckMark ]            = ImVec4( 0.70f, 0.70f, 0.90f, 1.00f );
	style.Colors[ ImGuiCol_SliderGrab ]           = ImVec4( 0.70f, 0.70f, 0.90f, 1.00f );
	style.Colors[ ImGuiCol_SliderGrabActive ]     = ImVec4( 0.80f, 0.80f, 0.90f, 1.00f );
	style.Colors[ ImGuiCol_Button ]               = ImVec4( 0.18f, 0.18f, 0.20f, 1.00f );
	style.Colors[ ImGuiCol_ButtonHovered ]        = ImVec4( 0.60f, 0.60f, 0.90f, 1.00f );
	style.Colors[ ImGuiCol_ButtonActive ]         = ImVec4( 0.28f, 0.56f, 0.96f, 1.00f );
	style.Colors[ ImGuiCol_Header ]               = ImVec4( 0.20f, 0.20f, 0.23f, 1.00f );
	style.Colors[ ImGuiCol_HeaderHovered ]        = ImVec4( 0.25f, 0.25f, 0.28f, 1.00f );
	style.Colors[ ImGuiCol_HeaderActive ]         = ImVec4( 0.30f, 0.30f, 0.34f, 1.00f );
	style.Colors[ ImGuiCol_Separator ]            = ImVec4( 0.40f, 0.40f, 0.45f, 1.00f );
	style.Colors[ ImGuiCol_SeparatorHovered ]     = ImVec4( 0.50f, 0.50f, 0.55f, 1.00f );
	style.Colors[ ImGuiCol_SeparatorActive ]      = ImVec4( 0.60f, 0.60f, 0.65f, 1.00f );
	style.Colors[ ImGuiCol_ResizeGrip ]           = ImVec4( 0.20f, 0.20f, 0.23f, 1.00f );
	style.Colors[ ImGuiCol_ResizeGripHovered ]    = ImVec4( 0.25f, 0.25f, 0.28f, 1.00f );
	style.Colors[ ImGuiCol_ResizeGripActive ]     = ImVec4( 0.30f, 0.30f, 0.34f, 1.00f );
	style.Colors[ ImGuiCol_PlotLines ]            = ImVec4( 0.61f, 0.61f, 0.64f, 1.00f );
	style.Colors[ ImGuiCol_PlotLinesHovered ]     = ImVec4( 0.70f, 0.70f, 0.75f, 1.00f );
	style.Colors[ ImGuiCol_PlotHistogram ]        = ImVec4( 0.61f, 0.61f, 0.64f, 1.00f );
	style.Colors[ ImGuiCol_PlotHistogramHovered ] = ImVec4( 0.70f, 0.70f, 0.75f, 1.00f );
	style.Colors[ ImGuiCol_TextSelectedBg ]       = ImVec4( 0.30f, 0.30f, 0.34f, 1.00f );
	style.Colors[ ImGuiCol_ModalWindowDimBg ]     = ImVec4( 0.10f, 0.10f, 0.12f, 0.80f );
	style.Colors[ ImGuiCol_Tab ]                  = ImVec4( 0.18f, 0.20f, 0.22f, 1.00f );
	style.Colors[ ImGuiCol_TabHovered ]           = ImVec4( 0.60f, 0.60f, 0.90f, 1.00f );
	style.Colors[ ImGuiCol_TabActive ]            = ImVec4( 0.28f, 0.56f, 0.96f, 1.00f );

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForOpenGL( pWindow->GetNativeWindow(), pRender->GetGLContext() );
	ImGui_ImplOpenGL3_Init( "#version 130" );

	return bWindowCreated;
}

TBOOL Application::OnUpdate( TFLOAT flDeltaTime )
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

			TRBWindowManager::GetSingleton()->AddWindow( pFileWindow );
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
				PTRB file( selection.second.c_str() );
				file.WriteToFile( selection.second.c_str(), TFALSE );
			}
		}

		// close
		ImGuiFileDialog::Instance()->Close();
	}

	TRBWindowManager::GetSingleton()->Render();

	ImGui::End();

	// Render to the window
	pRender->BeginScene();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );

	if ( ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
	{
		SDL_Window*   backup_current_window  = SDL_GL_GetCurrentWindow();
		SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		SDL_GL_MakeCurrent( backup_current_window, backup_current_context );
	}

	SDL_Window*   backup_current_window  = SDL_GL_GetCurrentWindow();
	SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();

	pRender->EndScene();

	return TTRUE;
}

int main( int argc, char** argv )
{
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
