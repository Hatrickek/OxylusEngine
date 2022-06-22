#include "ui.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ImGuizmo/ImGuizmo.h"

#include <cstdio>
#include <ShObjIdl.h>
#include <glm/gtc/type_ptr.hpp>

#include "core.h"
#include "editor.h"
#include "entity.h"
#include "level.h" //TODO: this is temporary import for accesing the lighting properties.
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "ssao.h"
#include "utility.h"
#include "window.h"
#include "panels/sceneHPanel.h"
#include "meth.h"
//---------------------------------------------------
//Imgui Functions
namespace FlatEngine {
	void UI::InitUI(GLFWwindow* window) {
		// Setup Dear ImGui context
		auto glsl_version = "#version 410";
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		(void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
		//io.ConfigViewportsNoAutoMerge = true;
		//io.ConfigViewportsNoTaskBarIcon = true;

		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		io.Fonts->AddFontFromFileTTF("resources/fonts/Roboto-Medium.ttf", 16.0f);
		io.Fonts->AddFontDefault();
		IM_ASSERT(font != NULL);
		io.Fonts->Build();

		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init(glsl_version);

		SceneHPanel::SetScene(Editor::GetActiveScene());
	}

	void UI::UpdateImgui() {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void UI::RenderImgui() {
		ImGuiIO& io = ImGui::GetIO();
		(void)io;
		auto clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
	}

	void UI::EndImgui() {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	//--------------------------------------
	//Custom Functions

	static bool performance_overlay = true;
	static bool inspector_window = true;
	static bool console_window = true;
	static wchar_t* OpenFileDialog();
	static wchar_t* SaveFileDialog();
	static int m_GizmoType = -1;
	void UI::DrawEditorUI() {
		DrawGizmos();
		ShowImguiDockSpace();
		DrawImguiPerformanceOverlay();
		if (GetEngineState() == EDITING) {
			DrawDebugPanel();
			SceneHPanel::DrawPanel();
			DrawConsoleWindow();
		}
	}
	void UI::DrawGizmos() {
		Entity selectedEntity = SceneHPanel::GetSelectedEntity();
		if (!selectedEntity && m_GizmoType == -1) return;
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::BeginFrame();
		ImGuizmo::Enable(true);
		//ImGuizmo::SetDrawlist();
		//ImGuiIO& io = ImGui::GetIO();
		ImGuizmo::SetRect(0,0, Window::SCR_WIDTH, Window::SCR_HEIGHT);
		const glm::mat4& cameraProjection = Editor::GetEditorCamera()->GetProjectionMatrix();
		glm::mat4 cameraView = Editor::GetEditorCamera()->GetViewMatrix();
		// Entity transform
		auto& tc = selectedEntity.GetComponent<TransformComponent>();
		glm::mat4 transform = tc.GetTransform();

		// Snapping
		bool snap = Input::GetKey(Key::LeftControl);
		float snapValue = 0.5f; // Snap to 0.5m for translation/scale
		// Snap to 45 degrees for rotation
		if (m_GizmoType == ImGuizmo::OPERATION::ROTATE)
			snapValue = 45.0f;
		float snapValues[3] = { snapValue, snapValue, snapValue };

			ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection),
				(ImGuizmo::OPERATION)m_GizmoType, ImGuizmo::LOCAL, glm::value_ptr(transform),
				nullptr, snap ? snapValues : nullptr);

			if (ImGuizmo::IsUsing())
			{
				glm::vec3 translation, rotation, scale;
				Math::DecomposeTransform(transform, translation, rotation, scale);

				glm::vec3 deltaRotation = rotation - tc.Rotation;
				tc.Translation = translation;
				tc.Rotation += deltaRotation;
				tc.Scale = scale;
			}
			if(Input::GetKeyDown(Key::Q)) 
			{
				if (!ImGuizmo::IsUsing())
					m_GizmoType = -1;
			}
			if(Input::GetKeyDown(Key::W))
			{
				if (!ImGuizmo::IsUsing())
					m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
			}
			if(Input::GetKeyDown(Key::E))
			{
				if (!ImGuizmo::IsUsing())
					m_GizmoType = ImGuizmo::OPERATION::ROTATE;
			}
			if(Input::GetKeyDown(Key::R))
			{
				if (!ImGuizmo::IsUsing())
					m_GizmoType = ImGuizmo::OPERATION::SCALE;
			}
	}
	void UI::DrawDebugPanel() {
		ImGui::Begin("DebugInfo");
		ImGui::Text("SSAO");
		ImGui::SameLine();
		static bool ssao_toggle = true;
		if (ImGui::Checkbox("Toggle", &ssao_toggle)) {
			SSAO::ssao_radius = ssao_toggle ? 0.5 : 0;
		}

		static int kernelSize = 64;
		ImGui::SliderInt("Kernel size", &kernelSize, 16, 256, "%d");
		SSAO::SetKernelSize(kernelSize);

		static float radius = 0.5f;
		ImGui::SliderFloat("Radius", &radius, 0.0f, 1.0f, "%.2f");
		if (ssao_toggle) SSAO::SetRadius(radius);

		static float sample = 64;
		ImGui::SliderFloat("Samples", &sample, 16, 256, "%.0f");

		ImGui::Separator();

		ImGui::Text("Lighting");
		ImGui::PushItemWidth(80);
		ImGui::Text("Position");
		ImGui::SliderFloat(":x", &lightPositions[0].x, -10, 10, "%.2f");
		ImGui::SameLine();
		ImGui::SliderFloat(":y", &lightPositions[0].y, -10, 10, "%.2f");
		ImGui::SameLine();
		ImGui::SliderFloat(":z", &lightPositions[0].z, -10, 10, "%.2f");
		ImGui::Text("Color");
		ImGui::SliderFloat(":r", &lightColors[0].x, 0, 1, "%.2f");
		ImGui::SameLine();
		ImGui::SliderFloat(":g", &lightColors[0].y, 0, 1, "%.2f");
		ImGui::SameLine();
		ImGui::SliderFloat(":b", &lightColors[0].z, 0, 1, "%.2f");
		ImGui::PopItemWidth();
		ImGui::End();
	}

	void UI::DrawConsoleWindow() {
		if (!console_window) return;
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoScrollbar;

		static char InputBuf[256];
		ImGui::Begin("Console", NULL, window_flags);
		float m_height = ImGui::GetWindowHeight();
		float m_width = ImGui::GetWindowWidth();

		ImGui::BeginChild("Console window", ImVec2(0, m_height - 60), false, ImGuiWindowFlags_AlwaysAutoResize);

		ImGui::EndChild();
		ImGui::PushItemWidth(m_width - 10);
		ImGui::InputText("##", InputBuf, FE_ARRAYSIZE(InputBuf));
		ImGui::PopItemWidth();
		ImGui::End();
	}

	void UI::DrawImguiPerformanceOverlay() {
		if (!performance_overlay) return;
		static int corner = 2;
		ImGuiIO& io = ImGui::GetIO();
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav;
		if (corner != -1) {
			const float PAD_X = 20;
			const float PAD_Y = 40;
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
			ImVec2 work_size = viewport->WorkSize;
			ImVec2 window_pos, window_pos_pivot;
			window_pos.x = (corner & 1) ? (work_pos.x + work_size.x - PAD_X) : (work_pos.x + PAD_X);
			window_pos.y = (corner & 2) ? (work_pos.y + work_size.y - PAD_Y) : (work_pos.y + PAD_Y);
			window_pos_pivot.x = (corner & 1) ? 1.0f : 0.0f;
			window_pos_pivot.y = (corner & 2) ? 1.0f : 0.0f;
			ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
			ImGui::SetNextWindowViewport(viewport->ID);
			window_flags |= ImGuiWindowFlags_NoMove;
		}
		ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
		if (ImGui::Begin("Performance Overlay", NULL, window_flags)) {
			ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		}
		if (ImGui::BeginPopupContextWindow()) {
			if (ImGui::MenuItem("Custom", NULL, corner == -1)) corner = -1;
			if (ImGui::MenuItem("Top-left", NULL, corner == 0)) corner = 0;
			if (ImGui::MenuItem("Top-right", NULL, corner == 1)) corner = 1;
			if (ImGui::MenuItem("Bottom-left", NULL, corner == 2)) corner = 2;
			if (ImGui::MenuItem("Bottom-right", NULL, corner == 3)) corner = 3;
			if (performance_overlay && ImGui::MenuItem("Close")) performance_overlay = false;
			ImGui::EndPopup();
		}
		ImGui::End();
	}

	void UI::ShowImguiDockSpace() {
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode |
			ImGuiDockNodeFlags_NoDockingInCentralNode;

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoBackground;

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpace Demo", NULL, window_flags);
		ImGui::PopStyleVar();

		ImGui::PopStyleVar(2);

		float height = ImGui::GetFrameHeight();

		// Submit the DockSpace
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
			ImGuiID dockspace_id = ImGui::GetID("Main dockspace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}
		if (ImGui::BeginViewportSideBar("##TopMenuBar", viewport, ImGuiDir_Up, height, window_flags)) {
			if (ImGui::BeginMenuBar()) {
				if (ImGui::BeginMenu("File")) {
					if (ImGui::MenuItem("Open Scene")) {
						OpenFileDialog();
					}
					if (ImGui::MenuItem("Save Scene")) {
						SaveFileDialog();
					}
					ImGui::MenuItem("Save Scene As...");
					if (ImGui::MenuItem("Exit")) {
						Window::ExitWindow(Window::GetOpenGLWindow());
					}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Edit")) {
					if (ImGui::MenuItem("Settings")) {
						//TODO: Add settings menu
					}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Window")) {
					ImGui::MenuItem("Inspector", 0, &inspector_window);
					ImGui::MenuItem("Scene hierarchy", 0, &SceneHPanel::scenehierarchy_window);
					ImGui::MenuItem("Console window", 0, &console_window);
					ImGui::MenuItem("Performance Overlay", 0, &performance_overlay);
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Help")) {
					ImGui::MenuItem("About");
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}
		}
		if (ImGui::BeginViewportSideBar("##SecondMenuBar", viewport, ImGuiDir_Up, height, window_flags)) {
			if (ImGui::BeginMenuBar()) {
				ImGui::SetCursorPos(ImVec2(Window::SCR_WIDTH / 2, 0));
				if (ImGui::MenuItem(">>")) {
					SetEngineState(PLAYING);
					Input::SetCursorState(Input::CURSOR_STATE_DISABLED, Window::GetOpenGLWindow());
					ImGui::EndMenu();
				}
				if (ImGui::MenuItem("||")) {
					SetEngineState(EDITING);
					Input::SetCursorState(Input::CURSOR_STATE_NORMAL, Window::GetOpenGLWindow());
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}
		}
		ImGui::End();
	}

	//-------------------------------
	//Windows functions
	const COMDLG_FILTERSPEC m_file_types[] =
	{
		{L"Scene file (*.scene)", L"*.scene"},
	};
	//TODO: IMPORTANT This functions leak memory
	static wchar_t* OpenFileDialog() {
		HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		PWSTR pszFilePath{};
		if (SUCCEEDED(hr)) {
			IFileOpenDialog* pFileOpen;

			// Create the FileOpenDialog object.
			hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog,
			                      reinterpret_cast<void**>(&pFileOpen));

			//Set the file extension
			hr = pFileOpen->SetFileTypes(ARRAYSIZE(m_file_types), m_file_types);
			hr = pFileOpen->SetFileTypeIndex(1);
			hr = pFileOpen->SetDefaultExtension(L"scene");

			if (SUCCEEDED(hr)) {
				// Show the Open dialog box.
				hr = pFileOpen->Show(NULL);
				// Get the file name from the dialog box.
				if (SUCCEEDED(hr)) {
					IShellItem* pItem;
					hr = pFileOpen->GetResult(&pItem);
					if (SUCCEEDED(hr)) {

						hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

						// Display the file name to the user.
						if (SUCCEEDED(hr)) {
							MessageBoxW(NULL, pszFilePath, L"File Path", MB_OK);
							CoTaskMemFree(pszFilePath);
						}
						pItem->Release();
					}
				}
				pFileOpen->Release();
			}
			CoUninitialize();
		}
		return pszFilePath;
	}

	static wchar_t* SaveFileDialog() {
		//TODO: Right now it has now use except opening a save window.
		IFileSaveDialog* pfsd = NULL;
		HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfsd));
		if (SUCCEEDED(hr)) {
			// Create an event handling object, and hook it up to the dialog.
			//IFileDialogEvents   *pfde       = NULL;
			DWORD dwCookie = 0;

			if (SUCCEEDED(hr)) {
				// Hook up the event handler.
				//  hr = pfsd->Advise(pfde, &dwCookie);
				if (SUCCEEDED(hr)) {
					// Set the file types to display.
					hr = pfsd->SetFileTypes(ARRAYSIZE(m_file_types), m_file_types);
					if (SUCCEEDED(hr)) {
						hr = pfsd->SetFileTypeIndex(1);
						if (SUCCEEDED(hr)) {
							hr = pfsd->SetDefaultExtension(L"doc");
							if (SUCCEEDED(hr)) {
								//IPropertyStore *pps = NULL;

								// The InMemory Property Store is a Property Store that is
								// kept in the memory instead of persisted in a file stream.
								//hr = PSCreateMemoryPropertyStore(IID_PPV_ARGS(&pps));
								if (SUCCEEDED(hr)) {
									PROPVARIANT propvarValue = {};
									//hr = InitPropVariantFromString(L"SampleKeywordsValue", &propvarValue);
									if (SUCCEEDED(hr)) {
										// Set the value to the property store of the item.
										//hr = pps->SetValue(PKEY_Keywords, propvarValue);
										if (SUCCEEDED(hr)) {
											// Commit does the actual writing back to the in memory store.
											//hr = pps->Commit();
											if (SUCCEEDED(hr)) {
												// Hand these properties to the File Dialog.
												//hr = pfsd->SetCollectedProperties(NULL, TRUE);
												if (SUCCEEDED(hr)) {
													//hr = pfsd->SetProperties(pps);
												}
											}
										}
										//PropVariantClear(&propvarValue);
									}
									//pps->Release();
								}
							}
						}
					}

					if (FAILED(hr)) {
						// Unadvise here in case we encounter failures before we get a chance to show the dialog.
						pfsd->Unadvise(dwCookie);
					}
				}
				//pfde->Release();
			}

			if (SUCCEEDED(hr)) {
				// Now show the dialog.
				hr = pfsd->Show(NULL);
				if (SUCCEEDED(hr)) {
					//
					// You can add your own code here to handle the results.
					//
				}
				// Unhook the event handler.
				pfsd->Unadvise(dwCookie);
			}
			pfsd->Release();
		}
		return nullptr;
	}
}
