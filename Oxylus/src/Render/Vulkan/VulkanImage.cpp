#include "src/oxpch.h"
#include "VulkanImage.h"
#include "VulkanContext.h"
#include "VulkanRenderer.h"
#include "Utils/VulkanUtils.h"
#include "Core/Resources.h"
#include "Utils/Profiler.h"

#include <fstream>
#include <ktx.h>
#include <ktxvulkan.h>
#include <stb_image.h>

namespace Oxylus {
  vk::AccessFlags AccessFlagsForLayout(const vk::ImageLayout layout) {
    switch (layout) {
      case vk::ImageLayout::ePreinitialized: return vk::AccessFlagBits::eHostWrite;
      case vk::ImageLayout::eTransferDstOptimal: return vk::AccessFlagBits::eTransferWrite;
      case vk::ImageLayout::eTransferSrcOptimal: return vk::AccessFlagBits::eTransferRead;
      case vk::ImageLayout::eColorAttachmentOptimal: return vk::AccessFlagBits::eColorAttachmentWrite;
      case vk::ImageLayout::eDepthStencilAttachmentOptimal: return vk::AccessFlagBits::eDepthStencilAttachmentWrite;
      case vk::ImageLayout::eShaderReadOnlyOptimal: return vk::AccessFlagBits::eShaderRead;
      default: return vk::AccessFlags();
    }
  }

  inline vk::PipelineStageFlags PipelineStageForLayout(vk::ImageLayout layout) {
    switch (layout) {
      case vk::ImageLayout::eTransferDstOptimal:
      case vk::ImageLayout::eTransferSrcOptimal: return vk::PipelineStageFlagBits::eTransfer;

      case vk::ImageLayout::eColorAttachmentOptimal: return vk::PipelineStageFlagBits::eColorAttachmentOutput;

      case vk::ImageLayout::eDepthStencilAttachmentOptimal: return vk::PipelineStageFlagBits::eEarlyFragmentTests;

      case vk::ImageLayout::eShaderReadOnlyOptimal: return vk::PipelineStageFlagBits::eFragmentShader;

      case vk::ImageLayout::ePreinitialized: return vk::PipelineStageFlagBits::eHost;

      case vk::ImageLayout::eUndefined: return vk::PipelineStageFlagBits::eTopOfPipe;

      default: return vk::PipelineStageFlagBits::eBottomOfPipe;
    }
  }

  VulkanImage::VulkanImage(const VulkanImageDescription& imageDescription) {
    Create(imageDescription);
  }

