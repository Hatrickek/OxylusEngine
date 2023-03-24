#include "src/oxpch.h"
#include "VulkanBuffer.h"

#include "VulkanContext.h"

namespace Oxylus {
  VulkanBuffer& VulkanBuffer::CreateBuffer(const vk::BufferUsageFlags usageFlags,
                                           const vk::MemoryPropertyFlags memoryPropertyFlags,
                                           const vk::DeviceSize size,
                                           const void* data) {
    const auto& LogicalDevice = VulkanContext::Context.Device;

    const auto alignment = VulkanContext::Context.DeviceProperties.limits.minUniformBufferOffsetAlignment;
    const auto extra = size % alignment;
    constexpr auto count = 1;
    const auto alignedSize = size + (alignment - extra);
    const auto allocatedSize = count * alignedSize;

    Memories.emplace_back(Memory{});

    vk::BufferCreateInfo bufferCI;
    bufferCI.usage = usageFlags;
    bufferCI.size = allocatedSize;
    //bufferCI.size = size;
    m_Buffer = LogicalDevice.createBuffer(bufferCI).value;

    LogicalDevice.getBufferMemoryRequirements(m_Buffer, &Memories[0].MemoryRequirements);

    Memories[0].memoryPropertyFlags = memoryPropertyFlags;
    FindMemoryTypeIndex(0);
    vk::MemoryAllocateFlagsInfoKHR allocFlagsInfo{};
    if (usageFlags & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
      allocFlagsInfo.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;
    }
    Allocate(0, &allocFlagsInfo);

    Alignment = alignedSize;
    Size = allocatedSize;
    //Size = size;
    UsageFlags = usageFlags;

    if (data) {
      Map(0, 0, Size);
      Copy(0, Size, data);
      if (!(memoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent))
        Flush(0);
      Unmap(0);
    }

    m_Descriptor.buffer = m_Buffer;
    m_Descriptor.offset = 0;
    m_Descriptor.range = VK_WHOLE_SIZE;

    Bind();

    return *this;
  }

  vk::BufferMemoryBarrier VulkanBuffer::CreateMemoryBarrier(const vk::AccessFlagBits src,
                                                            const vk::AccessFlagBits dst) const {
    vk::BufferMemoryBarrier memoryBarrier;
    memoryBarrier.size = Size;
    memoryBarrier.buffer = m_Buffer;
    memoryBarrier.offset = m_Descriptor.offset;
    memoryBarrier.srcAccessMask = src;
    memoryBarrier.dstAccessMask = dst;
    return memoryBarrier;
  }

  const VulkanBuffer& VulkanBuffer::Bind() const {
    BindBufferMemory(0, m_Buffer);
    return *this;
  }

  const VulkanBuffer& VulkanBuffer::CopyTo(const vk::Buffer& dstBuffer,
                                           const vk::CommandBuffer& copyCmd,
                                           const vk::BufferCopy& bufferCopy) const {
    copyCmd.copyBuffer(m_Buffer, dstBuffer, 1, &bufferCopy);
    return *this;
  }

  void VulkanBuffer::Destroy() {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    LogicalDevice.destroyBuffer(m_Buffer);
    FreeMemory(0);
    Memories.clear();
  }

  bool VulkanBuffer::operator==(std::nullptr_t null) const { return m_Buffer.operator==(null); }
}
