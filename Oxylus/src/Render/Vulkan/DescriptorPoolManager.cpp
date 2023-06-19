#include "DescriptorPoolManager.h"

#include "Utils/VulkanUtils.h"

namespace Oxylus {
  DescriptorPoolManager* DescriptorPoolManager::s_Instance = nullptr;

  void DescriptorPoolManager::Init() {
    if (s_Instance)
      return;

    s_Instance = new DescriptorPoolManager();

    constexpr vk::DescriptorPoolSize poolSizes[] = {
      {vk::DescriptorType::eSampler, 50}, {vk::DescriptorType::eCombinedImageSampler, 50},
      {vk::DescriptorType::eSampledImage, 50}, {vk::DescriptorType::eStorageImage, 10},
      {vk::DescriptorType::eUniformTexelBuffer, 50}, {vk::DescriptorType::eStorageTexelBuffer, 10},
      {vk::DescriptorType::eUniformBuffer, 50}, {vk::DescriptorType::eStorageBuffer, 50},
      {vk::DescriptorType::eUniformBufferDynamic, 50}, {vk::DescriptorType::eStorageBufferDynamic, 10},
      {vk::DescriptorType::eInputAttachment, 50}
    };
    vk::DescriptorPoolCreateInfo poolInfo = {};
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = 1000 * OX_ARRAYSIZE(poolSizes);
    poolInfo.poolSizeCount = (uint32_t)OX_ARRAYSIZE(poolSizes);
    poolInfo.pPoolSizes = poolSizes;
    VulkanUtils::CheckResult(VulkanContext::GetDevice().createDescriptorPool(&poolInfo, nullptr, &s_Instance->m_DefaultPool));

    constexpr auto typeSize = 20;
    constexpr vk::DescriptorPoolSize smallPoolSizes[] = {
      {vk::DescriptorType::eSampler, typeSize}, {vk::DescriptorType::eCombinedImageSampler, typeSize},
      {vk::DescriptorType::eSampledImage, typeSize}, {vk::DescriptorType::eStorageImage, typeSize},
      {vk::DescriptorType::eUniformTexelBuffer, 5}, {vk::DescriptorType::eStorageTexelBuffer, typeSize},
      {vk::DescriptorType::eUniformBuffer, typeSize}, {vk::DescriptorType::eStorageBuffer, typeSize},
      {vk::DescriptorType::eUniformBufferDynamic, typeSize}, {vk::DescriptorType::eStorageBufferDynamic, typeSize},
      {vk::DescriptorType::eInputAttachment, typeSize}
    };
    poolInfo.maxSets = 2 * OX_ARRAYSIZE(smallPoolSizes);
    poolInfo.poolSizeCount = (uint32_t)OX_ARRAYSIZE(smallPoolSizes);
    poolInfo.pPoolSizes = smallPoolSizes;
    s_Instance->m_SmallPoolCreateInfo = poolInfo;
  }

  void DescriptorPoolManager::Release() {
    for (const auto& pool : s_Instance->m_InUsePools) {
      VulkanContext::GetDevice().destroyDescriptorPool(pool);
    }

    delete s_Instance;
  }

  vk::DescriptorPool DescriptorPoolManager::GetFreePool() {
    return s_Instance->m_DefaultPool;
  }

  void DescriptorPoolManager::FreePool(vk::DescriptorPool pool) {
    for (uint32_t i = 0; i < (uint32_t)m_InUsePools.size(); ++i) {
      if (m_InUsePools[i] == pool) {
        VulkanContext::GetDevice().resetDescriptorPool(m_InUsePools[i]);
        m_InUsePools.erase(m_InUsePools.begin() + i);
        m_FreePools.push_back(pool);
        break;
      }
    }
  }

  vk::DescriptorPool DescriptorPoolManager::CreatePool() const {
    const auto res = VulkanContext::GetDevice().createDescriptorPool(m_SmallPoolCreateInfo);
    VulkanUtils::CheckResult(res.result);
    return res.value;
  }
}
