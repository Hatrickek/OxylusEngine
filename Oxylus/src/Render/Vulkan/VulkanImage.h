#pragma once
#include <vulkan/vulkan.hpp>

#include "vk_mem_alloc.h"
#include "VulkanCommandBuffer.h"
#include "Core/Base.h"
#include "Core/Types.h"

namespace Oxylus {
  enum class ImageType {
    TYPE_2D,
    TYPE_CUBE,
    TYPE_2DARRAY
  };

  struct VulkanImageDescription {
    std::string Path{};
    ImageType Type = ImageType::TYPE_2D;
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t Depth = 1;
    uint32_t MipLevels = 1;
    vk::Format Format = vk::Format::eR8G8B8A8Unorm;
    vk::ImageTiling ImageTiling = vk::ImageTiling::eOptimal;
    vk::ImageUsageFlags UsageFlags = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
                                     vk::ImageUsageFlagBits::eSampled;
    bool CreateView = true;
    bool CreateSampler = true;
    vk::ImageAspectFlags AspectFlag = vk::ImageAspectFlagBits::eColor;
    vk::SampleCountFlagBits SampleCount = vk::SampleCountFlagBits::e1;
    vk::SharingMode SharingMode = vk::SharingMode::eExclusive;
    vk::ImageLayout InitalImageLayout = vk::ImageLayout::eUndefined;
    vk::ImageLayout FinalImageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    unsigned char* EmbeddedData = nullptr;
    const unsigned char* EmbeddedStbData = nullptr;
    unsigned char* EmbeddedKtxData = nullptr;
    size_t EmbeddedDataLength = 0;
    bool CreateDescriptorSet = false;
    bool FlipOnLoad = false;
    bool TransitionLayoutAtCreate = true;
    vk::DescriptorSetLayout DescriptorSetLayout; //Optional
    vk::Filter SamplerFiltering = vk::Filter::eLinear;
    vk::BorderColor SamplerBorderColor = vk::BorderColor::eFloatOpaqueWhite;
    vk::SamplerAddressMode SamplerAddressMode = vk::SamplerAddressMode::eRepeat;
    int32_t ImageArrayLayerCount = 1;
    int32_t ViewArrayLayerCount = 1;
    int32_t BaseArrayLayerIndex = 0;
  };

  class VulkanImage {
  public:
    vk::DescriptorImageInfo DescriptorImageInfo;
    std::string Name = "Image";
    bool LoadCallback = false; //True when an image is loaded.
    size_t ImageSize = 0; //Total loaded image size

    VulkanImage() = default;

    VulkanImage(const VulkanImageDescription& imageDescription);

    void Create(const VulkanImageDescription& imageDescription);
    void CreateWithImage(const VulkanImageDescription& imageDescription, vk::Image image);

    void SetImageLayout(vk::ImageLayout oldImageLayout,
                        vk::ImageLayout newImageLayout,
                        vk::ImageSubresourceRange subresourceRange,
                        const VulkanCommandBuffer* cmdBuffer = nullptr);

    uint32_t GetWidth() const { return m_ImageDescription.Width; }
    uint32_t GetHeight() const { return m_ImageDescription.Height; }
    vk::Image GetImage() const { return m_Image; }
    const vk::ImageView& GetImageView() const { return m_View; }
    const vk::Sampler& GetImageSampler() const { return m_Sampler; }
    const VulkanImageDescription& GetDesc() const { return m_ImageDescription; }
    const vk::DescriptorSet& GetDescriptorSet() const { return m_DescSet; }
    const vk::DescriptorImageInfo& GetDescImageInfo() const { return DescriptorImageInfo; }
    const vk::ImageLayout& GetImageLayout() const { return m_ImageLayout; }
    static VulkanImageDescription GetColorAttachmentImageDescription(vk::Format format,
                                                                     uint32_t width,
                                                                     uint32_t height);
    static Ref<VulkanImage> GetBlankImage();
    static IVec3 GetMipMapLevelSize(uint32_t width, uint32_t height, uint32_t depth, uint32_t level);

    void Destroy();

  private:
    void LoadTextureFromFile();
    void LoadCubeMapFromFile(int version);
    void LoadKtxFile(int version = 1);
    void LoadStbFile();
    void CreateImage(const vk::Device& LogicalDevice, bool hasPath);
    void LoadAndCreateResources(bool hasPath);
    void CreateImageView();
    void CreateSampler();
    vk::DescriptorSet CreateDescriptorSet() const;

    vk::ImageLayout m_ImageLayout = vk::ImageLayout::eUndefined;
    VkImage m_Image{};
    vk::ImageView m_View{};
    vk::Sampler m_Sampler{};
    vk::DescriptorSet m_DescSet{};
    VulkanImageDescription m_ImageDescription{};
    const uint8_t* m_ImageData = nullptr;
    VmaAllocation m_Allocation{};
  };
}
