#pragma once

#include <vulkan/vulkan.hpp>

#include "VulkanContext.h"

namespace Oxylus {
  class DescriptorPoolManager {
  public:
    DescriptorPoolManager() = default;
    ~DescriptorPoolManager() = default;

    static void Init();
    static void Release();

    static DescriptorPoolManager* Get() { return s_Instance; }

    vk::DescriptorPool& GetDefaultPool() { return m_DefaultPool; }
    vk::DescriptorPool GetFreePool();
    void FreePool(vk::DescriptorPool pool);

  private:
    static DescriptorPoolManager* s_Instance;

    vk::DescriptorPool m_DefaultPool = {};

    vk::DescriptorPoolCreateInfo m_SmallPoolCreateInfo = {};
    std::vector<vk::DescriptorPool> m_InUsePools = {};
    std::vector<vk::DescriptorPool> m_FreePools = {};

    vk::DescriptorPool CreatePool() const;
  };
}
