#include "oxpch.h"
#include "Prefilter.h"

#include "Render/Vulkan/VulkanContext.h"
#include "Render/Vulkan/VulkanPipeline.h"
#include "Render/Vulkan/VulkanRenderer.h"

#define M_PI       3.14159265358979323846

namespace Oxylus {
  void Prefilter::GenerateBRDFLUT(VulkanImage& target) {
    const auto& LogicalDevice = VulkanContext::Context.Device;

    constexpr auto format = vk::Format::eR16G16Sfloat;
    constexpr int32_t dim = 512;

    VulkanImageDescription description;
    description.Width = dim;
    description.Height = dim;
    description.CreateView = true;
    description.Format = format;
    description.UsageFlags = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    target.Create(description);

    // FB, Att, RP, Pipe, etc.
    VulkanRenderPass renderPass;
    {
      vk::AttachmentDescription attDesc;
      // Color attachment
      attDesc.format = format;
      attDesc.loadOp = vk::AttachmentLoadOp::eClear;
      attDesc.storeOp = vk::AttachmentStoreOp::eStore;
      attDesc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
      attDesc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
      attDesc.initialLayout = vk::ImageLayout::eUndefined;
      attDesc.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
      vk::AttachmentReference colorReference{0, vk::ImageLayout::eColorAttachmentOptimal};

      vk::SubpassDescription subpassDescription = {};
      subpassDescription.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
      subpassDescription.colorAttachmentCount = 1;
      subpassDescription.pColorAttachments = &colorReference;

      // Use subpass dependencies for layout transitions
      std::array dependencies{
        vk::SubpassDependency{
          VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eBottomOfPipe,
          vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eMemoryRead,
          vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
          vk::DependencyFlagBits::eByRegion
        },
        vk::SubpassDependency{
          0, VK_SUBPASS_EXTERNAL, vk::PipelineStageFlagBits::eColorAttachmentOutput,
          vk::PipelineStageFlagBits::eBottomOfPipe,
          vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
          vk::AccessFlagBits::eMemoryRead, vk::DependencyFlagBits::eByRegion
        },
      };

      // Create the actual renderpass
      vk::RenderPassCreateInfo renderPassCI;
      renderPassCI.attachmentCount = 1;
      renderPassCI.pAttachments = &attDesc;
      renderPassCI.subpassCount = 1;
      renderPassCI.pSubpasses = &subpassDescription;
      renderPassCI.dependencyCount = 2;
      renderPassCI.pDependencies = dependencies.data();
      renderPass.CreateRenderPass(renderPassCI);
    }

    vk::Framebuffer framebuffer;
    {
      vk::FramebufferCreateInfo framebufferCI;
      framebufferCI.renderPass = renderPass.Get();
      framebufferCI.attachmentCount = 1;
      framebufferCI.pAttachments = &target.GetImageView();
      framebufferCI.width = dim;
      framebufferCI.height = dim;
      framebufferCI.layers = 1;
      framebuffer = LogicalDevice.createFramebuffer(framebufferCI);
    }

    // Desriptors
    vk::DescriptorSetLayout descriptorsetlayout = LogicalDevice.createDescriptorSetLayout({});

    // Descriptor Pool
    std::vector<vk::DescriptorPoolSize> poolSizes{{vk::DescriptorType::eCombinedImageSampler, 1}};
    vk::DescriptorPool descriptorpool = LogicalDevice.createDescriptorPool({
      {}, 2, static_cast<uint32_t>(poolSizes.size()), poolSizes.data()
    });

    // Pipeline layout
    vk::PipelineLayout pipelinelayout = LogicalDevice.createPipelineLayout({{}, 1, &descriptorsetlayout});

    // Load shader
    VulkanShader shader = {
      ShaderCI{
        .VertexPath = "resources/shaders/GenBrdfLut.vert", .FragmentPath = "resources/shaders/GenBrdfLut.frag",
        .EntryPoint = "main", .Name = "BRDFLUT",
      }
    };

    // Pipeline
    VulkanPipeline pipeline;
    PipelineDescription pipelineDescription;
    pipelineDescription.Shader = CreateRef<VulkanShader>(shader);
    pipelineDescription.RasterizerDesc.CullMode = vk::CullModeFlagBits::eNone;
    pipelineDescription.DepthSpec.DepthEnable = false;
    pipelineDescription.DepthSpec.DepthWriteEnable = false;
    pipelineDescription.DepthSpec.CompareOp = vk::CompareOp::eNever;
    pipelineDescription.RenderPass = renderPass;
    pipeline.CreateGraphicsPipeline(pipelineDescription);

    // Render
    vk::ClearValue clearValues[1];
    clearValues[0].color = vk::ClearColorValue(std::array{0.0f, 0.0f, 0.0f, 1.0f});

    vk::RenderPassBeginInfo renderPassBeginInfo;
    renderPassBeginInfo.renderPass = renderPass.Get();
    renderPassBeginInfo.renderArea.extent.width = dim;
    renderPassBeginInfo.renderArea.extent.height = dim;
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = clearValues;
    renderPassBeginInfo.framebuffer = framebuffer;

    VulkanRenderer::SubmitOnce([&](const VulkanCommandBuffer& cmdBuf) {
      cmdBuf.Get().beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
      vk::Viewport viewport{0, 0, static_cast<float>(dim), static_cast<float>(dim), 0, 1};
      vk::Rect2D scissor{vk::Offset2D{}, vk::Extent2D{static_cast<uint32_t>(dim), static_cast<uint32_t>(dim)}};
      cmdBuf.Get().setViewport(0, viewport);
      cmdBuf.Get().setScissor(0, scissor);
      pipeline.BindPipeline(cmdBuf.Get());
      cmdBuf.Get().draw(3, 1, 0, 0);
      cmdBuf.Get().endRenderPass();
    });
    VulkanRenderer::WaitGraphicsQueueIdle();

    pipeline.Destroy();
    LogicalDevice.destroyPipelineLayout(pipelinelayout);
    renderPass.Destroy();
    LogicalDevice.destroyFramebuffer(framebuffer);
    LogicalDevice.destroyDescriptorSetLayout(descriptorsetlayout);
    LogicalDevice.destroyDescriptorPool(descriptorpool);
  }

