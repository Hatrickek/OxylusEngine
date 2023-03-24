#include "oxpch.h"
#include "VulkanAllocation.h"
#include "VulkanUtils.h"
#include "Utils/Log.h"

namespace Oxylus {
  void VulkanAllocation::Allocate(int memoryIndex, void* pNext) {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    vk::MemoryAllocateInfo memoryAllocateInfo;
    memoryAllocateInfo.allocationSize = Memories[memoryIndex].MemoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = Memories[memoryIndex].MemoryTypeIndex;
    memoryAllocateInfo.pNext = pNext;
    VulkanUtils::CheckResult(LogicalDevice.allocateMemory(&memoryAllocateInfo,
      nullptr,
      &Memories[memoryIndex].DeviceMemory));
  }

  void VulkanAllocation::BindBufferMemory(int memoryIndex, const vk::Buffer& buffer, int offset) const {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    LogicalDevice.bindBufferMemory(buffer, Memories[memoryIndex].DeviceMemory, offset);
  }

  void VulkanAllocation::BindImageMemory(int memoryIndex, const vk::Image& image, vk::DeviceSize offset) const {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    LogicalDevice.bindImageMemory(image, Memories[memoryIndex].DeviceMemory, offset);
  }

  void VulkanAllocation::Unmap(int memoryIndex) {
    if (Memories.empty() || !Memories[memoryIndex].Mapped)
      return;
    VulkanContext::Context.Device.unmapMemory(Memories[memoryIndex].DeviceMemory);
    Memories[memoryIndex].Mapped = nullptr;
  }

  bool VulkanAllocation::FindMemoryTypeIndex(int memoryIndex) {
    vk::PhysicalDeviceMemoryProperties memoryProperties;
    const auto& PhysicalDevice = VulkanContext::Context.PhysicalDevice;
    PhysicalDevice.getMemoryProperties(&memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
      // Check each memory type to see if its bit is set to 1.
      if (Memories[memoryIndex].MemoryRequirements.memoryTypeBits & (1 << i) && (
            memoryProperties.memoryTypes[i].propertyFlags & Memories[memoryIndex].memoryPropertyFlags) == Memories[
            memoryIndex].memoryPropertyFlags) {
        Memories[memoryIndex].MemoryTypeIndex = i;
        return true;
      }
    }

    OX_CORE_FATAL("Unable to find suitable memory type!");
    return false;
  }

  void VulkanAllocation::CopyToMemory(const vk::DeviceMemory& memory,
                                      const void* data,
                                      vk::DeviceSize size,
                                      vk::DeviceSize offset) {
    void* mapped = VulkanContext::Context.Device.mapMemory(memory, offset, size, vk::MemoryMapFlags());
    memcpy(mapped, data, size);
    VulkanContext::Context.Device.unmapMemory(memory);
  }

  void VulkanAllocation::Flush(int memoryIndex, vk::DeviceSize size, vk::DeviceSize offset) const {
    return VulkanContext::Context.Device.flushMappedMemoryRanges(vk::MappedMemoryRange{
      Memories[memoryIndex].DeviceMemory, offset, size
    });
  }

  void VulkanAllocation::Invalidate(int memoryIndex, vk::DeviceSize size, vk::DeviceSize offset) const {
    return VulkanContext::Context.Device.invalidateMappedMemoryRanges(vk::MappedMemoryRange{
      Memories[memoryIndex].DeviceMemory, offset, size
    });
  }

  void VulkanAllocation::FreeMemory(int memoryIndex) {
    if (Memories.empty())
      return;
    if (nullptr != Memories[memoryIndex].Mapped) {
      Unmap(memoryIndex);
    }
    if (Memories[memoryIndex].DeviceMemory) {
      VulkanContext::Context.Device.freeMemory(Memories[memoryIndex].DeviceMemory);
      Memories[memoryIndex].DeviceMemory = vk::DeviceMemory();
    }
  }

  void VulkanAllocation::Map(int memoryIndex, size_t offset, VkDeviceSize size) {
    VulkanUtils::CheckResult(VulkanContext::Context.Device.mapMemory(Memories[memoryIndex].DeviceMemory,
      offset,
      size,
      {},
      &Memories[memoryIndex].Mapped));
  }

  void VulkanAllocation::Map(int memoryIndex, size_t offset, VkDeviceSize size, void* data) const {
    VulkanUtils::CheckResult(
      VulkanContext::Context.Device.mapMemory(Memories[memoryIndex].DeviceMemory, offset, size, {}, &data));
  }

  template <typename T> void VulkanAllocation::CopyToMemory(const vk::DeviceMemory& memory,
                                                            const T& data,
                                                            size_t offset) const {
    CopyToMemory(memory, &data, sizeof(T), offset);
  }

  template <typename T> void VulkanAllocation::CopyToMemory(const vk::DeviceMemory& memory,
                                                            const std::vector<T>& data,
                                                            size_t offset) const {
    CopyToMemory(memory, data.data(), data.size() * sizeof(T), offset);
  }
}
