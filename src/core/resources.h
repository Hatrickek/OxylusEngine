#pragma once
#include "model.h"
#include "utility.h"

namespace FlatEngine {
	class Resources {
	public:
		static void InitResources();
		static Ref<Model>  GetDefaultCube();
		static Ref<Shader> GetDefaultShader();
	};
}
