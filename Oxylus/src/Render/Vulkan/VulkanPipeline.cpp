﻿#include "src/oxpch.h"
#include "VulkanPipeline.h"

#include "VulkanContext.h"
#include "VulkanRenderer.h"
#include "Utils/VulkanUtils.h"

namespace Oxylus {
  vk::PipelineCache VulkanPipeline::s_Cache;

  static std::pair<vk::PipelineLayout, std::vector<vk::DescriptorSetLayout>> CreatePipelineLayout(
    const PipelineDescription& pipDesc) {
    const auto& LogicalDevice = VulkanContext::Context.Device;

    vk::PipelineLayoutCreateInfo pipelineLayoutCI;

    std::vector<vk::DescriptorSetLayout> descriptorSetLayouts;

    for (auto& bindings : pipDesc.DescriptorSetLayoutBindings) {
      vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
      descriptorSetLayoutCreateInfo.pBindings = bindings.data();
      descriptorSetLayoutCreateInfo.bindingCount = (uint32_t)bindings.size();

      descriptorSetLayouts.emplace_back(
        LogicalDevice.createDescriptorSetLayout(descriptorSetLayoutCreateInfo, nullptr).value);
    }

    pipelineLayoutCI.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutCI.setLayoutCount = (uint32_t)descriptorSetLayouts.size();

    if (!pipDesc.PushConstantRanges.empty()) {
      pipelineLayoutCI.pushConstantRangeCount = (uint32_t)pipDesc.PushConstantRanges.size();
      pipelineLayoutCI.pPushConstantRanges = pipDesc.PushConstantRanges.data();
    }
    return {LogicalDevice.createPipelineLayout(pipelineLayoutCI, nullptr).value, descriptorSetLayouts};
  }

  void VulkanPipeline::CreateGraphicsPipeline(PipelineDescription& pipelineSpecification) {
    const auto& LogicalDevice = VulkanContext::Context.Device;

    m_PipelineDescription = pipelineSpecification;

    std::vector<vk::AttachmentDescription> AttachmentDescriptions;

    for (int i = 0; i < pipelineSpecification.ColorAttachmentCount; i++) {
      vk::AttachmentDescription ColorAttachment = {
        vk::AttachmentDescriptionFlags(), pipelineSpecification.RenderTargets[i].Format, pipelineSpecification.Samples,
        pipelineSpecification.RenderTargets[i].LoadOp, pipelineSpecification.RenderTargets[i].StoreOp,
        vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
        pipelineSpecification.RenderTargets[i].InitialLayout, pipelineSpecification.RenderTargets[i].FinalLayout
      };
      AttachmentDescriptions.emplace_back(ColorAttachment);
    }

    if (pipelineSpecification.DepthSpec.DepthEnable) {
      vk::AttachmentDescription DepthAttachment = {
        vk::AttachmentDescriptionFlags(), pipelineSpecification.DepthSpec.DepthStenctilFormat,
        pipelineSpecification.Samples, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare,
        vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthStencilAttachmentOptimal
      };
      AttachmentDescriptions.emplace_back(DepthAttachment);
    }

    std::vector<vk::SubpassDescription> subpasses;

    vk::AttachmentReference ColorAttachmentReference;
    ColorAttachmentReference.attachment = 0;
    ColorAttachmentReference.layout = m_PipelineDescription.ColorAttachmentLayout;

    vk::AttachmentReference DepthAttachmentReference;
    DepthAttachmentReference.attachment = 1;
    DepthAttachmentReference.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::SubpassDescription subpassDescription;
    subpassDescription.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpassDescription.colorAttachmentCount = pipelineSpecification.ColorAttachmentCount;
    if (pipelineSpecification.ColorAttachmentCount > 0)
      subpassDescription.pColorAttachments = &ColorAttachmentReference;
    if (pipelineSpecification.DepthSpec.DepthEnable)
      subpassDescription.pDepthStencilAttachment = &DepthAttachmentReference;

    // Input from a shader
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;

    // Attachments used for multisampling colour attachments
    subpassDescription.pResolveAttachments = nullptr;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = 0;

    subpasses.emplace_back(subpassDescription);

    std::vector<vk::SubpassDependency> dependencies;

    vk::SubpassDependency subpassDependency;
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    subpassDependency.srcAccessMask = vk::AccessFlagBits::eMemoryRead;
    subpassDependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    subpassDependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead |
                                      vk::AccessFlagBits::eColorAttachmentWrite;

    dependencies.emplace_back(subpassDependency);

    if (!GetDesc().RenderPass.Get()) {
      vk::RenderPassCreateInfo renderPassCI;
      renderPassCI.pAttachments = AttachmentDescriptions.data();
      renderPassCI.attachmentCount = static_cast<uint32_t>(AttachmentDescriptions.size());
      renderPassCI.pSubpasses = subpasses.data();
      renderPassCI.subpassCount = static_cast<uint32_t>(subpasses.size());
      renderPassCI.pDependencies = dependencies.data();
      renderPassCI.dependencyCount = static_cast<uint32_t>(dependencies.size());

      m_RenderPass.CreateRenderPass(renderPassCI);
    }
    else {
      m_RenderPass = GetDesc().RenderPass;
    }
    if (static_cast<uint32_t>(pipelineSpecification.PushConstantRanges.size()) > 0) {
      if (static_cast<uint32_t>(pipelineSpecification.PushConstantRanges.size()) > 32) {
        OX_CORE_ERROR("CreateGraphicsPipeline(): Cannot have more than 32 push constant ranges. Passed count: {}",
          static_cast<uint32_t>(pipelineSpecification.PushConstantRanges.size()));
      }
    }

    auto [fst, snd] = CreatePipelineLayout(m_PipelineDescription);
    m_Layout = fst;
    m_DescriptorSetLayouts = snd;

    vk::GraphicsPipelineCreateInfo PipelineCI;
    PipelineCI.stageCount = pipelineSpecification.Shader->GetStageCount();
    PipelineCI.pStages = pipelineSpecification.Shader->GetShaderStages().data();
    PipelineCI.renderPass = m_RenderPass.Get();
    PipelineCI.layout = m_Layout;

    vk::PipelineVertexInputStateCreateInfo vertexInputStateCI;
    vertexInputStateCI.pVertexAttributeDescriptions = m_PipelineDescription.VertexInputState.attributeDescriptions.
                                                                            data();
    vertexInputStateCI.pVertexBindingDescriptions = m_PipelineDescription.VertexInputState.bindingDescriptions.data();
    vertexInputStateCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_PipelineDescription.VertexInputState.
      attributeDescriptions.size());
    vertexInputStateCI.vertexBindingDescriptionCount = static_cast<uint32_t>(m_PipelineDescription.VertexInputState.
      bindingDescriptions.size());
    PipelineCI.pVertexInputState = &vertexInputStateCI;