  void VulkanImage::CreateImage(const bool hasPath) {
    if (m_ImageDescription.FlipOnLoad)
      stbi_set_flip_vertically_on_load(true);

    if (!hasPath && !m_ImageDescription.EmbeddedStbData && !m_ImageDescription.EmbeddedKtxData) {
      vk::ImageCreateInfo imageCreateInfo;
      imageCreateInfo.imageType = vk::ImageType::e2D;
      imageCreateInfo.extent = vk::Extent3D{m_ImageDescription.Width, m_ImageDescription.Height, m_ImageDescription.Depth};
      imageCreateInfo.mipLevels = m_ImageDescription.MipLevels;
      imageCreateInfo.format = m_ImageDescription.Format;
      imageCreateInfo.arrayLayers = m_ImageDescription.Type == ImageType::TYPE_CUBE ? 6 : m_ImageDescription.ImageArrayLayerCount;
      imageCreateInfo.tiling = m_ImageDescription.ImageTiling;
      imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
      imageCreateInfo.usage = m_ImageDescription.UsageFlags;
      imageCreateInfo.samples = m_ImageDescription.SampleCount;
      imageCreateInfo.sharingMode = m_ImageDescription.SharingMode;
      imageCreateInfo.flags = m_ImageDescription.Type == ImageType::TYPE_CUBE
                                ? vk::ImageCreateFlagBits::eCubeCompatible
                                : vk::ImageCreateFlags{};

      const VkImageCreateInfo _imageci = (VkImageCreateInfo)imageCreateInfo;
      VmaAllocationCreateInfo allocationCreateInfo{};
      allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
      VmaAllocationInfo allocInfo{};
      vmaCreateImage(VulkanContext::GetAllocator(), &_imageci, &allocationCreateInfo, &m_Image, &m_Allocation, &allocInfo);
      if (m_ImageDescription.TransitionLayoutAtCreate) {
        vk::ImageSubresourceRange subresourceRange;
        subresourceRange.aspectMask = m_ImageDescription.AspectFlag;
        subresourceRange.levelCount = m_ImageDescription.MipLevels;
        subresourceRange.layerCount = 1;
        if (m_ImageDescription.MipLevels > 1 && m_ImageDescription.Type != ImageType::TYPE_CUBE)
          SetImageLayout(m_ImageDescription.InitalImageLayout, (vk::ImageLayout)VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
        else
          SetImageLayout(m_ImageDescription.InitalImageLayout, m_ImageDescription.FinalImageLayout, subresourceRange);
      }
      GPUMemory::TotalAllocated += allocInfo.size;
      ImageSize = allocInfo.size;
    }
  }

  void VulkanImage::LoadAndCreateResources(const bool hasPath) {
    if (hasPath || m_ImageDescription.EmbeddedData || m_ImageDescription.EmbeddedStbData || m_ImageDescription.EmbeddedKtxData) {
      LoadTextureFromFile();
    }

    if (m_ImageDescription.MipLevels > 1 && m_ImageDescription.Type != ImageType::TYPE_CUBE || m_ImageDescription.GenerateMips)
      GenerateMips();

    // Create view
    if (m_ImageDescription.CreateView)
      m_View = CreateImageView();

    // Create sampler
    if (m_ImageDescription.CreateSampler)
      CreateSampler();

    if (m_ImageDescription.CreateSampler && m_ImageDescription.CreateView && m_ImageDescription.CreateDescriptorSet)
      m_DescSet = CreateDescriptorSet();

    DescriptorImageInfo.imageLayout = m_ImageLayout;
    DescriptorImageInfo.imageView = m_View;
    DescriptorImageInfo.sampler = m_Sampler;

    if (m_ImageDescription.FlipOnLoad)
      stbi_set_flip_vertically_on_load(false);
  }

  void VulkanImage::Create(const VulkanImageDescription& imageDescription) {
    m_ImageDescription = imageDescription;

    const bool hasPath = !m_ImageDescription.Path.empty();

    CreateImage(hasPath);

    LoadAndCreateResources(hasPath);

    LoadCallback = true;
  }

  void VulkanImage::CreateWithImage(const VulkanImageDescription& imageDescription, const vk::Image image) {
    m_ImageDescription = imageDescription;
    m_Image = image;

    const bool hasPath = !m_ImageDescription.Path.empty();
    LoadAndCreateResources(hasPath);
  }

  void VulkanImage::LoadTextureFromFile() {
    const auto& path = m_ImageDescription.Path;
    const std::filesystem::path filepath = path;
    const auto extension = filepath.extension();

    if (extension == ".hdr")
      OX_CORE_ERROR("Loading .hdr files are not supported! .hdr files should be converted to .KTX files to load.");
    if (extension == ".ktx") {
      if (m_ImageDescription.Type == ImageType::TYPE_CUBE)
        LoadCubeMapFromFile(1);
      else
        LoadKtxFile(1);
    }
    if (extension == ".ktx2" || m_ImageDescription.EmbeddedKtxData) {
      if (m_ImageDescription.Type == ImageType::TYPE_CUBE)
        LoadCubeMapFromFile(2);
      else
        LoadKtxFile(2);
    }
    if (extension != ".ktx" && extension != ".ktx2")
      LoadStbFile();

    Name = filepath.filename().string();
  }

  void VulkanImage::LoadCubeMapFromFile(const int version) {
    ProfilerTimer timer;
    const auto& LogicalDevice = VulkanContext::GetDevice();
    const auto& PhysicalDevice = VulkanContext::GetPhysicalDevice();

    const auto& path = m_ImageDescription.Path;

    ktxTexture1* kTexture1 = nullptr;
    ktxTexture2* kTexture2 = nullptr;
    ktxVulkanTexture texture{};
    ktxVulkanDeviceInfo kvdi;

    ktxVulkanDeviceInfo_Construct(&kvdi,
      PhysicalDevice,
      LogicalDevice,
      VulkanContext::VulkanQueue.GraphicsQueue,
      VulkanRenderer::s_RendererContext.CommandPool,
      nullptr);

    if (version == 1) {
      ktxResult result = ktxTexture1_CreateFromNamedFile(path.c_str(), KTX_TEXTURE_CREATE_NO_FLAGS, &kTexture1);

      if (result != KTX_SUCCESS)
        OX_CORE_ERROR("Couldn't load the KTX texture file. {}", ktxErrorString(result));

      result = ktxTexture1_VkUploadEx(kTexture1,
        &kvdi,
        &texture,
        VK_IMAGE_TILING_OPTIMAL,
        static_cast<VkImageUsageFlags>(m_ImageDescription.UsageFlags),
        static_cast<VkImageLayout>(m_ImageDescription.FinalImageLayout));

      if (result != KTX_SUCCESS)
        OX_CORE_ERROR("Failed to upload the ktx texture. {}", ktxErrorString(result));

      m_ImageDescription.Width = kTexture1->baseWidth;
      m_ImageDescription.Height = kTexture1->baseHeight;
      m_ImageDescription.MipLevels = kTexture1->numLevels;

      ktxTexture_Destroy(ktxTexture(kTexture1));
    }

    if (version == 2) {
      ktxResult result = ktxTexture2_CreateFromNamedFile(path.c_str(), KTX_TEXTURE_CREATE_NO_FLAGS, &kTexture2);

      if (result != KTX_SUCCESS)
        OX_CORE_ERROR("Couldn't load the KTX texture file. {}", ktxErrorString(result));
      result = ktxTexture2_VkUploadEx(kTexture2,
        &kvdi,
        &texture,
        VK_IMAGE_TILING_OPTIMAL,
        static_cast<VkImageUsageFlags>(m_ImageDescription.UsageFlags),
        static_cast<VkImageLayout>(m_ImageDescription.FinalImageLayout));

      if (result != KTX_SUCCESS)
        OX_CORE_ERROR("Failed to upload the ktx texture. {}", ktxErrorString(result));

      m_ImageDescription.Width = kTexture2->baseWidth;
      m_ImageDescription.Height = kTexture2->baseHeight;
      m_ImageDescription.MipLevels = kTexture2->numLevels;

      ktxTexture_Destroy(ktxTexture(kTexture2));
    }

    m_Image = texture.image;
    m_ImageLayout = static_cast<vk::ImageLayout>(texture.imageLayout);

    m_ImageDescription.Format = static_cast<vk::Format>(texture.imageFormat);

    ktxVulkanDeviceInfo_Destruct(&kvdi);

    timer.Stop();
    OX_CORE_TRACE("Texture cube map loaded: {}, {} ms",
      std::filesystem::path(m_ImageDescription.Path).filename().string().c_str(),
      timer.ElapsedMilliSeconds());
  }

  void VulkanImage::LoadStbFile() {
    ProfilerTimer timer;

    const auto& path = m_ImageDescription.Path;
    int texWidth = 0, texHeight = 0, texChannels = 4;
    vk::ImageCreateInfo imageCreateInfo;
    imageCreateInfo.imageType = vk::ImageType::e2D;
    imageCreateInfo.extent = vk::Extent3D{m_ImageDescription.Width, m_ImageDescription.Height, m_ImageDescription.Depth};
    imageCreateInfo.arrayLayers = m_ImageDescription.Type == ImageType::TYPE_CUBE ? 6 : 1;
    imageCreateInfo.tiling = m_ImageDescription.ImageTiling;
    imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageCreateInfo.usage = m_ImageDescription.UsageFlags;
    imageCreateInfo.samples = m_ImageDescription.SampleCount;
    imageCreateInfo.sharingMode = m_ImageDescription.SharingMode;

    if (m_ImageDescription.EmbeddedStbData) {
      m_ImageData = m_ImageDescription.EmbeddedStbData;
    }

    else {
      if (m_ImageDescription.EmbeddedData) {
        m_ImageData = stbi_load_from_memory(m_ImageDescription.EmbeddedData,
          (int)m_ImageDescription.EmbeddedDataLength,
          &texWidth,
          &texHeight,
          &texChannels,
          STBI_rgb_alpha);
      }
      else
        m_ImageData = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
      if (!m_ImageData) {
        OX_CORE_BERROR("Failed to load texture file {}", path);
      }
    }
    if (m_ImageDescription.Width == 0)
      m_ImageDescription.Width = texWidth;
    if (m_ImageDescription.Height == 0)
      m_ImageDescription.Height = texHeight;

    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.extent.width = m_ImageDescription.Width;
    imageCreateInfo.extent.height = m_ImageDescription.Height;
    imageCreateInfo.format = m_ImageDescription.Format;
    size_t imageSize = static_cast<size_t>(m_ImageDescription.Width) * m_ImageDescription.Height * 4;

    if (m_ImageDescription.Format == vk::Format::eR32G32B32A32Sfloat) //TODO: Hardcoded
      imageSize = m_ImageDescription.EmbeddedDataLength;

    const int sizeMultiplier = GetDesc().Type == ImageType::TYPE_CUBE ? 6 : 1;

    VulkanBuffer stagingBuffer;
    stagingBuffer.CreateBuffer(vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible |
      vk::MemoryPropertyFlagBits::eHostCoherent,
      imageSize,
      m_ImageData,
      VMA_MEMORY_USAGE_AUTO_PREFER_HOST).Map();

    if (m_ImageData)
      stagingBuffer.Copy(m_ImageData, imageSize);
    stagingBuffer.Flush();
    stagingBuffer.Unmap();

    if (!m_ImageDescription.Path.empty())
      delete m_ImageData;

    const VkImageCreateInfo _imagecreateinfo = imageCreateInfo;
    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    VmaAllocationInfo allocInfo{};
    VulkanUtils::CheckResult(
      vmaCreateImage(VulkanContext::GetAllocator(), &_imagecreateinfo, &allocationInfo, &m_Image, &m_Allocation, &allocInfo));
    GPUMemory::TotalAllocated += allocInfo.size;
    ImageSize = allocInfo.size;

    vk::ImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = sizeMultiplier;

    VulkanCommandBuffer copyCmd;
    copyCmd.CreateBuffer();

    vk::ImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.image = m_Image;
    imageMemoryBarrier.subresourceRange = subresourceRange;
    imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
    imageMemoryBarrier.oldLayout = vk::ImageLayout::eUndefined;
    imageMemoryBarrier.newLayout = vk::ImageLayout::eTransferDstOptimal;

    std::vector<vk::BufferImageCopy> bufferCopyRegions;
    bufferCopyRegions.clear();

    vk::BufferImageCopy bufferCopyRegion = {};
    bufferCopyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    bufferCopyRegion.imageSubresource.layerCount = sizeMultiplier;
    bufferCopyRegion.imageExtent.width = m_ImageDescription.Width;
    bufferCopyRegion.imageExtent.height = m_ImageDescription.Height;
    bufferCopyRegion.imageExtent.depth = 1;
    bufferCopyRegions.push_back(bufferCopyRegion);

    copyCmd.Begin(vk::CommandBufferBeginInfo{});
    copyCmd.Get().pipelineBarrier(vk::PipelineStageFlagBits::eHost,
      vk::PipelineStageFlagBits::eTransfer,
      {},
      nullptr,
      nullptr,
      imageMemoryBarrier);
    copyCmd.Get().copyBufferToImage(stagingBuffer.Get(),
      m_Image,
      vk::ImageLayout::eTransferDstOptimal,
      static_cast<uint32_t>(bufferCopyRegions.size()),
      bufferCopyRegions.data());

    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    imageMemoryBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    imageMemoryBarrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    copyCmd.Get().pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
      vk::PipelineStageFlagBits::eFragmentShader,
      vk::DependencyFlags{},
      nullptr,
      nullptr,
      imageMemoryBarrier);
    copyCmd.End();

    VulkanRenderer::SubmitQueue(copyCmd);

    m_ImageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    stagingBuffer.Destroy();

    timer.Stop();
    if (!m_ImageDescription.Path.empty())
      OX_CORE_TRACE("Texture loaded: {}, {} ms",
      std::filesystem::path(m_ImageDescription.Path).filename().string().c_str(),
      timer.ElapsedMilliSeconds());
  }

