#include "src/oxpch.h"
#include "VulkanPipeline.h"

#include <future>
#include <type_traits>
#include <type_traits>
#include <type_traits>
#include <type_traits>

#include "Render/ShaderLibrary.h"
#include "VulkanContext.h"
#include "Utils/VulkanUtils.h"

namespace Oxylus {
  vk::PipelineCache VulkanPipeline::s_Cache;

  void VulkanPipeline::CreateGraphicsPipeline(PipelineDescription& pipelineSpecification) {
    const auto& LogicalDevice = VulkanContext::Context.Device;

    m_PipelineDescription = pipelineSpecification;

    m_Shader = pipelineSpecification.Shader;

    std::vector<vk::AttachmentDescription> attachmentDescriptions;

    CreateColorAttachments(pipelineSpecification, attachmentDescriptions);
    CreateDepthAttachments(pipelineSpecification, attachmentDescriptions);

    std::vector<vk::SubpassDescription> subpasses;

    vk::AttachmentReference colorAttachmentReference;
    colorAttachmentReference.attachment = m_PipelineDescription.ColorAttachment;
    colorAttachmentReference.layout = m_PipelineDescription.ColorAttachmentLayout;

    vk::AttachmentReference depthAttachmentReference;
    depthAttachmentReference.attachment = m_PipelineDescription.DepthDesc.DepthReferenceAttachment;
    depthAttachmentReference.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::SubpassDescription subpassDescription;
    subpassDescription.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpassDescription.colorAttachmentCount = pipelineSpecification.ColorAttachmentCount;
    if (pipelineSpecification.ColorAttachmentCount > 0)
      subpassDescription.pColorAttachments = &colorAttachmentReference;
    if (pipelineSpecification.DepthDesc.DepthEnable)
      subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;

    // Input from a shader
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;

    // Attachments used for multisampling colour attachments
    subpassDescription.pResolveAttachments = nullptr;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = 0;

    subpasses.emplace_back(subpassDescription);

    std::vector<vk::SubpassDependency> dependencies;

    for (uint32_t i = 0; i < m_PipelineDescription.SubpassDependencyCount; ++i) {
      vk::SubpassDependency subpassDependency;
      subpassDependency.srcSubpass = m_PipelineDescription.SubpassDescription[i].SrcSubpass;
      subpassDependency.dstSubpass = m_PipelineDescription.SubpassDescription[i].DstSubpass;
      subpassDependency.srcStageMask = m_PipelineDescription.SubpassDescription[i].SrcStageMask;
      subpassDependency.dstStageMask = m_PipelineDescription.SubpassDescription[i].DstStageMask;
      subpassDependency.srcAccessMask = m_PipelineDescription.SubpassDescription[i].SrcAccessMask;
      subpassDependency.dstAccessMask = m_PipelineDescription.SubpassDescription[i].DstAccessMask;
      dependencies.emplace_back(subpassDependency);
    }

    if (!GetDesc().RenderPass.Get()) {
      vk::RenderPassCreateInfo renderPassCI;
      renderPassCI.pAttachments = attachmentDescriptions.data();
      renderPassCI.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
      renderPassCI.pSubpasses = subpasses.data();
      renderPassCI.subpassCount = static_cast<uint32_t>(subpasses.size());
      renderPassCI.pDependencies = dependencies.data();
      renderPassCI.dependencyCount = static_cast<uint32_t>(dependencies.size());

      RenderPassDescription description = {};
      description.Name = m_PipelineDescription.Name.empty() ? "Pipeline" : m_PipelineDescription.Name;
      description.CreateInfo = renderPassCI;
      m_RenderPass.CreateRenderPass(description);
    }
    else {
      m_RenderPass = GetDesc().RenderPass;
    }

    m_Layout = m_Shader->GetPipelineLayout();
    m_DescriptorSetLayouts = m_Shader->GetDescriptorSetLayouts();

    vk::GraphicsPipelineCreateInfo PipelineCI;
    PipelineCI.stageCount = pipelineSpecification.Shader->GetStageCount();
    PipelineCI.pStages = pipelineSpecification.Shader->GetShaderStages().data();
    PipelineCI.renderPass = m_RenderPass.Get();
    PipelineCI.layout = m_Layout;

    vk::PipelineVertexInputStateCreateInfo vertexInputStateCI;
    vertexInputStateCI.pVertexAttributeDescriptions = m_PipelineDescription.VertexInputState.attributeDescriptions.data();
    vertexInputStateCI.pVertexBindingDescriptions = m_PipelineDescription.VertexInputState.bindingDescriptions.data();
    vertexInputStateCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_PipelineDescription.VertexInputState.attributeDescriptions.size());
    vertexInputStateCI.vertexBindingDescriptionCount = static_cast<uint32_t>(m_PipelineDescription.VertexInputState.bindingDescriptions.size());
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
    RasterizerStateCI.depthClampEnable = pipelineSpecification.RasterizerDesc.DepthClampEnable ? VK_TRUE : VK_FALSE;
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
    RasterizerStateCI.lineWidth = pipelineSpecification.RasterizerDesc.LineWidth;

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
    depthStencilStateCI.depthTestEnable = pipelineSpecification.DepthDesc.DepthEnable ? VK_TRUE : VK_FALSE;
    depthStencilStateCI.depthWriteEnable = pipelineSpecification.DepthDesc.DepthWriteEnable ? VK_TRUE : VK_FALSE;
    depthStencilStateCI.depthCompareOp = pipelineSpecification.DepthDesc.CompareOp;
    depthStencilStateCI.depthBoundsTestEnable = pipelineSpecification.DepthDesc.BoundTest ? VK_TRUE : VK_FALSE;
    depthStencilStateCI.stencilTestEnable = pipelineSpecification.DepthDesc.StencilEnable ? VK_TRUE : VK_FALSE;
    vk::StencilOpState FrontStencilState = {};
    FrontStencilState.failOp = pipelineSpecification.DepthDesc.FrontFace.StencilFailOp;
    FrontStencilState.passOp = pipelineSpecification.DepthDesc.FrontFace.StencilPassOp;
    FrontStencilState.depthFailOp = pipelineSpecification.DepthDesc.FrontFace.StencilDepthFailOp;
    FrontStencilState.compareOp = pipelineSpecification.DepthDesc.FrontFace.StencilFunc;
    FrontStencilState.compareMask = pipelineSpecification.DepthDesc.StencilReadMask;
    FrontStencilState.writeMask = pipelineSpecification.DepthDesc.StencilWriteMask;
    FrontStencilState.reference = 0; // Set dynamically
    vk::StencilOpState BackStencilState = {};
    BackStencilState.failOp = pipelineSpecification.DepthDesc.BackFace.StencilFailOp;
    BackStencilState.passOp = pipelineSpecification.DepthDesc.BackFace.StencilPassOp;
    BackStencilState.depthFailOp = pipelineSpecification.DepthDesc.BackFace.StencilDepthFailOp;
    BackStencilState.compareOp = pipelineSpecification.DepthDesc.BackFace.StencilFunc;
    BackStencilState.compareMask = pipelineSpecification.DepthDesc.StencilReadMask;
    BackStencilState.writeMask = pipelineSpecification.DepthDesc.StencilWriteMask;
    BackStencilState.reference = 0; // Set dynamically
    depthStencilStateCI.front = FrontStencilState;
    depthStencilStateCI.back = BackStencilState;
    depthStencilStateCI.minDepthBounds = pipelineSpecification.DepthDesc.MinDepthBound;
    depthStencilStateCI.maxDepthBounds = pipelineSpecification.DepthDesc.MaxDepthBound;
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