    vk::PipelineInputAssemblyStateCreateInfo InputAssemblyCI;
    InputAssemblyCI.primitiveRestartEnable = VK_FALSE;

    vk::PipelineTessellationStateCreateInfo TessStateCI{};

    if (pipelineSpecification.Type == PipelineType::Mesh) {
      InputAssemblyCI.topology = vk::PrimitiveTopology::eTriangleList;
      PipelineCI.pTessellationState = nullptr;
    }
    else {
      InputAssemblyCI.topology = pipelineSpecification.PrimitiveTopology;
      TessStateCI.patchControlPoints = 0;
    }
    PipelineCI.pInputAssemblyState = &InputAssemblyCI;
    PipelineCI.pTessellationState = &TessStateCI;

    vk::PipelineViewportStateCreateInfo ViewPortStateCI{};
    ViewPortStateCI.viewportCount = pipelineSpecification.NumViewports;
    ViewPortStateCI.pViewports = nullptr;
    ViewPortStateCI.scissorCount = ViewPortStateCI.viewportCount;
    vk::Rect2D ScissorRect = {};
    if (pipelineSpecification.RasterizerDesc.ScissorEnable) {
      ViewPortStateCI.pScissors = nullptr; // Ignored if the scissor state is dynamic
    }
    else {
      const auto& Props = VulkanContext::Context.DeviceProperties;
      ScissorRect.extent.width = Props.limits.maxViewportDimensions[0];
      ScissorRect.extent.height = Props.limits.maxViewportDimensions[1];
      ViewPortStateCI.pScissors = &ScissorRect;
    }
    PipelineCI.pViewportState = &ViewPortStateCI;

    vk::PipelineRasterizationStateCreateInfo RasterizerStateCI;
    RasterizerStateCI.depthClampEnable = pipelineSpecification.RasterizerDesc.DepthClamppEnable ? VK_TRUE : VK_FALSE;
    RasterizerStateCI.rasterizerDiscardEnable = VK_FALSE;
    RasterizerStateCI.polygonMode = pipelineSpecification.RasterizerDesc.FillMode;
    RasterizerStateCI.cullMode = pipelineSpecification.RasterizerDesc.CullMode;
    RasterizerStateCI.frontFace = pipelineSpecification.RasterizerDesc.FrontCounterClockwise
                                    ? vk::FrontFace::eCounterClockwise
                                    : vk::FrontFace::eClockwise;
    RasterizerStateCI.depthBiasEnable = (pipelineSpecification.RasterizerDesc.DepthBias != 0 || pipelineSpecification.
                                         RasterizerDesc.SlopeScaledDepthBias != 0.f)
                                          ? VK_TRUE
                                          : VK_FALSE;
    RasterizerStateCI.depthBiasConstantFactor = static_cast<float>(pipelineSpecification.RasterizerDesc.DepthBias);
    RasterizerStateCI.depthBiasClamp = pipelineSpecification.RasterizerDesc.DepthBiasClamp;
    RasterizerStateCI.depthBiasSlopeFactor = pipelineSpecification.RasterizerDesc.SlopeScaledDepthBias;
    RasterizerStateCI.lineWidth = 1.f;