  void VulkanImage::LoadKtxFile(const int version) {
    ProfilerTimer timer;
    const auto& LogicalDevice = VulkanContext::Context.Device;
    const auto& PhysicalDevice = VulkanContext::Context.PhysicalDevice;

    const auto& path = m_ImageDescription.Path;
    ktxTexture1* kTexture1 = nullptr;
    ktxTexture2* kTexture2 = nullptr;
    ktxVulkanTexture texture;
    ktxVulkanDeviceInfo kvdi;

    ktxVulkanDeviceInfo_Construct(&kvdi,
      PhysicalDevice,
      LogicalDevice,
      VulkanContext::VulkanQueue.GraphicsQueue,
      VulkanRenderer::s_RendererContext.CommandPool,
      nullptr);

    if (version == 1) {
      ktxResult result = ktxTexture1_CreateFromNamedFile(path.c_str(), KTX_TEXTURE_CREATE_NO_FLAGS, &kTexture1);

      if (result != KTX_SUCCESS)
        OX_CORE_ERROR("Couldn't load the KTX texture file. {}", ktxErrorString(result));

      result = ktxTexture1_VkUploadEx(kTexture1,
        &kvdi,
        &texture,
        VK_IMAGE_TILING_OPTIMAL,
        static_cast<VkImageUsageFlags>(m_ImageDescription.UsageFlags),
        static_cast<VkImageLayout>(m_ImageDescription.FinalImageLayout));

      if (result != KTX_SUCCESS)
        OX_CORE_ERROR("Failed to upload the ktx texture. {}", ktxErrorString(result));

      m_ImageDescription.Width = kTexture1->baseWidth;
      m_ImageDescription.Height = kTexture1->baseHeight;
      m_ImageDescription.MipLevels = kTexture1->numLevels;

      ktxTexture_Destroy(ktxTexture(kTexture1));
    }

    if (version == 2) {
      ktxResult result;
      if (m_ImageDescription.EmbeddedKtxData)
        result = ktxTexture2_CreateFromMemory(m_ImageDescription.EmbeddedKtxData,
          sizeof m_ImageDescription.EmbeddedKtxData,
          KTX_TEXTURE_CREATE_NO_FLAGS,
          &kTexture2);
      else
        result = ktxTexture2_CreateFromNamedFile(path.c_str(), KTX_TEXTURE_CREATE_NO_FLAGS, &kTexture2);

      if (result != KTX_SUCCESS)
        OX_CORE_ERROR("Couldn't load the KTX texture file. {}", ktxErrorString(result));

      result = ktxTexture2_VkUploadEx(kTexture2,
        &kvdi,
        &texture,
        VK_IMAGE_TILING_OPTIMAL,
        static_cast<VkImageUsageFlags>(m_ImageDescription.UsageFlags),
        static_cast<VkImageLayout>(m_ImageDescription.FinalImageLayout));

      if (result != KTX_SUCCESS)
        OX_CORE_ERROR("Failed to upload the ktx texture. {}", ktxErrorString(result));

      m_ImageDescription.Width = kTexture2->baseWidth;
      m_ImageDescription.Height = kTexture2->baseHeight;
      m_ImageDescription.MipLevels = kTexture2->numLevels;

      ktxTexture_Destroy(ktxTexture(kTexture2));
    }

    m_Image = texture.image;
    m_ImageLayout = static_cast<vk::ImageLayout>(texture.imageLayout);

    m_ImageDescription.Format = static_cast<vk::Format>(texture.imageFormat);

    ktxVulkanDeviceInfo_Destruct(&kvdi);

    timer.Stop();
    OX_CORE_TRACE("Image loaded: {}, {} ms",
      std::filesystem::path(m_ImageDescription.Path).filename().string().c_str(),
      timer.ElapsedMilliSeconds());
  }