    if (!pipelineSpecification.Name.empty())
      VulkanUtils::SetObjectName((uint64_t)(VkPipeline)m_Pipeline, vk::ObjectType::ePipeline, pipelineSpecification.Name.c_str());

    //Connect pipeline with shader
    pipelineSpecification.Shader->OnReloadBegin([this] {
      Destroy();
    });
    pipelineSpecification.Shader->OnReloadEnd([this] {
      CreateGraphicsPipeline(m_PipelineDescription);
    });
  }

  std::future<void> VulkanPipeline::CreateGraphicsPipelineAsync(PipelineDescription& pipelineSpecification) {
    return std::async([this, &pipelineSpecification] {
      CreateGraphicsPipeline(pipelineSpecification);
    });
  }

  void VulkanPipeline::CreateDepthAttachments(const PipelineDescription& pipelineSpecification,
                                              std::vector<vk::AttachmentDescription>& attachmentDescriptions) const {
    if (pipelineSpecification.DepthDesc.DepthEnable) {
      vk::AttachmentDescription DepthAttachment = {
        vk::AttachmentDescriptionFlags(), pipelineSpecification.DepthDesc.DepthStenctilFormat,
        pipelineSpecification.Samples, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined,
        pipelineSpecification.DepthAttachmentLayout
      };
      attachmentDescriptions.emplace_back(DepthAttachment);
    }
  }

  void VulkanPipeline::CreateColorAttachments(const PipelineDescription& pipelineSpecification,
                                              std::vector<vk::AttachmentDescription>& AttachmentDescriptions) const {
    for (int i = 0; i < pipelineSpecification.ColorAttachmentCount; i++) {
      vk::AttachmentDescription ColorAttachment = {
        vk::AttachmentDescriptionFlags(), pipelineSpecification.RenderTargets[i].Format, pipelineSpecification.Samples,
        pipelineSpecification.RenderTargets[i].LoadOp, pipelineSpecification.RenderTargets[i].StoreOp,
        vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
        pipelineSpecification.RenderTargets[i].InitialLayout, pipelineSpecification.RenderTargets[i].FinalLayout
      };
      AttachmentDescriptions.emplace_back(ColorAttachment);
    }
  }

  void VulkanPipeline::CreateComputePipeline(const PipelineDescription& pipelineSpecification) {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    m_PipelineDescription = pipelineSpecification;
    m_Shader = pipelineSpecification.Shader;

    vk::ComputePipelineCreateInfo PipelineCI;
    PipelineCI.stage = pipelineSpecification.Shader->GetComputeShaderStage();

    m_Layout = m_Shader->GetPipelineLayout();
    m_DescriptorSetLayouts = m_Shader->GetDescriptorSetLayouts();

    PipelineCI.layout = m_Layout;
    const auto res = LogicalDevice.createComputePipeline(s_Cache, PipelineCI);
    VulkanUtils::CheckResult(res.result);
    m_Pipeline = res.value;
    m_IsCompute = true;

    // Connect pipeline with shader
    pipelineSpecification.Shader->OnReloadBegin([this] {
      Destroy();
    });
    pipelineSpecification.Shader->OnReloadEnd([this] {
      CreateComputePipeline(m_PipelineDescription);
    });
  }

  std::future<void> VulkanPipeline::CreateComputePipelineAsync(const PipelineDescription& pipelineSpecification) {
    return std::async([this, &pipelineSpecification] {
      CreateComputePipeline(pipelineSpecification);
    });
  }

  void VulkanPipeline::BindDescriptorSets(const vk::CommandBuffer& commandBuffer,
                                          const std::vector<vk::DescriptorSet>& descriptorSets,
                                          uint32_t firstSet,
                                          uint32_t dynamicOffsetCount,
                                          const uint32_t* pDynamicOffsets) const {
    const auto bindPoint = m_IsCompute ? vk::PipelineBindPoint::eCompute : vk::PipelineBindPoint::eGraphics;
    commandBuffer.bindDescriptorSets(bindPoint,
      m_Layout,
      firstSet,
      (uint32_t)descriptorSets.size(),
      descriptorSets.data(),
      dynamicOffsetCount,
      pDynamicOffsets);
  }

  void VulkanPipeline::BindPipeline(const vk::CommandBuffer& cmdBuffer) const {
    OX_CORE_ASSERT(m_Pipeline);
    const auto bindPoint = m_IsCompute ? vk::PipelineBindPoint::eCompute : vk::PipelineBindPoint::eGraphics;
    cmdBuffer.bindPipeline(bindPoint, m_Pipeline);
  }

  void VulkanPipeline::Destroy() const {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    LogicalDevice.destroyPipeline(Get());
  }
}
