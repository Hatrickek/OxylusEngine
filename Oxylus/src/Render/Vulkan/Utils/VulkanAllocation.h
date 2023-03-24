#pragma once

#include <vulkan/vulkan.hpp>

namespace Oxylus {
  class VulkanAllocation {
  public:
    virtual ~VulkanAllocation() = default;

    struct Memory {
      vk::DeviceMemory DeviceMemory{};
      vk::MemoryRequirements MemoryRequirements{};
      vk::DeviceSize size{0};
      vk::DeviceSize Alignment{0};
      vk::DeviceSize AllocSize{0};
      int MemoryTypeIndex;
      void* Mapped = nullptr;
      vk::MemoryPropertyFlags memoryPropertyFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;
    };

    std::vector<Memory> Memories;

    void Allocate(int memoryIndex, void* pNext = nullptr);

    void BindBufferMemory(int memoryIndex, const vk::Buffer& buffer, int offset = 0) const;

    void BindImageMemory(int memoryIndex, const vk::Image& image, vk::DeviceSize offset = 0) const;

    void Map(int memoryIndex = 0, size_t offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

    void Map(int memoryIndex, size_t offset, VkDeviceSize size, void* data) const;

    void Unmap(int memoryIndex = 0);

    bool FindMemoryTypeIndex(int memoryIndex);

    template <typename T = uint8_t> void Copy(int memoryIndex, size_t size, const void* data, VkDeviceSize offset = 0) {
      memcpy((T*)Memories[memoryIndex].Mapped + offset, data, size);
    }

    template <typename T = uint8_t> void Copy(const void* data, size_t size, VkDeviceSize offset = 0) {
      memcpy((T*)Memories[0].Mapped + offset, data, size);
    }

    template <typename T> void Copy(const std::vector<T>& data, VkDeviceSize offset = 0) {
      Copy(0, sizeof(T) * data.size(), data.data(), offset);
    }

    static void CopyToMemory(const vk::DeviceMemory& memory,
                             const void* data,
                             vk::DeviceSize size,
                             vk::DeviceSize offset = 0);

    template <typename T> void CopyToMemory(const vk::DeviceMemory& memory, const T& data, size_t offset = 0) const;

    template <typename T> void CopyToMemory(const vk::DeviceMemory& memory,
                                            const std::vector<T>& data,
                                            size_t offset = 0) const;

    void Flush(int memoryIndex, vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0) const;

    void Invalidate(int memoryIndex, vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0) const;

    virtual void FreeMemory(int memoryIndex = 0);
  };
}