  vk::ImageView VulkanImage::CreateImageView(const uint32_t mipmapIndex) const {
    const auto& LogicalDevice = VulkanContext::GetDevice();

    vk::ImageViewCreateInfo viewCreateInfo;
    viewCreateInfo.image = m_Image;
    if (GetDesc().Type == ImageType::TYPE_CUBE)
      viewCreateInfo.viewType = vk::ImageViewType::eCube;
    else if (GetDesc().Type == ImageType::TYPE_2D)
      viewCreateInfo.viewType = vk::ImageViewType::e2D;
    else if (GetDesc().Type == ImageType::TYPE_2DARRAY)
      viewCreateInfo.viewType = vk::ImageViewType::e2DArray;

    viewCreateInfo.format = GetDesc().Format;
    viewCreateInfo.components = {
      vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA
    };
    viewCreateInfo.subresourceRange.aspectMask = m_ImageDescription.AspectFlag;
    viewCreateInfo.subresourceRange.baseMipLevel = mipmapIndex;
    viewCreateInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    viewCreateInfo.subresourceRange.baseArrayLayer = m_ImageDescription.BaseArrayLayerIndex;
    viewCreateInfo.subresourceRange.layerCount = GetDesc().Type == ImageType::TYPE_CUBE ? 6 : m_ImageDescription.ViewArrayLayerCount;

    const auto res = LogicalDevice.createImageView(viewCreateInfo, nullptr);
    VulkanUtils::CheckResult(res.result);
    return res.value;
  }

