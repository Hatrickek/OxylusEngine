#pragma once
#include <vulkan/vulkan.hpp>

#include "VulkanRenderPass.h"
#include "VulkanShader.h"
#include "Render/Mesh.h"

namespace Oxylus {
#define MAX_RENDER_TARGETS 8

  struct VertexInputDescription {
    std::vector<vk::VertexInputBindingDescription> bindingDescriptions;
    std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
    VertexInputDescription() = default;

    VertexInputDescription(const VertexLayout& vertexLayout,
                           uint32_t binding = 0,
                           vk::VertexInputRate rate = vk::VertexInputRate::eVertex) {
      bindingDescriptions.emplace_back(binding, vertexLayout.stride(), rate);
      const auto componentsSize = vertexLayout.components.size();
      attributeDescriptions.reserve(attributeDescriptions.size() + componentsSize);
      const auto attributeIndexOffset = static_cast<uint32_t>(attributeDescriptions.size());
      for (uint32_t i = 0; i < componentsSize; ++i) {
        const auto& component = vertexLayout.components[i];
        const auto format = VertexLayout::ComponentFormat(component);
        const auto offset = vertexLayout.offset(i);
        attributeDescriptions.emplace_back(attributeIndexOffset + i, binding, format, offset);
      }
    }
  };

  struct StencilDescription {
    vk::CompareOp StencilFunc = vk::CompareOp::eLess;
    vk::StencilOp StencilFailOp = vk::StencilOp::eKeep;
    vk::StencilOp StencilPassOp = vk::StencilOp::eKeep;
    vk::StencilOp StencilDepthFailOp = vk::StencilOp::eKeep;
  };
  
  struct SubpassDescription {
    uint32_t SrcSubpass = VK_SUBPASS_EXTERNAL;
    uint32_t DstSubpass = 0;
    vk::PipelineStageFlags SrcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::PipelineStageFlags DstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::AccessFlags SrcAccessMask = vk::AccessFlagBits::eMemoryRead;
    vk::AccessFlags DstAccessMask = vk::AccessFlagBits::eColorAttachmentRead |
                                    vk::AccessFlagBits::eColorAttachmentWrite;
  };
  
  struct DepthStencilDescription {
    bool DepthEnable = true;
    bool DepthWriteEnable = true;
    bool StencilEnable = false;
    bool BoundTest = false;
    uint8_t StencilReadMask = 0x00;  //OxFF
    uint8_t StencilWriteMask = 0x00; //OxFF
    vk::CompareOp CompareOp = vk::CompareOp::eLess;
    StencilDescription FrontFace;
    StencilDescription BackFace;
    float MinDepthBound = 0;
    float MaxDepthBound = 1;
    vk::Format DepthStenctilFormat;
    uint32_t DepthReferenceAttachment = 0;
  };

  struct RasterizerDescription {
    vk::CullModeFlags CullMode = vk::CullModeFlagBits::eBack;
    bool ScissorEnable = false;
    bool DepthClamppEnable = false;
    vk::PolygonMode FillMode = vk::PolygonMode::eFill;
    bool FrontCounterClockwise = true;
    int DepthBias = 0;
    float SlopeScaledDepthBias = 0;
    float DepthBiasClamp = 0;
  };

  struct RenderTargetBlendDesc {
    bool BlendEnable = false;
    bool LogicOperationEnable = false;
    vk::BlendFactor SrcBlend = vk::BlendFactor::eOne;
    vk::BlendFactor DestBlend = vk::BlendFactor::eZero;
    vk::BlendOp BlendOp = vk::BlendOp::eAdd;
    vk::BlendFactor SrcBlendAlpha = vk::BlendFactor::eOne;
    vk::BlendFactor DestBlendAlpha = vk::BlendFactor::eZero;
    vk::BlendOp BlendOpAlpha = vk::BlendOp::eAdd;
    vk::LogicOp LogicOp = vk::LogicOp::eClear;
    vk::Flags<vk::ColorComponentFlagBits> WriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  };

