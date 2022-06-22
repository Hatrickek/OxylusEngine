#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "core/filesystem.h"
#include "core/shader.h"
#include "core/camera.h"
#include "core/model.h"

#include <iostream>
#include <random>

#include "core/draw.h"
#include "core/ui.h"
#include "core/input.h"
#include "core/animation.h"
#include "core/animator.h"
#include "core/window.h"
#include "core/log.h"

#include "core/ssao.h"
#include "level.h"
#include "core/application.h"
#include "core/editor.h"
#include "core/gbuffer.h"
#include "core/scene.h"
using namespace FlatEngine;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void ProcessInput(GLFWwindow* window);

// camera
Camera camera;
float lastX = (float)FlatEngine::Window::SCR_WIDTH / 2.0;
float lastY = (float)FlatEngine::Window::SCR_HEIGHT / 2.0;
bool firstMouse = true;

std::shared_ptr<FlatEngine::SSAO> m_ssao;
std::shared_ptr<FlatEngine::GBuffer> m_gBuffer;
int main() {
	// glfw: initialize and configure
	FlatEngine::Log::Init();
	FlatEngine::Window::InitOpenGLWindow();
	glfwSetFramebufferSizeCallback(FlatEngine::Window::GetOpenGLWindow(), framebuffer_size_callback);
	glfwSetCursorPosCallback(FlatEngine::Window::GetOpenGLWindow(), mouse_callback);
	glfwSetScrollCallback(FlatEngine::Window::GetOpenGLWindow(), scroll_callback);
	
	// configure global opengl state
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_MULTISAMPLE);  
	Editor::OnInit();
	UI::InitUI(Window::GetOpenGLWindow());

	// build and compile shaders
	Shader shader_ssao_geometry_pass("resources/shaders/ssao_geometry.vs", "resources/shaders/ssao_geometry.fs");
	Shader shader_lighting_pass("resources/shaders/ssao.vs", "resources/shaders/ssao_lighting.fs");
	Shader shader_ssao("resources/shaders/ssao.vs", "resources/shaders/ssao.fs");
	Shader shader_ssao_blur("resources/shaders/ssao.vs", "resources/shaders/ssao_blur.fs");

	// load models
	//Model patrick_model(FileSystem::getPath("resources/objects/patrick_animated/patrick_animated.obj"));
	//Animation patrick_animation(FileSystem::getPath("resources/objects/patrick_animated/patrick_animated.obj"), &patrick_model);
	//Animator patrick_animator(&patrick_animation);

	//Model rsr_model(FileSystem::getPath("resources/objects/911rsr.obj"));

	m_gBuffer = std::make_shared<FlatEngine::GBuffer>(FlatEngine::Window::SCR_WIDTH, FlatEngine::Window::SCR_HEIGHT);

	m_ssao = std::make_shared<FlatEngine::SSAO>(FlatEngine::Window::SCR_WIDTH, FlatEngine::Window::SCR_HEIGHT);
	m_ssao->SetupSSAOShader(&shader_ssao, &shader_ssao_blur);
	//m_ssao = &ssao;
	// lighting info
	lightPositions.emplace_back(glm::vec3(2.0, 1.0, -2.0));
	lightColors.emplace_back(glm::vec3(0.2, 0.2, 0.7));
	// shader configuration
	shader_lighting_pass.use();
	shader_lighting_pass.setInt("gPosition", 0);
	shader_lighting_pass.setInt("gNormal", 1);
	shader_lighting_pass.setInt("gAlbedo", 2);
	shader_lighting_pass.setInt("ssao", 3);
	
	// render loop
	// -----------
	camera = *Editor::GetEditorCamera();

	camera.SetPosition(glm::vec3(1,1,3));

	

	while (!glfwWindowShouldClose(FlatEngine::Window::GetOpenGLWindow())) {
		glfwPollEvents();

		FlatEngine::App::Update();

		// input
		// -----
		ProcessInput(FlatEngine::Window::GetOpenGLWindow());
		//patrick_model.UpdateAnimation(FlatEngine::Timestep::GetDeltaTime());

		FlatEngine::UI::UpdateImgui();
		// render
		// ------
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// 1. geometry pass: render scene's geometry/color data into gbuffer
		// -----------------------------------------------------------------
		m_gBuffer->Begin();
		glm::mat4 projection = camera.GetProjectionMatrix();
		glm::mat4 view = camera.GetViewMatrix();
		glm::mat4 model = glm::mat4(1.0f);
		shader_ssao_geometry_pass.use();
		shader_ssao_geometry_pass.setMat4("projection", projection);
		shader_ssao_geometry_pass.setMat4("view", view);

		shader_ssao_geometry_pass.setInt("invertedNormals", 1);
		Draw::DrawCube(shader_ssao_geometry_pass, WHITE,glm::vec3(0.0, 7.0f, 0.0f), glm::vec3(7.5f, 7.5f, 7.5f));
		shader_ssao_geometry_pass.setInt("invertedNormals", 0);

		Editor::OnUpdate();
		
		//Draw the lights as cubes
		for (unsigned int i = 0; i < lightPositions.size(); i++) {
			model = glm::mat4(1.0f);
			model = glm::translate(model, glm::vec3(lightPositions[i]));
			model = glm::scale(model, glm::vec3(0.1f));
			shader_ssao_geometry_pass.setMat4("model", model);
			Draw::DrawCube(shader_ssao_geometry_pass, glm::vec4(lightColors[i], 1), lightPositions[i], glm::vec3(.1,.1,.1));
		}
		m_gBuffer->End();

		m_ssao->BeginSSAOTexture(projection, m_gBuffer->gPosition, m_gBuffer->gNormal);
		Draw::render_quad();
		m_ssao->EndSSAOTexture();

		// 3. blur SSAO texture to remove noise
		m_ssao->BeginSSAOBlurTexture();
		Draw::render_quad();
		m_ssao->EndSSAOBlurTexture();

		//TODO: Lighting class 
		// 4. lighting pass: traditional deferred Blinn-Phong lighting with added screen-space ambient occlusion
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		shader_lighting_pass.use();
		// send light relevant uniforms
		glm::vec3 lightPosView = glm::vec3(camera.GetViewMatrix() * glm::vec4(lightPositions[0], 1.0));
		shader_lighting_pass.setVec3("light.Position", lightPosView);
		shader_lighting_pass.setVec3("light.Color", lightColors[0]);
		// Update attenuation parameters
		const float linear = 0.09f;
		const float quadratic = 0.032f;
		shader_lighting_pass.setFloat("light.Linear", linear);
		shader_lighting_pass.setFloat("light.Quadratic", quadratic);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_gBuffer->gPosition);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, m_gBuffer->gNormal);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, m_gBuffer->gAlbedo);
		glActiveTexture(GL_TEXTURE3); // add extra SSAO texture to lighting pass
		glBindTexture(GL_TEXTURE_2D, m_ssao->ssaoColorBufferBlur);
		Draw::render_quad();

		FlatEngine::UI::DrawEditorUI();

        //finish imgui render.
        FlatEngine::UI::RenderImgui();

		glfwSwapBuffers(FlatEngine::Window::GetOpenGLWindow());
	}

	FlatEngine::UI::EndImgui();

	glfwTerminate();
	return 0;
}
void ProcessInput(GLFWwindow* window) {
	if (glfwGetKey(window, GLFW_KEY_F12) == GLFW_PRESS)
		FlatEngine::Window::ExitWindow(window);
	static bool cursorLockKeyDown = false;
	if (FlatEngine::Input::GetKeyDown(FlatEngine::Key::Escape) && !cursorLockKeyDown) {
		cursorLockKeyDown = true;
		if(FlatEngine::Input::GetCursorState() == FlatEngine::Input::CURSOR_STATE_NORMAL)
			FlatEngine::Input::SetCursorState(FlatEngine::Input::CURSOR_STATE_DISABLED, window);
		else
			FlatEngine::Input::SetCursorState(FlatEngine::Input::CURSOR_STATE_NORMAL, window);
	}		
	if(FlatEngine::Input::GetKeyUp(FlatEngine::Key::Escape)){
		cursorLockKeyDown = false;	
	}

	if (FlatEngine::Input::GetKey(FlatEngine::Key::W))
		camera.ProcessKeyboard(FORWARD, FlatEngine::Timestep::GetDeltaTime());
	if (FlatEngine::Input::GetKey(FlatEngine::Key::S))
		camera.ProcessKeyboard(BACKWARD, FlatEngine::Timestep::GetDeltaTime());
	if (FlatEngine::Input::GetKey(FlatEngine::Key::A))
		camera.ProcessKeyboard(LEFT, FlatEngine::Timestep::GetDeltaTime());
	if (FlatEngine::Input::GetKey(FlatEngine::Key::D))
		camera.ProcessKeyboard(RIGHT, FlatEngine::Timestep::GetDeltaTime());
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	m_gBuffer->RegenerateBuffers(width, height);
	m_ssao->RegenerateBuffers(width, height);
	FlatEngine::Window::SCR_HEIGHT = height;
	FlatEngine::Window::SCR_WIDTH = width;

	glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
	float xpos = static_cast<float>(xposIn);
	float ypos = static_cast<float>(yposIn);
	if (firstMouse) {
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

	lastX = xpos;
	lastY = ypos;

	camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
	camera.ProcessMouseScroll(static_cast<float>(yoffset));
}
