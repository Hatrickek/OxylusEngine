#include "src/oxpch.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "VulkanBuffer.h"

#include "VulkanContext.h"

namespace Oxylus {
  VulkanBuffer::~VulkanBuffer() { }

  VulkanBuffer& VulkanBuffer::CreateBuffer(const vk::BufferUsageFlags usageFlags,
                                           const vk::MemoryPropertyFlags memoryPropertyFlags,
                                           const vk::DeviceSize size,
                                           const void* data,
                                           VmaMemoryUsage memoryUsageFlag) {
    Size = size;
    UsageFlags = usageFlags;

    vk::BufferCreateInfo bufferCI;
    bufferCI.usage = usageFlags;
    bufferCI.size = size;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = memoryUsageFlag;
    allocInfo.preferredFlags = (VkMemoryPropertyFlags)memoryPropertyFlags;
    allocInfo.flags = VMA_ALLOCATION_CREATE_DONT_BIND_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    const VkBufferCreateInfo createinfo = bufferCI;
    VmaAllocationInfo allocationInfo{};
    vmaCreateBuffer(VulkanContext::GetAllocator(), &createinfo, &allocInfo, &m_Buffer, &m_Allocation, &allocationInfo);
    GPUMemory::TotalAllocated += allocationInfo.size;

    if (data) {
      Map();
      Copy(data, size);
      if (!(memoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent))
        Flush();
      Unmap();
    }

    m_Descriptor.buffer = m_Buffer;
    m_Descriptor.offset = 0;
    m_Descriptor.range = VK_WHOLE_SIZE;

    Bind();

    m_Freed = false;

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
    vmaBindBufferMemory(VulkanContext::GetAllocator(), m_Allocation, m_Buffer);
    return *this;
  }

  const VulkanBuffer& VulkanBuffer::CopyTo(const vk::Buffer& dstBuffer,
                                           const vk::CommandBuffer& copyCmd,
                                           const vk::BufferCopy& bufferCopy) const {
    copyCmd.copyBuffer(m_Buffer, dstBuffer, 1, &bufferCopy);
    return *this;
  }

  void VulkanBuffer::Map() {
    vmaMapMemory(VulkanContext::GetAllocator(), m_Allocation, &m_Mapped);
  }

  void VulkanBuffer::Unmap() {
    vmaUnmapMemory(VulkanContext::GetAllocator(), m_Allocation);
    m_Mapped = nullptr;
  }

  void VulkanBuffer::Flush() const {
    vmaFlushAllocation(VulkanContext::GetAllocator(), m_Allocation, 0, VK_WHOLE_SIZE);
  }

  void VulkanBuffer::Destroy() {
    if (m_Freed)
      return;
    if (m_Mapped)
      Unmap();
    vmaDestroyBuffer(VulkanContext::GetAllocator(), m_Buffer, m_Allocation);
    GPUMemory::TotalFreed += Size;
    m_Freed = true;
  }
}