  void Prefilter::GenerateIrradianceCube(VulkanImage& target,
                                         const Mesh& skybox,
                                         const VertexLayout& vertexLayout,
                                         const vk::DescriptorImageInfo& skyboxDescriptor) {
    const auto& LogicalDevice = VulkanContext::Context.Device;

    constexpr vk::Format format = vk::Format::eR32G32B32A32Sfloat;
    constexpr int32_t dim = 64;
    const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

    // Pre-filtered cube map
    // Image
    VulkanImageDescription imageDescription;
    imageDescription.Width = dim;
    imageDescription.Height = dim;
    imageDescription.MipLevels = numMips;
    imageDescription.UsageFlags = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
    imageDescription.Type = ImageType::TYPE_CUBE;
    imageDescription.Format = format;
    imageDescription.CreateView = true;
    imageDescription.CreateSampler = true;
    target.Create(imageDescription);

    VulkanRenderPass renderpass;
    {
      // FB, Att, RP, Pipe, etc.
      vk::AttachmentDescription attDesc = {};
      // Color attachment
      attDesc.format = format;
      attDesc.loadOp = vk::AttachmentLoadOp::eClear;
      attDesc.storeOp = vk::AttachmentStoreOp::eStore;
      attDesc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
      attDesc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
      attDesc.initialLayout = vk::ImageLayout::eUndefined;
      attDesc.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
      vk::AttachmentReference colorReference{0, vk::ImageLayout::eColorAttachmentOptimal};
      vk::SubpassDescription subpassDescription{{}, vk::PipelineBindPoint::eGraphics, 0, nullptr, 1, &colorReference};

      // Use subpass dependencies for layout transitions
      std::array dependencies{
        vk::SubpassDependency{
          VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eBottomOfPipe,
          vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eMemoryRead,
          vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
          vk::DependencyFlagBits::eByRegion
        },
        vk::SubpassDependency{
          0, VK_SUBPASS_EXTERNAL, vk::PipelineStageFlagBits::eColorAttachmentOutput,
          vk::PipelineStageFlagBits::eBottomOfPipe,
          vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
          vk::AccessFlagBits::eMemoryRead, vk::DependencyFlagBits::eByRegion
        },
      };

      // Renderpass
      vk::RenderPassCreateInfo renderPassCI;
      renderPassCI.attachmentCount = 1;
      renderPassCI.pAttachments = &attDesc;
      renderPassCI.subpassCount = 1;
      renderPassCI.pSubpasses = &subpassDescription;
      renderPassCI.dependencyCount = 2;
      renderPassCI.pDependencies = dependencies.data();

      renderpass.CreateRenderPass(renderPassCI);
    }

    struct {
      VulkanImage image;
      vk::Framebuffer framebuffer;
    } offscreen;

    // Offfscreen framebuffer
    {
      // Color attachment
      VulkanImageDescription description;
      description.Format = format;
      description.Height = dim;
      description.Width = dim;
      description.UsageFlags = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;
      description.CreateView = true;
      description.CreateSampler = false;
      description.TransitionLayoutAtCreate = false;
      offscreen.image.Create(description);

      vk::FramebufferCreateInfo fbufCreateInfo;
      fbufCreateInfo.renderPass = renderpass.Get();
      fbufCreateInfo.attachmentCount = 1;
      fbufCreateInfo.pAttachments = &offscreen.image.GetImageView();
      fbufCreateInfo.width = dim;
      fbufCreateInfo.height = dim;
      fbufCreateInfo.layers = 1;
      offscreen.framebuffer = LogicalDevice.createFramebuffer(fbufCreateInfo);
      vk::ImageSubresourceRange subresourceRange;
      subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
      subresourceRange.levelCount = 1;
      subresourceRange.layerCount = 1;
      offscreen.image.SetImageLayout(vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        subresourceRange);
    }

    // Descriptors
    vk::DescriptorSetLayout descriptorsetlayout;
    vk::DescriptorSetLayoutBinding setLayoutBinding{
      0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment
    };
    descriptorsetlayout = LogicalDevice.createDescriptorSetLayout({{}, 1, &setLayoutBinding});

    // Descriptor Pool
    vk::DescriptorPoolSize poolSize{vk::DescriptorType::eCombinedImageSampler, 1};
    vk::DescriptorPool descriptorpool = LogicalDevice.createDescriptorPool({{}, 2, 1, &poolSize});
    // Descriptor sets
    vk::DescriptorSet descriptorset = LogicalDevice.allocateDescriptorSets({descriptorpool, 1, &descriptorsetlayout})[
      0];
    vk::WriteDescriptorSet writeDescriptorSet{
      descriptorset, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &skyboxDescriptor
    };
    LogicalDevice.updateDescriptorSets(writeDescriptorSet, nullptr);

    // Pipeline layout
    struct PushBlock {
      glm::mat4 mvp;
      // Sampling deltas
      float deltaPhi = (2.0f * static_cast<float>(M_PI)) / 180.0f;
      float deltaTheta = (0.5f * static_cast<float>(M_PI)) / 64.0f;
    } pushBlock;
    vk::PushConstantRange pushConstantRange{
      vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushBlock)
    };