  struct BlendStateDescription {
    bool AlphaToCoverageEnable = false;
    bool IndependentBlendEnable = false;
    RenderTargetBlendDesc RenderTargets[MAX_RENDER_TARGETS];
  };

  enum class PipelineType {
    Graphics,
    Mesh,
  };

  struct RenderTargetDescription {
    vk::Format Format = vk::Format::eB8G8R8A8Unorm;
    vk::ImageLayout InitialLayout = vk::ImageLayout::eUndefined;
    vk::ImageLayout FinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    vk::AttachmentLoadOp LoadOp = vk::AttachmentLoadOp::eClear;
    vk::AttachmentStoreOp StoreOp = vk::AttachmentStoreOp::eStore;
  };

  struct PipelineDescription {
    RenderTargetDescription RenderTargets[MAX_RENDER_TARGETS];
    bool DepthAttachmentFirst = false;
    uint32_t ColorAttachment = 0;
    vk::ImageLayout ColorAttachmentLayout = vk::ImageLayout::eColorAttachmentOptimal;
    vk::ImageLayout DepthAttachmentLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    SubpassDescription SubpassDescription[4];
    uint32_t SubpassDependencyCount = 1;
    Ref<VulkanShader> Shader;
    DepthStencilDescription DepthSpec;
    uint8_t ColorAttachmentCount = 1;
    RasterizerDescription RasterizerDesc;
    VertexInputDescription VertexInputState;
    vk::PrimitiveTopology PrimitiveTopology = vk::PrimitiveTopology::eTriangleList;
    vk::SampleCountFlagBits Samples = vk::SampleCountFlagBits::e1;
    PipelineType Type = PipelineType::Graphics;
    uint8_t NumViewports = 1;
    uint8_t SubpassIndex = 0;
    uint8_t SampleCount = 1;
    uint32_t SampleMask = 0xFFFFFFFF;
    BlendStateDescription BlendStateDesc;
    std::vector<vk::DynamicState> DynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    std::vector<std::vector<vk::DescriptorSetLayoutBinding>> DescriptorSetLayoutBindings;
    std::vector<vk::PushConstantRange> PushConstantRanges;
    VulkanRenderPass RenderPass;
  };

  class VulkanPipeline {
  public:
    VulkanPipeline() = default;

    ~VulkanPipeline() = default;

    void CreateGraphicsPipeline(PipelineDescription& pipelineSpecification);

    void CreateComputePipeline(const PipelineDescription& pipelineSpecification);

    void BindDescriptorSets(const vk::CommandBuffer& commandBuffer,
                            const std::vector<vk::DescriptorSet>& descriptorSets,
                            uint32_t firstSet = 0,
                            uint32_t setCount = 1,
                            uint32_t dynamicOffsetCount = 0,
                            const uint32_t* pDynamicOffsets = nullptr) const;

    void BindPipeline(const vk::CommandBuffer& cmdBuffer) const;

    void Destroy() const;

    const vk::Pipeline& Get() const {
      return m_Pipeline;
    }

    const PipelineDescription& GetDesc() const {
      return m_PipelineDescription;
    }

    const VulkanRenderPass& GetRenderPass() const {
      return m_RenderPass;
    }

    const vk::PipelineLayout& GetPipelineLayout() const {
      return m_Layout;
    }

    const std::vector<vk::DescriptorSetLayout>& GetDescriptorSetLayout() const {
      return m_DescriptorSetLayouts;
    }

  private:
    vk::Pipeline m_Pipeline;
    vk::PipelineLayout m_Layout;
    std::vector<vk::DescriptorSetLayout> m_DescriptorSetLayouts;
    static vk::PipelineCache s_Cache;
    VulkanRenderPass m_RenderPass;
    PipelineDescription m_PipelineDescription;
    bool m_IsCompute = false;
    void CreateColorAttachments(const PipelineDescription& pipelineSpecification,
                                std::vector<vk::AttachmentDescription>& AttachmentDescriptions) const;
    void CreateDepthAttachments(const PipelineDescription& pipelineSpecification,
                                std::vector<vk::AttachmentDescription>& attachmentDescriptions) const;
  };
}
