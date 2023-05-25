#pragma once
#include <vulkan/vulkan.hpp>

#include "VulkanContext.h"

namespace Oxylus {
  class CommandPoolManager {
  public:
    CommandPoolManager() = default;
    ~CommandPoolManager() = default;

    static void Init();
    static void Release();

    static CommandPoolManager* Get() { return s_Instance; }

    vk::CommandPool GetFreePool();
    void FreePool(vk::CommandPool pool);

  private:
    static CommandPoolManager* s_Instance;

    std::vector<vk::CommandPool> m_InUsePools = {};
    std::vector<vk::CommandPool> m_FreePools = {};

    vk::CommandPool CreatePool() const;
  };
}
