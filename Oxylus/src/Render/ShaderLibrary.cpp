#include <oxpch.h>
#include "ShaderLibrary.h"

namespace Oxylus {
  std::unordered_map<std::string, Ref<VulkanShader>> ShaderLibrary::s_Shaders;

  void ShaderLibrary::AddShader(const Ref<VulkanShader>& shader) {
    s_Shaders.emplace(shader->GetName(), shader);
  }

  void ShaderLibrary::RemoveShader(const std::string& name) {
    s_Shaders.erase(name);
  }
  
  Ref<VulkanShader> ShaderLibrary::GetShader(const std::string& name) {
    return s_Shaders[name];
  }

  void ShaderLibrary::UnloadShaders() {
    for (auto& [name, shader] : s_Shaders) {
      shader->Unload();
    }
  }
}