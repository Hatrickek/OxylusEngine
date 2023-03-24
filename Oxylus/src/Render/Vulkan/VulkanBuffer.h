#pragma once

#include "Utils/VulkanAllocation.h"
#include <vulkan/vulkan.hpp>

namespace Oxylus {
  class VulkanBuffer : public VulkanAllocation {
  public:
    vk::DeviceSize Alignment;
    vk::DeviceSize Size;
    vk::BufferUsageFlags UsageFlags;

    VulkanBuffer() = default;

    VulkanBuffer& CreateBuffer(vk::BufferUsageFlags usageFlags,
                               vk::MemoryPropertyFlags memoryPropertyFlags,
                               vk::DeviceSize size,
                               const void* data = nullptr);

    vk::BufferMemoryBarrier CreateMemoryBarrier(vk::AccessFlagBits src, vk::AccessFlagBits dst) const;

    const VulkanBuffer& Bind() const;

    const VulkanBuffer& CopyTo(const vk::Buffer& dstBuffer,
                               const vk::CommandBuffer& copyCmd,
                               const vk::BufferCopy& bufferCopy) const;

    const vk::Buffer& Get() const { return m_Buffer; }

    const vk::DescriptorBufferInfo& GetDescriptor() const { return m_Descriptor; }

    vk::DescriptorBufferInfo& GetDescriptor() { return m_Descriptor; }

    void Destroy();
    bool operator==(std::nullptr_t null) const;

  private:
    vk::Buffer m_Buffer;
    vk::DescriptorBufferInfo m_Descriptor;
  };
}