    PipelineCI.pRasterizationState = &RasterizerStateCI;

    vk::PipelineMultisampleStateCreateInfo MSStateCI{};
    MSStateCI.rasterizationSamples = static_cast<vk::SampleCountFlagBits>(pipelineSpecification.SampleCount);
    MSStateCI.sampleShadingEnable = VK_FALSE;
    MSStateCI.minSampleShading = 0;
    uint32_t SampleMask[] = {pipelineSpecification.SampleMask, 0};
    MSStateCI.pSampleMask = SampleMask;
    MSStateCI.alphaToCoverageEnable = VK_FALSE;
    MSStateCI.alphaToOneEnable = VK_FALSE;
    PipelineCI.pMultisampleState = &MSStateCI;

    vk::PipelineDepthStencilStateCreateInfo depthStencilStateCI;
    depthStencilStateCI.depthTestEnable = pipelineSpecification.DepthSpec.DepthEnable ? VK_TRUE : VK_FALSE;
    depthStencilStateCI.depthWriteEnable = pipelineSpecification.DepthSpec.DepthWriteEnable ? VK_TRUE : VK_FALSE;
    depthStencilStateCI.depthCompareOp = pipelineSpecification.DepthSpec.CompareOp;
    depthStencilStateCI.depthBoundsTestEnable = pipelineSpecification.DepthSpec.BoundTest ? VK_TRUE : VK_FALSE;
    depthStencilStateCI.stencilTestEnable = pipelineSpecification.DepthSpec.StencilEnable ? VK_TRUE : VK_FALSE;
    vk::StencilOpState FrontStencilState = {};
    FrontStencilState.failOp = pipelineSpecification.DepthSpec.FrontFace.StencilFailOp;
    FrontStencilState.passOp = pipelineSpecification.DepthSpec.FrontFace.StencilPassOp;
    FrontStencilState.depthFailOp = pipelineSpecification.DepthSpec.FrontFace.StencilDepthFailOp;
    FrontStencilState.compareOp = pipelineSpecification.DepthSpec.FrontFace.StencilFunc;
    FrontStencilState.compareMask = pipelineSpecification.DepthSpec.StencilReadMask;
    FrontStencilState.writeMask = pipelineSpecification.DepthSpec.StencilWriteMask;
    FrontStencilState.reference = 0; // Set dynamically
    vk::StencilOpState BackStencilState = {};
    BackStencilState.failOp = pipelineSpecification.DepthSpec.BackFace.StencilFailOp;
    BackStencilState.passOp = pipelineSpecification.DepthSpec.BackFace.StencilPassOp;
    BackStencilState.depthFailOp = pipelineSpecification.DepthSpec.BackFace.StencilDepthFailOp;
    BackStencilState.compareOp = pipelineSpecification.DepthSpec.BackFace.StencilFunc;
    BackStencilState.compareMask = pipelineSpecification.DepthSpec.StencilReadMask;
    BackStencilState.writeMask = pipelineSpecification.DepthSpec.StencilWriteMask;
    BackStencilState.reference = 0; // Set dynamically
    depthStencilStateCI.front = FrontStencilState;
    depthStencilStateCI.back = BackStencilState;
    depthStencilStateCI.minDepthBounds = pipelineSpecification.DepthSpec.MinDepthBound;
    depthStencilStateCI.maxDepthBounds = pipelineSpecification.DepthSpec.MaxDepthBound;
    PipelineCI.pDepthStencilState = &depthStencilStateCI;

    std::vector<vk::PipelineColorBlendAttachmentState> ColorBlendAttachmentStates(1);

