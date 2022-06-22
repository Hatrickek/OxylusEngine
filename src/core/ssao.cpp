#pragma once
#include "ssao.h"
namespace FlatEngine {
		unsigned int SSAO::width, SSAO::height;
		Shader* SSAO::shader_ssao;
		Shader* SSAO::shader_ssao_blur;

		SSAO::SSAO(unsigned int m_width, unsigned int m_height) {
			width  = m_width;
			height = m_height;

			//// also create framebuffer to hold SSAO processing stage 
			glGenFramebuffers(1, &ssaoFBO);
			glGenFramebuffers(1, &ssaoBlurFBO);
			glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
			// SSAO color buffer
			glGenTextures(1, &ssaoColorBuffer);
			glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBuffer, 0);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				FE_LOG_ERROR("SSAO Framebuffer not complete!");
			// and blur stage
			glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
			glGenTextures(1, &ssaoColorBufferBlur);
			glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBufferBlur, 0);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				FE_LOG_ERROR("SSAO Blur Framebuffer not complete!");
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			// generate sample kernel
			std::uniform_real_distribution<GLfloat> randomFloats(0.0, 1.0); // generates random floats between 0.0 and 1.0
			std::default_random_engine generator;
			for (unsigned int i = 0; i < ssao_sample; ++i) {
				glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0,
				                 randomFloats(generator));
				sample = glm::normalize(sample);
				sample *= randomFloats(generator);
				float scale = float(i) / ssao_sample;

				// scale samples s.t. they're more aligned to center of kernel
				scale = m_lerp(0.1f, 1.0f, scale * scale);
				sample *= scale;
				ssaoKernel.push_back(sample);
			}

			// generate noise texture
			std::vector<glm::vec3> ssaoNoise;
			for (unsigned int i = 0; i < 16; i++) {
				glm::vec3 noise(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, 0.0f);
				// rotate around z-axis (in tangent space)
				ssaoNoise.push_back(noise);
			}
			glGenTextures(1, &noiseTexture);
			glBindTexture(GL_TEXTURE_2D, noiseTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}
		SSAO::~SSAO() = default;
		void SSAO::SetupSSAOShader(Shader* m_shader_ssao, Shader* m_shader_ssao_blur) {
			shader_ssao = m_shader_ssao;
			shader_ssao_blur = m_shader_ssao_blur;
			shader_ssao->use();
			shader_ssao->setInt("gPosition", 0);
			shader_ssao->setInt("gNormal", 1);
			shader_ssao->setInt("texNoise", 2);
			shader_ssao->setFloat("screen_width", width);
			shader_ssao->setFloat("screen_height", height);
			shader_ssao_blur->use();
			shader_ssao_blur->setInt("ssaoInput", 0);
		}
		void SSAO::RegenerateBuffers(unsigned int width, unsigned int height) {
			Unload();
			SSAO(width, height);
			SetupSSAOShader(shader_ssao, shader_ssao_blur);
		}
		void SSAO::Unload() {
			glDeleteFramebuffers(1, &ssaoFBO);
			glDeleteFramebuffers(1, &ssaoBlurFBO);
		    glDeleteTextures(1, &ssaoColorBuffer);
		    glDeleteTextures(1, &ssaoColorBufferBlur);
		    glDeleteTextures(1, &noiseTexture);
		}
		void SSAO::BeginSSAOTexture(glm::mat4 projection, unsigned int gPosition, unsigned int gNormal) {
			// 2. generate SSAO texture
			glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
			glClear(GL_COLOR_BUFFER_BIT);
			shader_ssao->use();
			// Send kernel + rotation 
			for (unsigned int i = 0; i < ssao_sample; ++i)
				shader_ssao->setVec3("samples[" + std::to_string(i) + "]", ssaoKernel[i]);
			shader_ssao->setMat4("projection", projection);
			shader_ssao->setInt("kernelSize", ssao_kernel_size);
			shader_ssao->setFloat("radius", ssao_radius);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, gPosition);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, gNormal);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, noiseTexture);
		}
		void SSAO::EndSSAOTexture() {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
		void SSAO::BeginSSAOBlurTexture() {
			glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
			glClear(GL_COLOR_BUFFER_BIT);
			shader_ssao_blur->use();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
		}
		void SSAO::EndSSAOBlurTexture() {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
		int SSAO::SetKernelSize(static int value) {
			ssao_kernel_size = value;
			return ssao_kernel_size;
		}
		float SSAO::SetRadius(float value) {
			ssao_radius = value;
			return ssao_radius;
		}
		float SSAO::SetSample(float value) {
			ssao_sample = value;
			return ssao_sample;
		}
		float SSAO::m_lerp(float a, float b, float f) {
			return a + f * (b - a);
		}
}
