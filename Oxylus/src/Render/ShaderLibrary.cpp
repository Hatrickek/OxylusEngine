#include <oxpch.h>
#include "ShaderLibrary.h"

namespace Oxylus {
  std::unordered_map<std::string, Ref<VulkanShader>> ShaderLibrary::s_Shaders = {};

  Ref<VulkanShader> ShaderLibrary::CreateShader(const ShaderCI& shaderCreateInfo) {
    return s_Shaders.emplace(shaderCreateInfo.Name, CreateRef<VulkanShader>(shaderCreateInfo)).first->second;
  }

  std::future<Ref<VulkanShader>> ShaderLibrary::CreateShaderAsync(ShaderCI shaderCreateInfo) {
    return std::async([shaderCreateInfo] {
      return s_Shaders.emplace(shaderCreateInfo.Name, CreateRef<VulkanShader>(shaderCreateInfo)).first->second;
    });
  }

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