  void VulkanImage::CreateSampler() {
    const auto& LogicalDevice = VulkanContext::Context.Device;

    vk::SamplerCreateInfo sampler;
    sampler.minFilter = m_ImageDescription.MinFiltering;
    sampler.magFilter = m_ImageDescription.MagFiltering;
    sampler.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sampler.addressModeU = m_ImageDescription.SamplerAddressMode;
    sampler.addressModeV = m_ImageDescription.SamplerAddressMode;
    sampler.addressModeW = m_ImageDescription.SamplerAddressMode;
    sampler.mipLodBias = 0.0f;
    sampler.compareOp = vk::CompareOp::eNever;
    sampler.minLod = 0.0f;
    sampler.maxLod = static_cast<float>(m_ImageDescription.MipLevels);
    if (VulkanContext::Context.DeviceFeatures.samplerAnisotropy) {
      sampler.maxAnisotropy = VulkanContext::Context.DeviceProperties.limits.maxSamplerAnisotropy;
      sampler.anisotropyEnable = VK_TRUE;
    }
    else {
      sampler.maxAnisotropy = 1.0;
      sampler.anisotropyEnable = VK_FALSE;
    }
    sampler.borderColor = m_ImageDescription.SamplerBorderColor;
    VulkanUtils::CheckResult(LogicalDevice.createSampler(&sampler, nullptr, &m_Sampler));
  }