    vk::PipelineLayout pipelinelayout = LogicalDevice.createPipelineLayout(vk::PipelineLayoutCreateInfo{
      {}, 1, &descriptorsetlayout, 1, &pushConstantRange
    });

    // Load shader
    VulkanShader shader = {
      ShaderCI{
        .VertexPath = "resources/shaders/FilterCube.vert", .FragmentPath = "resources/shaders/IrradianceCube.frag",
        .EntryPoint = "main",
      }
    };

    // Pipeline
    VulkanPipeline pipeline;
    PipelineDescription pipelineDescription;
    pipelineDescription.DescriptorSetLayoutBindings = {
      {{0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}}
    };
    pipelineDescription.PushConstantRanges = {
      vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushBlock)}
    };
    pipelineDescription.Shader = CreateRef<VulkanShader>(shader);
    pipelineDescription.RenderPass = renderpass;
    pipelineDescription.RasterizerDesc.CullMode = vk::CullModeFlagBits::eNone;
    pipelineDescription.DepthSpec.DepthEnable = false;
    pipelineDescription.DepthSpec.DepthWriteEnable = false;
    pipelineDescription.DepthSpec.CompareOp = vk::CompareOp::eNever;
    pipelineDescription.VertexInputState.bindingDescriptions = {
      {0, vertexLayout.stride(), vk::VertexInputRate::eVertex},
    };
    pipelineDescription.VertexInputState.attributeDescriptions = {{0, 0, vk::Format::eR32G32B32Sfloat, 0},};
    pipeline.CreateGraphicsPipeline(pipelineDescription);

    // Render
    vk::ClearValue clearValues[1];
    clearValues[0].color = vk::ClearColorValue(std::array{0.0f, 0.0f, 0.2f, 0.0f});

    vk::RenderPassBeginInfo renderPassBeginInfo{
      renderpass.Get(), offscreen.framebuffer,
      vk::Rect2D{vk::Offset2D{}, vk::Extent2D{static_cast<uint32_t>(dim), static_cast<uint32_t>(dim)}}, 1, clearValues
    };

    const std::vector matrices = {
      // POSITIVE_X
      glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
        glm::radians(180.0f),
        glm::vec3(1.0f, 0.0f, 0.0f)),
      // NEGATIVE_X
      glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
        glm::radians(180.0f),
        glm::vec3(1.0f, 0.0f, 0.0f)),
      // POSITIVE_Y
      glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
      // NEGATIVE_Y
      glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
      // POSITIVE_Z
      glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
      // NEGATIVE_Z
      glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
    };

    VulkanRenderer::SubmitOnce([&](const VulkanCommandBuffer& cmdBuf) {
      vk::Viewport viewport{0, 0, static_cast<float>(dim), static_cast<float>(dim), 0.0f, 1.0f};
      vk::Rect2D scissor{vk::Offset2D{}, vk::Extent2D{static_cast<uint32_t>(dim), static_cast<uint32_t>(dim)}};

      cmdBuf.Get().setViewport(0, viewport);
      cmdBuf.Get().setScissor(0, scissor);

      vk::ImageSubresourceRange subresourceRange = {};
      subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
      subresourceRange.levelCount = numMips;
      subresourceRange.layerCount = 6;

      // Change image layout for all cubemap faces to transfer destination
      target.SetImageLayout(vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        subresourceRange,
        &cmdBuf);

      for (uint32_t m = 0; m < numMips; m++) {
        for (uint32_t f = 0; f < 6; f++) {
          viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
          viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
          cmdBuf.Get().setViewport(0, 1, &viewport);
          // Render scene from cube face's point of view
          cmdBuf.BeginRenderPass(renderPassBeginInfo);

          // Update shader push constant block
          pushBlock.mvp = glm::perspective(static_cast<float>((M_PI / 2.0)), 1.0f, 0.1f, 512.0f) * matrices[f];

          cmdBuf.Get().pushConstants<PushBlock>(pipelinelayout,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
            0,
            pushBlock);

          pipeline.BindPipeline(cmdBuf.Get());
          pipeline.BindDescriptorSets(cmdBuf.Get(), {descriptorset});

          std::vector<vk::DeviceSize> offsets{0};

          cmdBuf.Get().bindVertexBuffers(0, skybox.VerticiesBuffer.Get(), offsets);
          cmdBuf.Get().bindIndexBuffer(skybox.IndiciesBuffer.Get(), 0, vk::IndexType::eUint32);
          cmdBuf.Get().drawIndexed(skybox.IndexCount, 1, 0, 0, 0);

          cmdBuf.EndRenderPass();

          vk::ImageSubresourceRange subresourceRange;
          subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
          subresourceRange.levelCount = 1;
          subresourceRange.layerCount = 1;
          offscreen.image.SetImageLayout(vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::eTransferSrcOptimal,
            subresourceRange,
            &cmdBuf);

          // Copy region for transfer from framebuffer to cube face
          vk::ImageCopy copyRegion;
          copyRegion.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
          copyRegion.srcSubresource.baseArrayLayer = 0;
          copyRegion.srcSubresource.mipLevel = 0;
          copyRegion.srcSubresource.layerCount = 1;
          copyRegion.srcOffset = vk::Offset3D{};
          copyRegion.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
          copyRegion.dstSubresource.baseArrayLayer = f;
          copyRegion.dstSubresource.mipLevel = m;
          copyRegion.dstSubresource.layerCount = 1;
          copyRegion.dstOffset = vk::Offset3D{};
          copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
          copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
          copyRegion.extent.depth = 1;

          cmdBuf.Get().copyImage(offscreen.image.GetImage(),
            vk::ImageLayout::eTransferSrcOptimal,
            target.GetImage(),
            vk::ImageLayout::eTransferDstOptimal,
            copyRegion);

          offscreen.image.SetImageLayout(vk::ImageLayout::eTransferSrcOptimal,
            vk::ImageLayout::eColorAttachmentOptimal,
            subresourceRange,
            &cmdBuf);
        }
      }
      target.SetImageLayout(vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        subresourceRange,
        &cmdBuf);
    });

    renderpass.Destroy();
    LogicalDevice.destroyFramebuffer(offscreen.framebuffer, nullptr);
    offscreen.image.Destroy();
    LogicalDevice.destroyDescriptorPool(descriptorpool, nullptr);
    LogicalDevice.destroyDescriptorSetLayout(descriptorsetlayout, nullptr);
    pipeline.Destroy();
    LogicalDevice.destroyPipelineLayout(pipelinelayout, nullptr);
  }

  void Prefilter::GeneratePrefilteredCube(VulkanImage& target,
                                          const Mesh& skybox,
                                          const VertexLayout& vertexLayout,
                                          const vk::DescriptorImageInfo& skyboxDescriptor) {
    const auto& LogicalDevice = VulkanContext::Context.Device;

    //target.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    constexpr vk::Format format = vk::Format::eR16G16B16A16Sfloat;
    constexpr int32_t dim = 512;
    const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

    // Pre-filtered cube map
    // Image
    VulkanImageDescription imgDescription;
    imgDescription.Format = format;
    imgDescription.Height = dim;
    imgDescription.Width = dim;
    imgDescription.MipLevels = numMips;
    imgDescription.Type = ImageType::TYPE_CUBE;
    imgDescription.UsageFlags = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
    imgDescription.CreateView = true;
    imgDescription.CreateSampler = true;
    target.Create(imgDescription);

    VulkanRenderPass renderpass;

    // FB, Att, RP, Pipe, etc.
    vk::AttachmentDescription attDesc = {};
    // Color attachment
    attDesc.format = format;
    attDesc.loadOp = vk::AttachmentLoadOp::eClear;
    attDesc.storeOp = vk::AttachmentStoreOp::eStore;
    attDesc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attDesc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attDesc.initialLayout = vk::ImageLayout::eUndefined;
    attDesc.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
    vk::AttachmentReference colorReference{0, vk::ImageLayout::eColorAttachmentOptimal};

    vk::SubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;

    // Use subpass dependencies for layout transitions
    std::array<vk::SubpassDependency, 2> dependencies{
      vk::SubpassDependency{
        VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eBottomOfPipe,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eMemoryRead,
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
        vk::DependencyFlagBits::eByRegion
      },
      vk::SubpassDependency{
        0, VK_SUBPASS_EXTERNAL, vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eBottomOfPipe,
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
        vk::AccessFlagBits::eMemoryRead, vk::DependencyFlagBits::eByRegion
      },
    };

    // Renderpass
    vk::RenderPassCreateInfo renderPassCreateInfo;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &attDesc;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassCreateInfo.pDependencies = dependencies.data();
    renderpass.CreateRenderPass(renderPassCreateInfo);

    struct {
      VulkanImage image;
      vk::Framebuffer framebuffer;
    } offscreen;

    // Offfscreen framebuffer
    VulkanImageDescription imageDescription;
    imageDescription.Format = format;
    imageDescription.Width = dim;
    imageDescription.Height = dim;
    imageDescription.UsageFlags = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;
    imageDescription.CreateView = true;
    imageDescription.CreateSampler = false;
    imageDescription.TransitionLayoutAtCreate = false;
    offscreen.image.Create(imageDescription);

    vk::FramebufferCreateInfo fbufCreateInfo;
    fbufCreateInfo.renderPass = renderpass.Get();
    fbufCreateInfo.attachmentCount = 1;
    fbufCreateInfo.pAttachments = &offscreen.image.GetImageView();
    fbufCreateInfo.width = dim;
    fbufCreateInfo.height = dim;
    fbufCreateInfo.layers = 1;
    offscreen.framebuffer = LogicalDevice.createFramebuffer(fbufCreateInfo);
    vk::ImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 1;
    offscreen.image.SetImageLayout(vk::ImageLayout::eUndefined,
      vk::ImageLayout::eColorAttachmentOptimal,
      subresourceRange);

    // Descriptors
    vk::DescriptorSetLayoutBinding setLayoutBinding{
      0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment
    };
    vk::DescriptorSetLayout descriptorsetlayout = LogicalDevice.createDescriptorSetLayout({{}, 1, &setLayoutBinding});
    // Descriptor Pool
    vk::DescriptorPoolSize poolSize{vk::DescriptorType::eCombinedImageSampler, 1};
    vk::DescriptorPool descriptorpool = LogicalDevice.createDescriptorPool({{}, 2, 1, &poolSize});
    // Descriptor sets
    vk::DescriptorSet descriptorset = LogicalDevice.allocateDescriptorSets({descriptorpool, 1, &descriptorsetlayout})[
      0];
    vk::WriteDescriptorSet writeDescriptorSet{
      descriptorset, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &skyboxDescriptor
    };
    LogicalDevice.updateDescriptorSets(writeDescriptorSet, nullptr);

    // Pipeline layout
    struct PushBlock {
      glm::mat4 mvp;
      float roughness;
      uint32_t numSamples = 32u;
    } pushBlock;

    vk::PushConstantRange pushConstantRange{
      vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushBlock)
    };
    vk::PipelineLayout pipelinelayout = LogicalDevice.createPipelineLayout({
      {}, 1, &descriptorsetlayout, 1, &pushConstantRange
    });

    // Pipeline
    VulkanShader shader{
      ShaderCI{
        .VertexPath = "resources/shaders/FilterCube.vert", .FragmentPath = "resources/shaders/PrefilterEnvMap.frag",
        .EntryPoint = "main",
      }
    };
    PipelineDescription pipelineDescription;
    pipelineDescription.PushConstantRanges = {
      vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushBlock)}
    };
    pipelineDescription.DescriptorSetLayoutBindings = {
      {{0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}}
    };
    pipelineDescription.Shader = CreateRef<VulkanShader>(shader);
    pipelineDescription.RenderPass = renderpass;
    pipelineDescription.DepthSpec.DepthEnable = false;
    pipelineDescription.DepthSpec.DepthWriteEnable = false;
    pipelineDescription.DepthSpec.CompareOp = vk::CompareOp::eNever;
    pipelineDescription.RasterizerDesc.CullMode = vk::CullModeFlagBits::eNone;
    pipelineDescription.VertexInputState.bindingDescriptions = {
      {0, vertexLayout.stride(), vk::VertexInputRate::eVertex},
    };
    pipelineDescription.VertexInputState.attributeDescriptions = {{0, 0, vk::Format::eR32G32B32Sfloat, 0},};
    VulkanPipeline pipeline;
    pipeline.CreateGraphicsPipeline(pipelineDescription);

    // Render
    vk::ClearValue clearValues[1];
    clearValues[0].color = vk::ClearColorValue(std::array{0.0f, 0.0f, 0.2f, 0.0f});

    vk::RenderPassBeginInfo renderPassBeginInfo{
      renderpass.Get(), offscreen.framebuffer,
      {vk::Offset2D{}, vk::Extent2D{static_cast<uint32_t>(dim), static_cast<uint32_t>(dim)}}, 1, clearValues
    };
    // Reuse render pass from example pass

    const std::vector<glm::mat4> matrices = {
      // POSITIVE_X
      glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
        glm::radians(180.0f),
        glm::vec3(1.0f, 0.0f, 0.0f)),
      // NEGATIVE_X
      glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
        glm::radians(180.0f),
        glm::vec3(1.0f, 0.0f, 0.0f)),
      // POSITIVE_Y
      glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
      // NEGATIVE_Y
      glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
      // POSITIVE_Z
      glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
      // NEGATIVE_Z
      glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
    };

    VulkanRenderer::SubmitOnce([&](const VulkanCommandBuffer& cmdBuf) {
      vk::Viewport viewport{0, 0, static_cast<float>(dim), static_cast<float>(dim), 0, 1};
      vk::Rect2D scissor{vk::Offset2D{}, vk::Extent2D{static_cast<uint32_t>(dim), static_cast<uint32_t>(dim)}};
      cmdBuf.Get().setViewport(0, viewport);
      cmdBuf.Get().setScissor(0, scissor);
      const vk::ImageSubresourceRange subresourceRange{vk::ImageAspectFlagBits::eColor, 0, numMips, 0, 6};
      // Change image layout for all cubemap faces to transfer destination
      target.SetImageLayout(vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        subresourceRange,
        &cmdBuf);

      for (uint32_t m = 0; m < numMips; m++) {
        pushBlock.roughness = static_cast<float>(m) / static_cast<float>(numMips - 1);
        for (uint32_t f = 0; f < 6; f++) {
          viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
          viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
          cmdBuf.Get().setViewport(0, viewport);

          // Render scene from cube face's point of view
          cmdBuf.Get().beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

          // Update shader push constant block
          pushBlock.mvp = glm::perspective(static_cast<float>((M_PI / 2.0)), 1.0f, 0.1f, 512.0f) * matrices[f];

          cmdBuf.Get().pushConstants<PushBlock>(pipelinelayout,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
            0,
            pushBlock);
          cmdBuf.Get().bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.Get());
          cmdBuf.Get().bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelinelayout, 0, descriptorset, nullptr);

          std::vector<vk::DeviceSize> offsets{0};
          cmdBuf.Get().bindVertexBuffers(0, skybox.VerticiesBuffer.Get(), offsets);
          cmdBuf.Get().bindIndexBuffer(skybox.IndiciesBuffer.Get(), 0, vk::IndexType::eUint32);
          cmdBuf.Get().drawIndexed(skybox.IndexCount, 1, 0, 0, 0);

          cmdBuf.Get().endRenderPass();

          vk::ImageSubresourceRange subresourceRange1;
          subresourceRange1.aspectMask = vk::ImageAspectFlagBits::eColor;
          subresourceRange1.levelCount = 1;
          subresourceRange1.layerCount = 1;
          offscreen.image.SetImageLayout(vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::eTransferSrcOptimal,
            subresourceRange1,
            &cmdBuf);

          // Copy region for transfer from framebuffer to cube face
          vk::ImageCopy copyRegion;
          copyRegion.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
          copyRegion.srcSubresource.baseArrayLayer = 0;
          copyRegion.srcSubresource.mipLevel = 0;
          copyRegion.srcSubresource.layerCount = 1;
          copyRegion.srcOffset = vk::Offset3D{};

          copyRegion.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
          copyRegion.dstSubresource.baseArrayLayer = f;
          copyRegion.dstSubresource.mipLevel = m;
          copyRegion.dstSubresource.layerCount = 1;
          copyRegion.dstOffset = vk::Offset3D{};

          copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
          copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
          copyRegion.extent.depth = 1;

          cmdBuf.Get().copyImage(offscreen.image.GetImage(),
            vk::ImageLayout::eTransferSrcOptimal,
            target.GetImage(),
            vk::ImageLayout::eTransferDstOptimal,
            copyRegion);
          // Transform framebuffer color attachment back
          offscreen.image.SetImageLayout(vk::ImageLayout::eTransferSrcOptimal,
            vk::ImageLayout::eColorAttachmentOptimal,
            subresourceRange1,
            &cmdBuf);
        }
      }
      target.SetImageLayout(vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        subresourceRange,
        &cmdBuf);
    });

    // todo: cleanup
    renderpass.Destroy();
    LogicalDevice.destroyFramebuffer(offscreen.framebuffer, nullptr);
    offscreen.image.Destroy();
    LogicalDevice.destroyDescriptorPool(descriptorpool, nullptr);
    LogicalDevice.destroyDescriptorSetLayout(descriptorsetlayout, nullptr);
    pipeline.Destroy();
    LogicalDevice.destroyPipelineLayout(pipelinelayout, nullptr);
  }
}
