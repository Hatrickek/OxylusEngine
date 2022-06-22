#pragma once
#include "shader.h"
#include "GLFW/glfw3.h"
#include <random>
#include "log.h"
namespace FlatEngine {
	class SSAO {
	public:
		unsigned int ssaoFBO, ssaoBlurFBO;
		unsigned int ssaoColorBuffer, ssaoColorBufferBlur;
		unsigned int noiseTexture;

		inline static int ssao_kernel_size = 64;
		inline static float ssao_radius = 0.5f;
		inline static float ssao_sample = 64;

		SSAO(unsigned int m_width, unsigned int m_height);
		~SSAO();
		void RegenerateBuffers(unsigned int width, unsigned int height);
		void BeginSSAOTexture(glm::mat4 projection, unsigned int gPosition, unsigned int gNormal);
		void Unload();
		void EndSSAOTexture();
		void BeginSSAOBlurTexture();
		void EndSSAOBlurTexture();
		static int SetKernelSize(static int value);
		static float SetRadius(float value);
		static float SetSample(float value);
		void SetupSSAOShader(Shader* m_shader_ssao, Shader* m_shader_ssao_blur);
	private:
		static unsigned int width, height;
		std::vector<glm::vec3> ssaoKernel;

		static Shader* shader_ssao;
		static Shader* shader_ssao_blur;

		static float m_lerp(float a, float b, float f);
	};
}
