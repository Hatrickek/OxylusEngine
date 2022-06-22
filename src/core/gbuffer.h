#pragma once
#include <ostream>
#include "glad/glad.h"

#include "log.h"

namespace FlatEngine {
	class GBuffer {
	public:
		unsigned int gbufferID;
		unsigned int gPosition, gNormal, gAlbedo;

		GBuffer(unsigned int width, unsigned int height) {
			m_width  = width;
			m_height = height;

			//position buffer
			glGenFramebuffers(1, &gbufferID);
			glBindFramebuffer(GL_FRAMEBUFFER, gbufferID);
			glGenTextures(1, &gPosition);
			glBindTexture(GL_TEXTURE_2D, gPosition);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

			// normal color buffer
			glGenTextures(1, &gNormal);
			glBindTexture(GL_TEXTURE_2D, gNormal);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

			// color + specular color buffer
			glGenTextures(1, &gAlbedo);
			glBindTexture(GL_TEXTURE_2D, gAlbedo);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedo, 0);

			//// tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
			unsigned int attachments[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
			glDrawBuffers(3, attachments);
			// create and attach depth buffer (renderbuffer)
			unsigned int rboDepth;
			glGenRenderbuffers(1, &rboDepth);
			glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
			// finally check if framebuffer is complete
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				FE_LOG_ERROR("Framebuffer not complete!" );
			else
				FE_LOG_INFO("Framebuffer created! ID: {0}", gbufferID);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
		void RegenerateBuffers(unsigned int width, unsigned int height) {
			Unload();
			GBuffer(width, height);
		}
		void Begin(){
			glBindFramebuffer(GL_FRAMEBUFFER, gbufferID);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}
		void End() {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
		void Unload() {
			glDeleteFramebuffers(1, &gbufferID);
		    glDeleteTextures(1, &gAlbedo);
		    glDeleteTextures(1, &gNormal);
		    glDeleteTextures(1, &gPosition);
		}
	private:
		unsigned int m_width;
		unsigned int m_height;
	};

}
