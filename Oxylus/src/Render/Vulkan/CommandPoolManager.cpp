#include "oxpch.h"
#include "CommandPoolManager.h"

#include "Utils/VulkanUtils.h"

namespace Oxylus {
  CommandPoolManager* CommandPoolManager::s_Instance = nullptr;

  void CommandPoolManager::Init() {
    if (s_Instance)
      return;
    
    s_Instance = new CommandPoolManager();
  }

  void CommandPoolManager::Release() {
    for (const auto& pool : s_Instance->m_FreePools)
      VulkanContext::GetDevice().destroyCommandPool(pool);

    delete s_Instance;
  }

  vk::CommandPool CommandPoolManager::GetFreePool() {
    if (!m_FreePools.empty()) {
      const auto pool = m_FreePools.back();
      m_FreePools.pop_back();
      return pool;
    }
    auto pool = CreatePool();
    m_InUsePools.emplace_back(pool);
    return pool;
  }

  void CommandPoolManager::FreePool(vk::CommandPool pool) {
    for (uint32_t i = 0; i < (uint32_t)m_InUsePools.size(); ++i) {
      if (m_InUsePools[i] == pool) {
        VulkanContext::GetDevice().resetCommandPool(m_InUsePools[i]);
        m_InUsePools.erase(m_InUsePools.begin() + i);
        m_FreePools.push_back(pool);
        break;
      }
    }
  }

  vk::CommandPool CommandPoolManager::CreatePool() const {
    vk::CommandPoolCreateInfo cmdPoolInfo;
    cmdPoolInfo.queueFamilyIndex = VulkanContext::VulkanQueue.graphicsQueueFamilyIndex;
    cmdPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    const auto res = VulkanContext::GetDevice().createCommandPool(cmdPoolInfo);
    VulkanUtils::CheckResult(res.result);
    return res.value;
  }
}
