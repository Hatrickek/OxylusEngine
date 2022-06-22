#include "resources.h"

namespace FlatEngine {
	static Ref<Model>	defaultCube;
	static Ref<Shader>	defaultShader; 
	void Resources::InitResources() {
		defaultCube		= CreateRef<Model>("resources\\objects\\cube.obj");
		defaultShader	= CreateRef<Shader>("resources\\shaders\\ssao_geometry.vs", "resources\\shaders\\ssao_geometry.fs");
	}
	Ref<Model> Resources::GetDefaultCube() {
		return defaultCube;
	}
	Ref<Shader> Resources::GetDefaultShader() {

		return defaultShader;
	}

}
