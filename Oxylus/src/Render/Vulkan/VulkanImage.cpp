#include "src/oxpch.h"
#include "VulkanImage.h"
#include "VulkanContext.h"
#include "VulkanRenderer.h"
#include "Utils/VulkanUtils.h"
#include <fstream>
#include <ktx.h>
#include <ktxvulkan.h>

#include "Core/Resources.h"
#include "tinygltf/stb_image.h"
#include "Utils/Profiler.h"

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

  void VulkanImage::CreateImage(const vk::Device& LogicalDevice, const bool hasPath) {
    vk::ImageCreateInfo ImageCI;
    ImageCI.imageType = vk::ImageType::e2D;
    ImageCI.extent = vk::Extent3D{m_ImageDescription.Width, m_ImageDescription.Height, m_ImageDescription.Depth};
    ImageCI.mipLevels = m_ImageDescription.MipLevels;
    ImageCI.format = m_ImageDescription.Format;
    ImageCI.arrayLayers = m_ImageDescription.Type == ImageType::TYPE_CUBE ? 6 : m_ImageDescription.ImageArrayLayerCount;
    ImageCI.tiling = m_ImageDescription.ImageTiling;
    ImageCI.initialLayout = vk::ImageLayout::eUndefined;
    ImageCI.usage = m_ImageDescription.UsageFlags;
    ImageCI.samples = m_ImageDescription.SampleCount;
    ImageCI.sharingMode = m_ImageDescription.SharingMode;
    ImageCI.flags = m_ImageDescription.Type == ImageType::TYPE_CUBE
                      ? vk::ImageCreateFlagBits::eCubeCompatible
                      : vk::ImageCreateFlags{};

    if (m_ImageDescription.FlipOnLoad)
      stbi_set_flip_vertically_on_load(true);

    if (!hasPath && !m_ImageDescription.EmbeddedStbData && !m_ImageDescription.EmbeddedKtxData) {
      VulkanUtils::CheckResult(LogicalDevice.createImage(&ImageCI, nullptr, &m_Image));
      Memories.emplace_back(Memory{});
      // Query memory requirements.
      LogicalDevice.getImageMemoryRequirements(m_Image, &Memories[0].MemoryRequirements);
      if (!FindMemoryTypeIndex(0)) {
        OX_CORE_ERROR("Required memory type not found. Image not valid.");
      }
      // Allocate memory
      Allocate(0);
      BindImageMemory(0, GetImage());
      if (m_ImageDescription.TransitionLayoutAtCreate) {
        vk::ImageSubresourceRange subresourceRange;
        subresourceRange.aspectMask = m_ImageDescription.AspectFlag;
        subresourceRange.levelCount = 1;
        subresourceRange.layerCount = 1;
        SetImageLayout(m_ImageDescription.InitalImageLayout, m_ImageDescription.FinalImageLayout, subresourceRange);
      }
    }
  }

  void VulkanImage::LoadAndCreateResources(const bool hasPath) {
    if (hasPath || m_ImageDescription.EmbeddedData || m_ImageDescription.EmbeddedStbData || m_ImageDescription.
        EmbeddedKtxData) {
      LoadTextureFromFile();
    }

    // Create view
    if (m_ImageDescription.CreateView)
      CreateImageView();

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
    const auto& LogicalDevice = VulkanContext::Context.Device;

    m_ImageDescription = imageDescription;

    const bool hasPath = !m_ImageDescription.Path.empty();

    CreateImage(LogicalDevice, hasPath);

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

    CreateSampler();

    CreateImageView();

    DescriptorImageInfo.imageLayout = GetImageLayout();
    DescriptorImageInfo.imageView = GetImageView();
    DescriptorImageInfo.sampler = m_Sampler;

    timer.Stop();
    OX_CORE_TRACE("Texture cube map loaded: {}, {} ms",
      std::filesystem::path(m_ImageDescription.Path).filename().string().c_str(),
      timer.ElapsedMilliSeconds());
  }

  void VulkanImage::LoadStbFile() {
    ProfilerTimer timer;

    const auto& path = m_ImageDescription.Path;
    const auto& LogicalDevice = VulkanContext::Context.Device;
    int texWidth = 0, texHeight = 0, texChannels = 4;
    vk::ImageCreateInfo ImageCI;
    ImageCI.imageType = vk::ImageType::e2D;
    ImageCI.extent = vk::Extent3D{m_ImageDescription.Width, m_ImageDescription.Height, m_ImageDescription.Depth};
    ImageCI.mipLevels = m_ImageDescription.MipLevels;
    ImageCI.format = m_ImageDescription.Format;
    ImageCI.arrayLayers = m_ImageDescription.Type == ImageType::TYPE_CUBE ? 6 : 1;
    ImageCI.tiling = m_ImageDescription.ImageTiling;
    ImageCI.initialLayout = vk::ImageLayout::eUndefined;
    ImageCI.usage = m_ImageDescription.UsageFlags;
    ImageCI.samples = m_ImageDescription.SampleCount;
    ImageCI.sharingMode = m_ImageDescription.SharingMode;

    if (m_ImageDescription.EmbeddedStbData)
      m_ImageData = m_ImageDescription.EmbeddedStbData;

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
        OX_CORE_ERROR("Failed to load texture file {}", path);
        OX_DEBUGBREAK();
      }
    }
    if (m_ImageDescription.Width == 0)
      m_ImageDescription.Width = texWidth;
    if (m_ImageDescription.Height == 0)
      m_ImageDescription.Height = texHeight;

    ImageCI.mipLevels = 1;
    ImageCI.extent.width = m_ImageDescription.Width;
    ImageCI.extent.height = m_ImageDescription.Height;
    ImageCI.format = m_ImageDescription.Format;
    size_t imageSize = static_cast<size_t>(m_ImageDescription.Width) * m_ImageDescription.Height * 4;

    if (m_ImageDescription.Format == vk::Format::eR32G32B32A32Sfloat) //TODO: Hardcoded
      imageSize = m_ImageDescription.EmbeddedDataLength;

    const int sizeMultiplier = GetDesc().Type == ImageType::TYPE_CUBE ? 6 : 1;
    vk::Buffer stagingBuffer;
    Memories.clear();
    Memories.emplace_back(Memory{}); //Staging memory
    vk::BufferCreateInfo bufferCI;
    bufferCI.usage = vk::BufferUsageFlagBits::eTransferSrc;
    bufferCI.sharingMode = vk::SharingMode::eExclusive;
    bufferCI.size = imageSize;
    stagingBuffer = LogicalDevice.createBuffer(bufferCI).value;
    LogicalDevice.getBufferMemoryRequirements(stagingBuffer, &Memories[0].MemoryRequirements);
    Memories[0].memoryPropertyFlags = vk::MemoryPropertyFlagBits::eHostVisible |
                                      vk::MemoryPropertyFlagBits::eHostCoherent;
    FindMemoryTypeIndex(0);
    Allocate(0);
    BindBufferMemory(0, stagingBuffer, 0);

    Map(0, 0, imageSize * sizeMultiplier);
    if (m_ImageData)
      Copy(0, imageSize, m_ImageData);
    Flush(0);
    Unmap(0);

    //if (imageData && !m_ImageDescription.EmbeddedData)
    //	stbi_image_free(imageData);

    VulkanUtils::CheckResult(LogicalDevice.createImage(&ImageCI, nullptr, &m_Image));

    Memories.emplace_back(Memory{});
    LogicalDevice.getImageMemoryRequirements(m_Image, &Memories[1].MemoryRequirements);
    FindMemoryTypeIndex(1);
    Allocate(1);
    BindImageMemory(1, m_Image, 0);

    VulkanCommandBuffer copyCmd;
    copyCmd.CreateBuffer();

    vk::ImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = sizeMultiplier;

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
    copyCmd.Get().copyBufferToImage(stagingBuffer,
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

    LogicalDevice.destroyBuffer(stagingBuffer);
    FreeMemory(0);

    CreateSampler();
    CreateImageView();

    DescriptorImageInfo.imageLayout = GetImageLayout();
    DescriptorImageInfo.imageView = GetImageView();
    DescriptorImageInfo.sampler = m_Sampler;

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

    CreateSampler();
    CreateImageView();

    DescriptorImageInfo.imageLayout = GetImageLayout();
    DescriptorImageInfo.imageView = GetImageView();
    DescriptorImageInfo.sampler = m_Sampler;

    timer.Stop();
    OX_CORE_TRACE("Image loaded: {}, {} ms",
      std::filesystem::path(m_ImageDescription.Path).filename().string().c_str(),
      timer.ElapsedMilliSeconds());
  }

  void VulkanImage::CreateImageView() {
    const auto& LogicalDevice = VulkanContext::Context.Device;

    vk::ImageViewCreateInfo viewCreateInfo;
    viewCreateInfo.image = m_Image;
    if (GetDesc().Type == ImageType::TYPE_CUBE)
      viewCreateInfo.viewType = vk::ImageViewType::eCube;
    else if (GetDesc().Type == ImageType::TYPE_2D)
      viewCreateInfo.viewType = vk::ImageViewType::e2D;
    else
      if (GetDesc().Type == ImageType::TYPE_2DARRAY)
        viewCreateInfo.viewType = vk::ImageViewType::e2DArray;

    viewCreateInfo.format = GetDesc().Format;
    viewCreateInfo.components = {
      vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA
    };
    viewCreateInfo.subresourceRange.aspectMask = m_ImageDescription.AspectFlag;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = m_ImageDescription.MipLevels;
    viewCreateInfo.subresourceRange.baseArrayLayer = m_ImageDescription.BaseArrayLayerIndex;
    viewCreateInfo.subresourceRange.layerCount = GetDesc().Type == ImageType::TYPE_CUBE
                                                   ? 6
                                                   : m_ImageDescription.ViewArrayLayerCount;

    VulkanUtils::CheckResult(LogicalDevice.createImageView(&viewCreateInfo, nullptr, &m_View));
  }

  void VulkanImage::CreateSampler() {
    const auto& LogicalDevice = VulkanContext::Context.Device;

    vk::SamplerCreateInfo sampler;
    sampler.magFilter = m_ImageDescription.SamplerFiltering;
    sampler.minFilter = m_ImageDescription.SamplerFiltering;
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
    return CreateRef<VulkanImage>(Resources::s_EngineResources.EmptyTexture);
  }

  IVec3 VulkanImage::GetMipMapLevelSize(uint32_t width, uint32_t height, uint32_t depth, uint32_t level) {
    return {width / (1 << level), height / (1 << level), depth / (1 << level)};
  }

  void VulkanImage::Destroy() {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    if (m_View) {
      LogicalDevice.destroyImageView(m_View, nullptr);
    }
    if (m_Image) {
      LogicalDevice.destroyImage(m_Image, nullptr);
    }
    for (int i = 0; i < static_cast<int>(Memories.size()); i++) {
      FreeMemory(i);
    }
    if (m_Sampler) {
      LogicalDevice.destroySampler(m_Sampler);
    }
  }
}