    vk::PipelineColorBlendStateCreateInfo BlendStateCI;
    BlendStateCI.pAttachments = !ColorBlendAttachmentStates.empty() ? ColorBlendAttachmentStates.data() : nullptr;
    BlendStateCI.attachmentCount = static_cast<uint32_t>(ColorBlendAttachmentStates.size());
    BlendStateCI.logicOp = pipelineSpecification.BlendStateDesc.RenderTargets[0].LogicOp;
    BlendStateCI.logicOpEnable = pipelineSpecification.BlendStateDesc.RenderTargets[0].LogicOperationEnable
                                   ? VK_TRUE
                                   : VK_FALSE;
    BlendStateCI.blendConstants[0] = 0.f;
    BlendStateCI.blendConstants[1] = 0.f;
    BlendStateCI.blendConstants[2] = 0.f;
    BlendStateCI.blendConstants[3] = 0.f;
    for (uint32_t attachment = 0; attachment < BlendStateCI.attachmentCount; ++attachment) {
      const auto& RTBlendState = pipelineSpecification.BlendStateDesc.IndependentBlendEnable
                                   ? pipelineSpecification.BlendStateDesc.RenderTargets[attachment]
                                   : pipelineSpecification.BlendStateDesc.RenderTargets[0];
      vk::PipelineColorBlendAttachmentState attachmentState;
      attachmentState.blendEnable = RTBlendState.BlendEnable;
      attachmentState.srcColorBlendFactor = RTBlendState.SrcBlend;
      attachmentState.dstColorBlendFactor = RTBlendState.DestBlend;
      attachmentState.colorBlendOp = RTBlendState.BlendOp;
      attachmentState.srcAlphaBlendFactor = RTBlendState.SrcBlendAlpha;
      attachmentState.dstAlphaBlendFactor = RTBlendState.DestBlendAlpha;
      attachmentState.alphaBlendOp = RTBlendState.BlendOpAlpha;
      attachmentState.colorWriteMask = RTBlendState.WriteMask;
      ColorBlendAttachmentStates[attachment] = attachmentState;
    }
    PipelineCI.pColorBlendState = &BlendStateCI;

    vk::PipelineDynamicStateCreateInfo DynamicStateCI;
    DynamicStateCI.pDynamicStates = pipelineSpecification.DynamicStates.data();
    DynamicStateCI.dynamicStateCount = static_cast<uint32_t>(pipelineSpecification.DynamicStates.size());

    PipelineCI.pDynamicState = &DynamicStateCI;
    PipelineCI.subpass = pipelineSpecification.SubpassIndex;
    PipelineCI.basePipelineHandle = VK_NULL_HANDLE;
    PipelineCI.basePipelineIndex = -1;

    const auto res = LogicalDevice.createGraphicsPipeline(s_Cache, PipelineCI, nullptr);
    VulkanUtils::CheckResult(res.result);
    m_Pipeline = res.value;

    //Connect pipeline with shader
    pipelineSpecification.Shader->OnReloadBegin([this] {
      Destroy();
    });
    pipelineSpecification.Shader->OnReloadEnd([this] {
      CreateGraphicsPipeline(m_PipelineDescription);
    });

    VulkanRenderer::AddShader(pipelineSpecification.Shader);
  }

  void VulkanPipeline::CreateComputePipeline(const PipelineDescription& pipelineSpecification) {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    m_PipelineDescription = pipelineSpecification;

    vk::ComputePipelineCreateInfo PipelineCI;
    PipelineCI.stage = pipelineSpecification.Shader->GetComputeShaderStage();

    auto [fst, scnd] = CreatePipelineLayout(pipelineSpecification);
    m_Layout = fst;
    m_DescriptorSetLayouts = scnd;

    PipelineCI.layout = m_Layout;
    const auto res = LogicalDevice.createComputePipeline(s_Cache, PipelineCI);
    VulkanUtils::CheckResult(res.result);
    m_Pipeline = res.value;
    m_IsCompute = true;

    //Connect pipeline with shader
    pipelineSpecification.Shader->OnReloadBegin([this] {
      Destroy();
    });
    pipelineSpecification.Shader->OnReloadEnd([this] {
      CreateComputePipeline(m_PipelineDescription);
    });

    VulkanRenderer::AddShader(pipelineSpecification.Shader);
  }

  void VulkanPipeline::BindDescriptorSets(const vk::CommandBuffer& commandBuffer,
                                          const std::vector<vk::DescriptorSet>& descriptorSets,
                                          uint32_t firstSet,
                                          uint32_t setCount,
                                          uint32_t dynamicOffsetCount,
                                          const uint32_t* pDynamicOffsets) const {
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
      m_Layout,
      firstSet,
      setCount,
      descriptorSets.data(),
      dynamicOffsetCount,
      pDynamicOffsets);
  }

  void VulkanPipeline::BindPipeline(const vk::CommandBuffer& cmdBuffer) const {
    if (m_IsCompute)
      cmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, m_Pipeline);
    else
      cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Pipeline);
  }

  void VulkanPipeline::Destroy() const {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    LogicalDevice.destroyPipeline(Get());
  }
}