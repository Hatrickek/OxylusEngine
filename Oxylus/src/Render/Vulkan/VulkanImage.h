#pragma once
#include <future>
#include <vk_mem_alloc.h>

#include "VulkanCommandBuffer.h"
#include "Core/Base.h"
#include "Core/Types.h"

namespace Oxylus {

  enum class ImageType {
    TYPE_2D,
    TYPE_CUBE,
    TYPE_2DARRAY
  };

  // TODO(hatrickek): Cleanup
  struct VulkanImageDescription {
    std::string Path{};
    ImageType Type = ImageType::TYPE_2D;
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t Depth = 1;
    uint32_t MipLevels = 1;     // If it is above 1 even when `GenerateMips` is false, it will still generate mips.
    bool GenerateMips = false;  // Will override the `MipLevels` value and get the max mip level.
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
    size_t EmbeddedDataLength = 0;
    bool CreateDescriptorSet = false;
    bool FlipOnLoad = false;
    bool TransitionLayoutAtCreate = true;
    vk::DescriptorSetLayout DescriptorSetLayout; //Optional
    vk::Filter MinFiltering = vk::Filter::eLinear;
    vk::Filter MagFiltering = vk::Filter::eLinear;
    vk::BorderColor SamplerBorderColor = vk::BorderColor::eFloatOpaqueWhite;
    vk::SamplerAddressMode SamplerAddressMode = vk::SamplerAddressMode::eRepeat;
    int32_t ImageArrayLayerCount = 1;
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

    /// Loads the given file using stb. Returned data must be freed manually.
    static uint8_t* LoadStbRawImage(const std::string& filename,
                                    uint32_t* width = nullptr,
                                    uint32_t* height = nullptr,
                                    uint32_t* bits = nullptr,
                                    bool flipY = false,
                                    bool srgb = true);

    void SetImageLayout(vk::ImageLayout oldImageLayout,
                        vk::ImageLayout newImageLayout,
                        const vk::ImageSubresourceRange& subresourceRange,
                        const VulkanCommandBuffer* cmdBuffer = nullptr);

    uint32_t GetWidth() const { return m_ImageDescription.Width; }
    uint32_t GetHeight() const { return m_ImageDescription.Height; }
    vk::Image GetImage() const { return m_Image; }
    const vk::ImageView& GetImageView(const uint32_t index = 0) const { return m_Views[index]; }
    const vk::Sampler& GetImageSampler() const { return m_Sampler; }
    const VulkanImageDescription& GetDesc() const { return m_ImageDescription; }
    const vk::DescriptorSet& GetDescriptorSet() const { return m_DescSet; }
    const vk::DescriptorImageInfo& GetDescImageInfo() const { return DescriptorImageInfo; }
    const vk::ImageLayout& GetImageLayout() const { return m_ImageLayout; }
    std::vector<vk::DescriptorImageInfo> GetMipDescriptors() const;
    static VulkanImageDescription GetColorAttachmentImageDescription(vk::Format format,
                                                                     uint32_t width,
                                                                     uint32_t height);
    static Ref<VulkanImage> GetBlankImage();
    static IVec3 GetMipMapLevelSize(uint32_t width, uint32_t height, uint32_t depth, uint32_t level);
    static uint32_t GetMaxMipmapLevel(uint32_t width, uint32_t height, uint32_t depth);

    void Destroy() const;

  private:
    void LoadTextureFromFile();
    void LoadCubeMapFromFile(int version);
    void LoadKtxFile(int version = 1);
    void LoadStbFile();
    void CreateImage();
    void LoadAndCreateResources(bool hasPath);
    std::vector<vk::ImageView> CreateImageView(uint32_t mipmapIndex = 0) const;
    void CreateSampler();
    void GenerateMips();
    void TransitionLayout();
    vk::DescriptorSet CreateDescriptorSet() const;

    vk::ImageLayout m_ImageLayout = vk::ImageLayout::eUndefined;
    VkImage m_Image = {};
    std::vector<vk::ImageView> m_Views = {};
    vk::Sampler m_Sampler = {};
    vk::DescriptorSet m_DescSet = {};
    VulkanImageDescription m_ImageDescription = {};
    const uint8_t* m_ImageData = nullptr;
    VmaAllocation m_Allocation = {};
    vk::CommandPool m_CommandPool = {};
  };
}
