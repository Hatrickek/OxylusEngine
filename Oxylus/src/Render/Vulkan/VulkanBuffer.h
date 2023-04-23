#pragma once

#include <vulkan/vulkan.hpp>

#include "vk_mem_alloc.h"
#include "Event/Event.h"

namespace Oxylus {
  class VulkanBuffer {
  public:
    vk::DeviceSize Size = 0;
    vk::BufferUsageFlags UsageFlags = {};

    VulkanBuffer() = default;
    ~VulkanBuffer();

    VulkanBuffer& CreateBuffer(vk::BufferUsageFlags usageFlags,
                               vk::MemoryPropertyFlags memoryPropertyFlags,
                               vk::DeviceSize size,
                               const void* data = nullptr,
                               VmaMemoryUsage memoryUsageFlag = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    vk::BufferMemoryBarrier CreateMemoryBarrier(vk::AccessFlagBits src, vk::AccessFlagBits dst) const;
    const VulkanBuffer& Bind() const;
    const VulkanBuffer& CopyTo(const vk::Buffer& dstBuffer,
                               const vk::CommandBuffer& copyCmd,
                               const vk::BufferCopy& bufferCopy) const;
    VulkanBuffer& Map();
    VulkanBuffer& Unmap();
    VulkanBuffer& Flush();
    VulkanBuffer& Destroy();
    VulkanBuffer& Update();

    template <typename T = uint8_t> void Copy(const void* data, size_t size, VkDeviceSize offset = 0) const {
      memcpy((T*)m_Mapped + offset, data, size);
    }

    template <typename T> void Copy(const std::vector<T>& data, VkDeviceSize offset = 0) {
      Copy(data.data(), sizeof(T) * data.size(), offset);
    }

    VulkanBuffer& SetOnUpdate(std::function<void()> func);

    template <typename Type>
    VulkanBuffer& Sink(EventDispatcher& dispatcher) {
      dispatcher.sink<Type>().connect < &VulkanBuffer::Update > (*this);
      return *this;
    }

    vk::Buffer Get() const { return m_Buffer; }
    const vk::DescriptorBufferInfo& GetDescriptor() const { return m_Descriptor; }
    vk::DescriptorBufferInfo& GetDescriptor() { return m_Descriptor; }

  private:
    VkBuffer m_Buffer;
    VmaAllocation m_Allocation;
    vk::DescriptorBufferInfo m_Descriptor;
    void* m_Mapped = nullptr;
    bool m_Freed = false;
    std::function<void()> m_OnUpdate = nullptr;
  };
}