  void VulkanImage::GenerateMips() {
    vk::FormatProperties formatProperties;
    VulkanContext::GetPhysicalDevice().getFormatProperties(m_ImageDescription.Format, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
      OX_CORE_WARN("Image format doesn't support linear blitting!");
      return;
    }
    VulkanRenderer::SubmitOnce([this](const VulkanCommandBuffer& cmdBuffer) {
      VkImageMemoryBarrier barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.image = GetImage();
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount = 1;
      barrier.subresourceRange.levelCount = 1;

      auto mipWidth = (int32_t)GetWidth();
      auto mipHeight = (int32_t)GetHeight();

      if (m_ImageDescription.GenerateMips)
        m_ImageDescription.MipLevels = GetMaxMipmapLevel(GetWidth(), GetHeight(), 1);

      for (uint32_t i = 1; i < m_ImageDescription.MipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vk::ImageMemoryBarrier bar = barrier;
        cmdBuffer.Get().pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, 0, nullptr, 0, nullptr, 1, &bar);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;
        vk::ImageBlit bli = blit;
        cmdBuffer.Get().blitImage(GetImage(), vk::ImageLayout::eTransferSrcOptimal, GetImage(), vk::ImageLayout::eTransferDstOptimal, 1, &bli, vk::Filter::eLinear);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = (VkImageLayout)m_ImageDescription.FinalImageLayout;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmdBuffer.Get(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        if (mipWidth > 1)
          mipWidth /= 2;
        if (mipHeight > 1)
          mipHeight /= 2;
      }

      barrier.subresourceRange.baseMipLevel = m_ImageDescription.MipLevels - 1;
      barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barrier.newLayout = (VkImageLayout)m_ImageDescription.FinalImageLayout;
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(cmdBuffer.Get(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

      m_ImageLayout = (vk::ImageLayout)barrier.newLayout;
    });
  }

  vk::DescriptorSet VulkanImage::CreateDescriptorSet() const {
    const auto& LogicalDevice = VulkanContext::Context.Device;

    vk::DescriptorSet descriptorSet;
    {
      vk::DescriptorSetAllocateInfo allocInfo = {};
      allocInfo.descriptorPool = VulkanRenderer::s_RendererContext.DescriptorPool;
      allocInfo.descriptorSetCount = 1;
      //if (m_ImageDescription.DescriptorSetLayout)
      //	allocInfo.pSetLayouts = &m_ImageDescription.DescriptorSetLayout;
      //else
      allocInfo.pSetLayouts = &VulkanRenderer::s_RendererData.ImageDescriptorSetLayout;
      VulkanUtils::CheckResult(LogicalDevice.allocateDescriptorSets(&allocInfo, &descriptorSet));
    }

    // Update the Descriptor Set:
    {
      vk::DescriptorImageInfo descImage[1] = {};
      descImage[0].sampler = m_Sampler;
      descImage[0].imageView = GetImageView();
      descImage[0].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
      vk::WriteDescriptorSet writeDesc[1] = {};
      writeDesc[0].dstSet = descriptorSet;
      writeDesc[0].descriptorCount = 1;
      writeDesc[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
      writeDesc[0].pImageInfo = descImage;
      LogicalDevice.updateDescriptorSets(1, writeDesc, 0, nullptr);
    }
    return descriptorSet;
  }

  void VulkanImage::SetImageLayout(const vk::ImageLayout oldImageLayout,
                                   const vk::ImageLayout newImageLayout,
                                   const vk::ImageSubresourceRange subresourceRange,
                                   const VulkanCommandBuffer* cmdBuffer) {
    if (cmdBuffer) {
      vk::ImageMemoryBarrier imageMemoryBarrier;
      imageMemoryBarrier.oldLayout = oldImageLayout;
      imageMemoryBarrier.newLayout = newImageLayout;
      imageMemoryBarrier.image = m_Image;
      imageMemoryBarrier.subresourceRange = subresourceRange;
      imageMemoryBarrier.srcAccessMask = AccessFlagsForLayout(oldImageLayout);
      imageMemoryBarrier.dstAccessMask = AccessFlagsForLayout(newImageLayout);
      const vk::PipelineStageFlags srcStageMask = PipelineStageForLayout(oldImageLayout);
      const vk::PipelineStageFlags destStageMask = PipelineStageForLayout(newImageLayout);
      cmdBuffer->Get().pipelineBarrier(srcStageMask,
        destStageMask,
        vk::DependencyFlags(),
        nullptr,
        nullptr,
        imageMemoryBarrier);
    }
    else {
      VulkanRenderer::SubmitOnce([&](const VulkanCommandBuffer& cmd) {
        vk::ImageMemoryBarrier imageMemoryBarrier;
        imageMemoryBarrier.oldLayout = oldImageLayout;
        imageMemoryBarrier.newLayout = newImageLayout;
        imageMemoryBarrier.image = m_Image;
        imageMemoryBarrier.subresourceRange = subresourceRange;
        imageMemoryBarrier.srcAccessMask = AccessFlagsForLayout(oldImageLayout);
        imageMemoryBarrier.dstAccessMask = AccessFlagsForLayout(newImageLayout);
        const vk::PipelineStageFlags srcStageMask = PipelineStageForLayout(oldImageLayout);
        const vk::PipelineStageFlags destStageMask = PipelineStageForLayout(newImageLayout);
        cmd.Get().pipelineBarrier(srcStageMask,
          destStageMask,
          vk::DependencyFlags(),
          nullptr,
          nullptr,
          imageMemoryBarrier);
      });
    }
    m_ImageLayout = newImageLayout;
  }

  std::vector<vk::DescriptorImageInfo> VulkanImage::GetMipDescriptors() const {
    std::vector<vk::DescriptorImageInfo> result;

    // TODO: Do caching with maps

    for (uint32_t i = 0; i < m_ImageDescription.MipLevels; i++) {
      vk::DescriptorImageInfo desc;
      desc.imageLayout = m_ImageLayout;
      desc.imageView = CreateImageView(i);
      desc.sampler = m_Sampler;
      result.emplace_back(desc);
    }

    return result;
  }

  VulkanImageDescription VulkanImage::GetColorAttachmentImageDescription(vk::Format format,
                                                                         uint32_t width,
                                                                         uint32_t height) {
    VulkanImageDescription colorImageDesc;
    colorImageDesc.Format = format;
    colorImageDesc.UsageFlags = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    colorImageDesc.ImageTiling = vk::ImageTiling::eOptimal;
    colorImageDesc.Width = width;
    colorImageDesc.Height = height;
    colorImageDesc.CreateView = true;
    colorImageDesc.CreateSampler = true;
    colorImageDesc.CreateDescriptorSet = true;
    colorImageDesc.AspectFlag = vk::ImageAspectFlagBits::eColor;
    colorImageDesc.FinalImageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    return colorImageDesc;
  }

  Ref<VulkanImage> VulkanImage::GetBlankImage() {
    return Resources::s_EngineResources.EmptyTexture;
  }

  IVec3 VulkanImage::GetMipMapLevelSize(uint32_t width, uint32_t height, uint32_t depth, uint32_t level) {
    return {width / (1 << level), height / (1 << level), depth / (1 << level)};
  }

  uint32_t VulkanImage::GetMaxMipmapLevel(uint32_t width, uint32_t height, uint32_t depth) {
    return std::ilogb(std::max(width, std::max(height, depth)));
  }

  void VulkanImage::Destroy() {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    if (m_View) {
      LogicalDevice.destroyImageView(m_View, nullptr);
    }
    vmaDestroyImage(VulkanContext::GetAllocator(), m_Image, m_Allocation);
    GPUMemory::TotalFreed += ImageSize;
    if (m_Sampler) {
      LogicalDevice.destroySampler(m_Sampler);
    }
  }
}
