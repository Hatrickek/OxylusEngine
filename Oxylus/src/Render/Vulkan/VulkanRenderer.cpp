#include "src/oxpch.h"
#include "VulkanRenderer.h"

#include "VulkanCommandBuffer.h"
#include "VulkanContext.h"
#include "VulkanPipeline.h"
#include "VulkanSwapchain.h"
#include "Utils/VulkanUtils.h"
#include "Core/resources.h"
#include "Render/Mesh.h"
#include "Render/Window.h"
#include "Render/PBR/Prefilter.h"
#include "Render/Vulkan/VulkanBuffer.h"
#include "Render/ResourcePool.h"
#include "Render/ShaderLibrary.h"
#include "Utils/Profiler.h"
#include "Core/Entity.h"

#include <random>
#include <backends/imgui_impl_vulkan.h>

namespace Oxylus {
  VulkanSwapchain VulkanRenderer::SwapChain;
  VulkanRenderer::RendererContext VulkanRenderer::s_RendererContext;
  VulkanRenderer::RendererData VulkanRenderer::s_RendererData;
  VulkanRenderer::RendererResources VulkanRenderer::s_Resources;
  VulkanRenderer::Pipelines VulkanRenderer::s_Pipelines;
  VulkanRenderer::FrameBuffers VulkanRenderer::s_FrameBuffers;
  RendererConfig VulkanRenderer::s_RendererConfig;

  static VulkanDescriptorSet s_PostProcessDescriptorSet;
  static VulkanDescriptorSet s_SkyboxDescriptorSet;
  static VulkanDescriptorSet s_ComputeDescriptorSet;
  static VulkanDescriptorSet s_SSAODescriptorSet;
  static VulkanDescriptorSet s_SSRDescriptorSet;
  static VulkanDescriptorSet s_QuadDescriptorSet;
  static VulkanDescriptorSet s_GaussDescriptorSet;
  static VulkanDescriptorSet s_DepthDescriptorSet;
  static VulkanDescriptorSet s_ShadowDepthDescriptorSet;
  static VulkanDescriptorSet s_BloomDescriptorSet;
  static VulkanDescriptorSet s_CompositeDescriptorSet;
  static Mesh s_SkyboxCube;
  static VulkanBuffer s_TriangleVertexBuffer;
  static VulkanBuffer s_QuadVertexBuffer;

  std::vector<VulkanRenderer::MeshData> VulkanRenderer::s_MeshDrawList;
  static bool s_ForceUpdateMaterials = false;

  std::vector<Entity> VulkanRenderer::s_SceneLights;
  Entity VulkanRenderer::s_Skylight;
  std::vector<VulkanRenderer::LightingData> VulkanRenderer::s_PointLightsData;

  std::vector<VulkanRenderer::QuadData> VulkanRenderer::s_QuadDrawList;
  std::vector<VulkanRenderer::RendererData::Vertex> VulkanRenderer::s_QuadVertexDataBuffer;

  /*
    Calculate frustum split depths and matrices for the shadow map cascades
    Based on https://johanmedestrom.wordpress.com/2016/03/18/opengl-cascaded-shadow-maps/
  */
  void VulkanRenderer::UpdateCascades(const glm::mat4& Transform,
                                      Camera* camera,
                                      RendererData::DirectShadowUB& cascadesUbo) {
    ZoneScoped;
    float cascadeSplits[SHADOW_MAP_CASCADE_COUNT]{};

    float nearClip = camera->NearClip;
    float farClip = camera->FarClip;
    float clipRange = farClip - nearClip;

    float minZ = nearClip;
    float maxZ = nearClip + clipRange;

    float range = maxZ - minZ;
    float ratio = maxZ / minZ;

    constexpr float cascadeSplitLambda = 0.95f;
    // Calculate split depths based on view camera frustum
    // Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
    for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
      float p = (float)(i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
      float log = minZ * std::pow(ratio, p);
      float uniform = minZ + range * p;
      float d = cascadeSplitLambda * (log - uniform) + uniform;
      cascadeSplits[i] = (d - nearClip) / clipRange;
    }

    // Calculate orthographic projection matrix for each cascade
    float lastSplitDist = 0.0;
    for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
      float splitDist = cascadeSplits[i];

      glm::vec3 frustumCorners[8] = {
        glm::vec3(-1.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(1.0f, -1.0f, 0.0f),
        glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(-1.0f, 1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(1.0f, -1.0f, 1.0f), glm::vec3(-1.0f, -1.0f, 1.0f),
      };

      // Project frustum corners into world space
      glm::mat4 invCam = inverse(camera->GetProjectionMatrix() * camera->GetViewMatrix());
      for (auto& frustumCorner : frustumCorners) {
        glm::vec4 invCorner = invCam * glm::vec4(frustumCorner, 1.0f);
        frustumCorner = invCorner / invCorner.w;
      }

      for (uint32_t j = 0; j < 4; j++) {
        glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
        frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
        frustumCorners[j] = frustumCorners[j] + (dist * lastSplitDist);
      }

      // Get frustum center
      auto frustumCenter = glm::vec3(0.0f);
      for (auto& frustumCorner : frustumCorners) {
        frustumCenter += frustumCorner;
      }
      frustumCenter /= 8.0f;

      float radius = 0.0f;
      for (auto& frustumCorner : frustumCorners) {
        float distance = glm::length(frustumCorner - frustumCenter);
        radius = glm::max(radius, distance);
      }
      radius = std::ceil(radius * 16.0f) / 16.0f;

      auto maxExtents = glm::vec3(radius);
      glm::vec3 minExtents = -maxExtents;

      glm::vec4 zDir = Transform * glm::vec4(0, 0, 1, 0);
      //glm::vec3 pos = Transform[3];
      glm::vec3 dir = glm::normalize(glm::vec3(zDir));
      //glm::vec3 lookAt = pos + dir;
      glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - dir * -minExtents.z,
        frustumCenter,
        glm::vec3(0.0f, 1.0f, 0.0f));
      glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x,
        maxExtents.x,
        minExtents.y,
        maxExtents.y,
        (maxExtents.z - minExtents.z) * -1,
        maxExtents.z - minExtents.z);

      //glm::vec4 zDir = Transform * glm::vec4(0, 0, 1, 0);
      //float near_plane = -100.0f, far_plane = 100.0f;
      //glm::mat4 lightProjection = glm::ortho(-20.0f, 20.0f, -10.0f, 10.0f, near_plane, far_plane);
      //glm::vec3 pos = Transform[3];
      //glm::vec3 dir = glm::normalize(glm::vec3(zDir));
      //glm::vec3 lookAt = pos + dir;
      //glm::mat4 dirLightView = glm::lookAt(pos, lookAt, glm::vec3(0, 1, 0));
      //glm::mat4 dirLightViewProj = lightProjection * dirLightView;

      // Store split distance and matrix in cascade
      cascadesUbo.cascadeSplits[i] = (camera->NearClip + splitDist * clipRange) * -1.0f;
      cascadesUbo.cascadeViewProjMat[i] = lightOrthoMatrix * lightViewMatrix;
      //cascadesUbo.cascadeViewProjMat[i] = dirLightViewProj;

      lastSplitDist = cascadeSplits[i];
    }
  }

  void VulkanRenderer::UpdateLightingData() {
    ZoneScoped;
    for (auto& e : s_SceneLights) {
      auto& lightComponent = e.GetComponent<LightComponent>();
      auto& transformComponent = e.GetComponent<TransformComponent>();
      switch (lightComponent.Type) {
        case LightComponent::LightType::Directional: break;
        case LightComponent::LightType::Point: {
          s_PointLightsData.emplace_back(LightingData{
            glm::vec4{transformComponent.Translation, lightComponent.Intensity},
            glm::vec4{lightComponent.Color, lightComponent.Range}, glm::vec4{transformComponent.Rotation, 1.0f}
          });
        }
        break;
        case LightComponent::LightType::Spot: break;
      }
    }

    if (!s_PointLightsData.empty()) {
      s_RendererData.LightsBuffer.Copy(s_PointLightsData);
      s_PointLightsData.clear();
    }
  }

  void VulkanRenderer::UpdateUniformBuffers() {
    ZoneScoped;
    s_RendererData.UBO_VS.projection = s_RendererContext.CurrentCamera->GetProjectionMatrixFlipped();
    s_RendererData.SkyboxBuffer.Copy(&s_RendererData.UBO_VS, sizeof s_RendererData.UBO_VS);

    s_RendererData.UBO_VS.projection = s_RendererContext.CurrentCamera->GetProjectionMatrixFlipped();
    s_RendererData.UBO_VS.view = s_RendererContext.CurrentCamera->GetViewMatrix();
    s_RendererData.UBO_VS.camPos = s_RendererContext.CurrentCamera->GetPosition();
    s_RendererData.VSBuffer.Copy(&s_RendererData.UBO_VS, sizeof s_RendererData.UBO_VS);

    s_RendererData.UBO_PbrPassParams.numLights = 1;
    s_RendererData.UBO_PbrPassParams.numThreads = (glm::ivec2(Window::GetWidth(), Window::GetHeight()) + PIXELS_PER_TILE - 1) /
                                          PIXELS_PER_TILE;
    s_RendererData.UBO_PbrPassParams.numThreadGroups = (s_RendererData.UBO_PbrPassParams.numThreads + TILES_PER_THREADGROUP - 1) /
                                               TILES_PER_THREADGROUP;
    s_RendererData.UBO_PbrPassParams.screenDimensions = glm::ivec2(Window::GetWidth(), Window::GetHeight());

    s_RendererData.ParametersBuffer.Copy(&s_RendererData.UBO_PbrPassParams, sizeof s_RendererData.UBO_PbrPassParams);

    UpdateLightingData();

    //TODO: Both not needed to be copied every frame.
    s_RendererData.UBO_SSAOParams.radius = RendererConfig::Get()->SSAOConfig.Radius;
    s_RendererData.SSAOBuffer.Copy(&s_RendererData.UBO_SSAOParams, sizeof s_RendererData.UBO_SSAOParams);

    s_RendererData.UBO_SSR.Samples = RendererConfig::Get()->SSRConfig.Samples;
    s_RendererData.UBO_SSR.MaxDist = RendererConfig::Get()->SSRConfig.MaxDist;
    s_RendererData.SSRBuffer.Copy(&s_RendererData.UBO_SSR, sizeof s_RendererData.UBO_SSR);

    //Composite buffer
    s_RendererData.UBO_CompositeParams.Tonemapper = RendererConfig::Get()->ColorConfig.Tonemapper;
    s_RendererData.UBO_CompositeParams.Exposure = RendererConfig::Get()->ColorConfig.Exposure;
    s_RendererData.UBO_CompositeParams.Gamma = RendererConfig::Get()->ColorConfig.Gamma;
    s_RendererData.UBO_CompositeParams.EnableSSAO = RendererConfig::Get()->SSAOConfig.Enabled;
    s_RendererData.UBO_CompositeParams.EnableBloom = RendererConfig::Get()->BloomConfig.Enabled;
    s_RendererData.UBO_CompositeParams.EnableSSR = RendererConfig::Get()->SSRConfig.Enabled;
    s_RendererData.UBO_CompositeParams.VignetteColor.a = RendererConfig::Get()->VignetteConfig.Intensity;
    s_RendererData.UBO_CompositeParams.VignetteOffset.w = RendererConfig::Get()->VignetteConfig.Enabled;
    s_RendererData.CompositeParametersBuffer.Copy(&s_RendererData.UBO_CompositeParams, sizeof s_RendererData.UBO_CompositeParams);
  }

  void VulkanRenderer::GeneratePrefilter() {
    ProfilerTimer timer("Generated Prefilters");

    Prefilter::GenerateBRDFLUT(s_Resources.LutBRDF);
    Prefilter::GenerateIrradianceCube(s_Resources.IrradianceCube,
      s_SkyboxCube,
      VertexLayout({VertexComponent::POSITION, VertexComponent::NORMAL, VertexComponent::UV}),
      s_Resources.CubeMap.GetDescImageInfo());
    Prefilter::GeneratePrefilteredCube(s_Resources.PrefilteredCube,
      s_SkyboxCube,
      VertexLayout({VertexComponent::POSITION, VertexComponent::NORMAL, VertexComponent::UV}),
      s_Resources.CubeMap.GetDescImageInfo());

    timer.Print();
  }

  void VulkanRenderer::CreateGraphicsPipelines() {
    PipelineDescription pipelineDescription{};
    pipelineDescription.Shader = CreateRef<VulkanShader>(ShaderCI{
      .VertexPath = "resources/shaders/Skybox.vert", .FragmentPath = "resources/shaders/Skybox.frag",
      .EntryPoint = "main", .Name = "Skybox"
    });
    pipelineDescription.ColorAttachmentCount = 1;
    pipelineDescription.RenderTargets[0].Format = SwapChain.m_ImageFormat;
    pipelineDescription.RasterizerDesc.CullMode = vk::CullModeFlagBits::eNone;
    pipelineDescription.RasterizerDesc.DepthBias = false;
    pipelineDescription.RasterizerDesc.FrontCounterClockwise = true;
    pipelineDescription.RasterizerDesc.DepthClamppEnable = false;
    pipelineDescription.DepthSpec.DepthWriteEnable = false;
    pipelineDescription.DepthSpec.DepthReferenceAttachment = 1;
    pipelineDescription.DepthSpec.DepthEnable = true;
    pipelineDescription.DepthSpec.CompareOp = vk::CompareOp::eLessOrEqual;
    pipelineDescription.DepthSpec.FrontFace.StencilFunc = vk::CompareOp::eNever;
    pipelineDescription.DepthSpec.BackFace.StencilFunc = vk::CompareOp::eNever;
    pipelineDescription.DepthSpec.MinDepthBound = 0;
    pipelineDescription.DepthSpec.MaxDepthBound = 0;
    pipelineDescription.DepthSpec.DepthStenctilFormat = vk::Format::eD32Sfloat; 
    pipelineDescription.DynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    pipelineDescription.VertexInputState = VertexInputDescription(VertexLayout({
      VertexComponent::POSITION, VertexComponent::NORMAL, VertexComponent::UV
    }));
    pipelineDescription.PushConstantRanges = {
      vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4)}
    };
    pipelineDescription.SetDescriptions = {
      {
        SetDescription{0, 0, 1, vDT::eUniformBuffer, vSS::eVertex, nullptr, &s_RendererData.SkyboxBuffer.GetDescriptor()},
        SetDescription{1, 0, 1, vDT::eUniformBuffer, vSS::eFragment, nullptr, &s_RendererData.CompositeParametersBuffer.GetDescriptor()},
        SetDescription{6, 0, 1, vDT::eCombinedImageSampler, vSS::eFragment, &s_Resources.CubeMap.GetDescImageInfo()},
      }
    };
    s_Pipelines.SkyboxPipeline.CreateGraphicsPipeline(pipelineDescription);

    std::vector<std::vector<SetDescription>> pbrDescriptorSet = {
      {
        SetDescription{0, 0, 1, vDT::eUniformBuffer, vSS::eFragment | vSS::eVertex, nullptr, &s_RendererData.VSBuffer.GetDescriptor()},
        SetDescription{1, 0, 1, vDT::eUniformBuffer, vSS::eFragment | vSS::eVertex, nullptr, &s_RendererData.ParametersBuffer.GetDescriptor()},
        SetDescription{2, 0, 1, vDT::eStorageBuffer, vSS::eFragment, nullptr, &s_RendererData.LightsBuffer.GetDescriptor()},
        SetDescription{3, 0, 1, vDT::eStorageBuffer, vSS::eFragment, nullptr, &s_RendererData.FrustumBuffer.GetDescriptor()},
        SetDescription{4, 0, 1, vDT::eStorageBuffer, vSS::eFragment, nullptr, &s_RendererData.LighIndexBuffer.GetDescriptor()},
        SetDescription{5, 0, 1, vDT::eStorageBuffer, vSS::eFragment, nullptr, &s_RendererData.LighGridBuffer.GetDescriptor()},
        SetDescription{6, 0, 1, vDT::eCombinedImageSampler, vSS::eFragment, &s_Resources.IrradianceCube.GetDescImageInfo()},
        SetDescription{7, 0, 1, vDT::eCombinedImageSampler, vSS::eFragment, &s_Resources.LutBRDF.GetDescImageInfo()},
        SetDescription{8, 0, 1, vDT::eCombinedImageSampler, vSS::eFragment, &s_Resources.PrefilteredCube.GetDescImageInfo()},
        SetDescription{9, 0, 1, vDT::eCombinedImageSampler, vSS::eFragment},
        SetDescription{10, 0, 1, vDT::eCombinedImageSampler, vSS::eFragment},
        SetDescription{11, 0, 1, vDT::eUniformBuffer, vSS::eFragment, nullptr, &s_RendererData.DirectShadowBuffer.GetDescriptor()},
      },
      {
        SetDescription{0, 0, 1, vDT::eCombinedImageSampler, vSS::eFragment},
        SetDescription{1, 0, 1, vDT::eCombinedImageSampler, vSS::eFragment},
        SetDescription{2, 0, 1, vDT::eCombinedImageSampler, vSS::eFragment},
        SetDescription{3, 0, 1, vDT::eCombinedImageSampler, vSS::eFragment},
        SetDescription{4, 0, 1, vDT::eCombinedImageSampler, vSS::eFragment},
      }
    };

    pipelineDescription.SetDescriptions = pbrDescriptorSet;
    pipelineDescription.PushConstantRanges.emplace_back(vk::ShaderStageFlagBits::eFragment,
                                                        sizeof(glm::mat4),
                                                        sizeof Material::Parameters);
    pipelineDescription.Shader = CreateRef<VulkanShader>(ShaderCI{
      .VertexPath = "resources/shaders/PBRTiled.vert", .FragmentPath = "resources/shaders/PBRTiled.frag",
      .EntryPoint = "main", .Name = "PBR",
      .MaterialProperties = {
        MaterialProperty{.Type = MaterialPropertyType::Float, .DisplayName = "Rougnhess",},
        MaterialProperty{.Type = MaterialPropertyType::Float, .DisplayName = "Metallic",},
        MaterialProperty{.Type = MaterialPropertyType::Float, .DisplayName = "Specular",},
        MaterialProperty{.Type = MaterialPropertyType::Float, .DisplayName = "Normal",},
        MaterialProperty{.Type = MaterialPropertyType::Float, .DisplayName = "AO",},
        MaterialProperty{.Type = MaterialPropertyType::Float3, .DisplayName = "Color", .IsColor = true},
        MaterialProperty{.Type = MaterialPropertyType::Int, .DisplayName = "UseAlbedo",},
        MaterialProperty{.Type = MaterialPropertyType::Int, .DisplayName = "UseRoughness",},
        MaterialProperty{.Type = MaterialPropertyType::Int, .DisplayName = "UseMetallic",},
        MaterialProperty{.Type = MaterialPropertyType::Int, .DisplayName = "UseNormal",},
        MaterialProperty{.Type = MaterialPropertyType::Int, .DisplayName = "UseAO",}
      }
    });
    pipelineDescription.DepthSpec.DepthWriteEnable = true;
    pipelineDescription.DepthSpec.DepthEnable = true;
    pipelineDescription.RasterizerDesc.CullMode = vk::CullModeFlagBits::eBack;
    s_Pipelines.PBRPipeline.CreateGraphicsPipeline(pipelineDescription);

    {
      PipelineDescription unlitPipelineDesc;
      unlitPipelineDesc.Shader = CreateRef<VulkanShader>(ShaderCI{
        .VertexPath = "resources/shaders/Unlit.vert", .FragmentPath = "resources/shaders/Unlit.frag",
        .EntryPoint = "main", .Name = "Unlit",
      });
      unlitPipelineDesc.ColorAttachmentCount = 1;
      unlitPipelineDesc.PushConstantRanges = {vk::PushConstantRange{vSS::eVertex, 0, sizeof glm::mat4() * 2}};
      unlitPipelineDesc.DepthSpec.DepthWriteEnable = false;
      unlitPipelineDesc.DepthSpec.DepthEnable = false;
      unlitPipelineDesc.RasterizerDesc.CullMode = vk::CullModeFlagBits::eBack;
      unlitPipelineDesc.VertexInputState = VertexInputDescription(VertexLayout({
        VertexComponent::POSITION, VertexComponent::NORMAL, VertexComponent::UV, VertexComponent::COLOR
      }));
      unlitPipelineDesc.BlendStateDesc.RenderTargets[0].BlendEnable = true;
      unlitPipelineDesc.BlendStateDesc.RenderTargets[0].DestBlend = vk::BlendFactor::eOneMinusSrcAlpha;

      s_Pipelines.UnlitPipeline.CreateGraphicsPipeline(unlitPipelineDesc);
    }
    
    PipelineDescription depthpassdescription;
    depthpassdescription.DepthSpec.CompareOp = vk::CompareOp::eLess;
    depthpassdescription.Shader = CreateRef<VulkanShader>(ShaderCI{
      .VertexPath = "resources/shaders/DepthNormalPass.vert", .FragmentPath = "resources/shaders/DepthNormalPass.frag",
      .EntryPoint = "main", .Name = "DepthPass"
    });
    depthpassdescription.SetDescriptions = pbrDescriptorSet;
    depthpassdescription.DepthAttachmentFirst = true;
    depthpassdescription.DepthSpec.DepthReferenceAttachment = 0;
    depthpassdescription.DepthSpec.DepthWriteEnable = true;
    depthpassdescription.DepthSpec.DepthEnable = true;
    depthpassdescription.DepthSpec.DepthStenctilFormat = vk::Format::eD32Sfloat;
    depthpassdescription.DepthSpec.BoundTest = true;
    depthpassdescription.DepthSpec.MinDepthBound = 0;
    depthpassdescription.DepthSpec.MaxDepthBound = 1;
    depthpassdescription.RasterizerDesc.CullMode = vk::CullModeFlagBits::eBack;
    depthpassdescription.ColorAttachmentCount = 1;
    depthpassdescription.ColorAttachment = 1;
    depthpassdescription.SubpassDependencyCount = 2;
    depthpassdescription.SubpassDescription[0].SrcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
    depthpassdescription.SubpassDescription[0].DstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests;
    depthpassdescription.SubpassDescription[0].SrcAccessMask = vk::AccessFlagBits::eMemoryRead;
    depthpassdescription.SubpassDescription[0].DstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead |
                                                               vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    depthpassdescription.SubpassDescription[1].SrcSubpass = 0;
    depthpassdescription.SubpassDescription[1].DstSubpass = VK_SUBPASS_EXTERNAL;
    depthpassdescription.SubpassDescription[1].SrcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    depthpassdescription.SubpassDescription[1].DstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
    depthpassdescription.SubpassDescription[1].SrcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    depthpassdescription.SubpassDescription[1].DstAccessMask = vk::AccessFlagBits::eShaderRead;
    depthpassdescription.VertexInputState = VertexInputDescription(VertexLayout({
      VertexComponent::POSITION,
      VertexComponent::NORMAL,
      VertexComponent::UV,
      VertexComponent::TANGENT
    }));
    depthpassdescription.PushConstantRanges = {
      vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4)}
    };
    depthpassdescription.PushConstantRanges.emplace_back(vk::ShaderStageFlagBits::eFragment,
      sizeof(glm::mat4),
      sizeof Material::Parameters);
    s_Pipelines.DepthPrePassPipeline.CreateGraphicsPipeline(depthpassdescription);

    pipelineDescription.Shader = CreateRef<VulkanShader>(ShaderCI{
      .VertexPath = "resources/shaders/DirectShadowDepthPass.vert",
      .FragmentPath = "resources/shaders/DirectShadowDepthPass.frag", .EntryPoint = "main", .Name = "DirectShadowDepth"
    });
    pipelineDescription.PushConstantRanges = {vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex, 0, 68}};
    pipelineDescription.ColorAttachmentCount = 0;
    pipelineDescription.RasterizerDesc.CullMode = vk::CullModeFlagBits::eNone;
    pipelineDescription.DepthSpec.DepthEnable = true;
    pipelineDescription.RasterizerDesc.DepthClamppEnable = true;
    pipelineDescription.RasterizerDesc.FrontCounterClockwise = false;
    pipelineDescription.DepthSpec.BoundTest = false;
    pipelineDescription.DepthSpec.CompareOp = vk::CompareOp::eLessOrEqual;
    pipelineDescription.DepthSpec.MaxDepthBound = 0;
    pipelineDescription.DepthSpec.DepthReferenceAttachment = 0;
    pipelineDescription.DepthSpec.BackFace.StencilFunc = vk::CompareOp::eAlways;
    pipelineDescription.DepthAttachmentLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
    pipelineDescription.SetDescriptions = {
      {
        SetDescription{0, 0, 1, vDT::eUniformBuffer, vSS::eVertex, nullptr, &s_RendererData.DirectShadowBuffer.GetDescriptor()}
      }
    };
    s_Pipelines.DirectShadowDepthPipeline.CreateGraphicsPipeline(pipelineDescription);

    PipelineDescription ssaoDescription;
    ssaoDescription.RenderTargets[0].Format = vk::Format::eR8Unorm;
    ssaoDescription.DepthSpec.DepthEnable = false;
    ssaoDescription.SubpassDescription[0].DstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    ssaoDescription.SubpassDescription[0].SrcAccessMask = {};
    ssaoDescription.SetDescriptions = {
      {
        SetDescription{0, 0, 1, vDT::eUniformBuffer, vSS::eCompute, nullptr, &s_RendererData.VSBuffer.GetDescriptor()},
        SetDescription{1, 0, 1, vDT::eCombinedImageSampler, vSS::eCompute},
        SetDescription{2, 0, 1, vDT::eStorageImage, vSS::eCompute},
        SetDescription{3, 0, 1, vDT::eCombinedImageSampler, vSS::eCompute},
      }
    };
    ssaoDescription.Shader = CreateRef<VulkanShader>(ShaderCI{
      .EntryPoint = "main",
      .Name = "SSAO",
      .ComputePath = "resources/Shaders/SSAO.comp",
    });
    s_Pipelines.SSAOPassPipeline.CreateComputePipeline(ssaoDescription);

    {
      //PipelineDescription gaussianBlurDesc;
      //gaussianBlurDesc.ColorAttachmentCount = 1;
      //gaussianBlurDesc.DepthSpec.DepthEnable = false;
      //gaussianBlurDesc.DescriptorSetLayoutBindings = {{{0, vDT::eCombinedImageSampler, 1, vSS::eFragment},}};
      //gaussianBlurDesc.PushConstantRanges = {
      //  vk::PushConstantRange{vk::ShaderStageFlagBits::eFragment, 0, sizeof(int32_t)}
      //};
      //gaussianBlurDesc.Shader = CreateRef<VulkanShader>(ShaderCI{
      //  .VertexPath = "resources/Shaders/FlatColor.vert", .FragmentPath = "resources/Shaders/GaussianBlur.frag",
      //  .EntryPoint = "main", .Name = "GaussianBlur"
      //});
      //gaussianBlurDesc.RasterizerDesc.CullMode = vk::CullModeFlagBits::eNone;
      //gaussianBlurDesc.VertexInputState = VertexInputDescription(VertexLayout({
      //  VertexComponent::POSITION, VertexComponent::NORMAL, VertexComponent::UV
      //}));
      //s_Pipelines.GaussianBlurPipeline.CreateGraphicsPipeline(gaussianBlurDesc);

      PipelineDescription bloomDesc;
      bloomDesc.ColorAttachmentCount = 1;
      bloomDesc.DepthSpec.DepthEnable = false;
      bloomDesc.PushConstantRanges.emplace_back(vk::ShaderStageFlagBits::eCompute, 0, 12);
      bloomDesc.SetDescriptions = {
        {
          SetDescription{0, 0, 1, vDT::eStorageImage, vSS::eCompute},
          SetDescription{0, 1, 1, vDT::eStorageImage, vSS::eCompute},
          SetDescription{1, 0, 1, vDT::eCombinedImageSampler, vSS::eCompute},
          SetDescription{1, 1, 1, vDT::eCombinedImageSampler, vSS::eCompute},
          SetDescription{1, 2, 1, vDT::eCombinedImageSampler, vSS::eCompute},
          SetDescription{2, 0, 1, vDT::eUniformBuffer, vSS::eCompute, nullptr, &s_RendererData.BloomBuffer.GetDescriptor()},
        }
      };
      bloomDesc.Shader = CreateRef<VulkanShader>(ShaderCI{
        .EntryPoint = "main",
        .Name = "Bloom",
        .ComputePath = "resources/shaders/Bloom.comp",
      });
      //s_Pipelines.BloomPipeline.CreateComputePipeline(bloomDesc);
    }
    {
      PipelineDescription ssrDesc;
      ssrDesc.DepthSpec.DepthEnable = false;
      ssrDesc.SetDescriptions = {
        {
          SetDescription{0, 0, 1, vDT::eStorageImage, vSS::eCompute},
          SetDescription{1, 0, 1, vDT::eCombinedImageSampler, vSS::eCompute},
          SetDescription{2, 0, 1, vDT::eCombinedImageSampler, vSS::eCompute},
          SetDescription{3, 0, 1, vDT::eCombinedImageSampler, vSS::eCompute},
          SetDescription{4, 0, 1, vDT::eCombinedImageSampler, vSS::eCompute},
          SetDescription{5, 0, 1, vDT::eUniformBuffer, vSS::eCompute, nullptr, &s_RendererData.VSBuffer.GetDescriptor()},
          SetDescription{6, 0, 1, vDT::eUniformBuffer, vSS::eCompute, nullptr, &s_RendererData.SSRBuffer.GetDescriptor()},
        }
      };
      ssrDesc.Shader = CreateRef<VulkanShader>(ShaderCI{
        .EntryPoint = "main",
        .Name = "SSR",
        .ComputePath = "resources/shaders/SSR.comp",
      });
      s_Pipelines.SSRPipeline.CreateComputePipeline(ssrDesc);
    }
    {
      PipelineDescription composite;
      composite.DepthSpec.DepthEnable = false;
      composite.SetDescriptions = {
        {
          SetDescription{0, 0, 1, vDT::eStorageImage, vSS::eCompute},
          SetDescription{1, 0, 1, vDT::eCombinedImageSampler, vSS::eCompute},
          SetDescription{2, 0, 1, vDT::eCombinedImageSampler, vSS::eCompute},
          SetDescription{3, 0, 1, vDT::eCombinedImageSampler, vSS::eCompute},
          SetDescription{4, 0, 1, vDT::eCombinedImageSampler, vSS::eCompute},
          SetDescription{5, 0, 1, vDT::eUniformBuffer, vSS::eCompute, nullptr, &s_RendererData.CompositeParametersBuffer.GetDescriptor()},
        }
      };
      composite.Shader = CreateRef<VulkanShader>(ShaderCI{
        .EntryPoint = "main",
        .Name = "Composite",
        .ComputePath = "resources/shaders/Composite.comp",
      });
      s_Pipelines.CompositePipeline.CreateComputePipeline(composite);
    }
    {
      PipelineDescription ppPass;
      ppPass.SetDescriptions = {
        {
          SetDescription{0, 0, 1, vDT::eCombinedImageSampler, vSS::eFragment},
          SetDescription{1, 0, 1, vDT::eUniformBuffer, vSS::eFragment, nullptr, &s_RendererData.CompositeParametersBuffer.GetDescriptor()},
        }
      };
      ppPass.Shader = CreateRef<VulkanShader>(ShaderCI{
        .VertexPath = "resources/shaders/PostProcess.vert", .FragmentPath = "resources/shaders/PostProcess.frag",
        .EntryPoint = "main", .Name = "PostProcess"
      });
      ppPass.RasterizerDesc.CullMode = vk::CullModeFlagBits::eNone;
      ppPass.VertexInputState = VertexInputDescription(VertexLayout({
        VertexComponent::POSITION, VertexComponent::NORMAL, VertexComponent::UV
      }));
      ppPass.DepthSpec.DepthEnable = false;
      s_Pipelines.PostProcessPipeline.CreateGraphicsPipeline(ppPass);
    }
    {
      PipelineDescription quadDescription;
      quadDescription.SetDescriptions = {
        {
          SetDescription{7, 0, 1, vDT::eCombinedImageSampler, vSS::eFragment},
        }
      };
      quadDescription.RenderPass = SwapChain.m_RenderPass;
      quadDescription.VertexInputState.attributeDescriptions.clear();
      quadDescription.VertexInputState.bindingDescriptions.clear();
      quadDescription.Shader = CreateRef<VulkanShader>(ShaderCI{
        .VertexPath = "resources/shaders/quad.vert", .FragmentPath = "resources/shaders/quad.frag", .EntryPoint = "main",
        .Name = "Quad",
      });
      quadDescription.RasterizerDesc.CullMode = vk::CullModeFlagBits::eNone;
      s_Pipelines.QuadPipeline.CreateGraphicsPipeline(quadDescription);
    }

    PipelineDescription computePipelineDesc;
    computePipelineDesc.SetDescriptions = {
      {
        SetDescription{0, 0, 1, vDT::eUniformBuffer, vSS::eCompute, nullptr, &s_RendererData.VSBuffer.GetDescriptor()},
        SetDescription{1, 0, 1, vDT::eUniformBuffer, vSS::eCompute, nullptr, &s_RendererData.ParametersBuffer.GetDescriptor()},
        SetDescription{2, 0, 1, vDT::eStorageBuffer, vSS::eCompute, nullptr, &s_RendererData.LightsBuffer.GetDescriptor()},
        SetDescription{3, 0, 1, vDT::eStorageBuffer, vSS::eCompute, nullptr, &s_RendererData.FrustumBuffer.GetDescriptor()},
        SetDescription{4, 0, 1, vDT::eStorageBuffer, vSS::eCompute, nullptr, &s_RendererData.LighIndexBuffer.GetDescriptor()},
        SetDescription{5, 0, 1, vDT::eStorageBuffer, vSS::eCompute, nullptr, &s_RendererData.LighGridBuffer.GetDescriptor()},
        SetDescription{6, 0, 1, vDT::eCombinedImageSampler, vSS::eCompute},
      }
    };
    computePipelineDesc.Shader = CreateRef<VulkanShader>(ShaderCI{
      .EntryPoint = "main", .Name = "FrustumGrid", .ComputePath = "resources/shaders/ComputeFrustumGrid.comp",
    });
    s_Pipelines.FrustumGridPipeline.CreateComputePipeline(computePipelineDesc);

    computePipelineDesc.Shader = CreateRef<VulkanShader>(ShaderCI{
      .EntryPoint = "main", .Name = "LightList", .ComputePath = "resources/shaders/ComputeLightList.comp",
    });
    s_Pipelines.LightListPipeline.CreateComputePipeline(computePipelineDesc);

    const std::vector vertexInputBindings = {
      vk::VertexInputBindingDescription{0, sizeof(ImDrawVert), vk::VertexInputRate::eVertex},
    };
    const std::vector vertexInputAttributes = {
      vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(ImDrawVert, pos)},
      vk::VertexInputAttributeDescription{1, 0, vk::Format::eR32G32Sfloat, (uint32_t)offsetof(ImDrawVert, uv)},
      vk::VertexInputAttributeDescription{2, 0, vk::Format::eR8G8B8A8Unorm, (uint32_t)offsetof(ImDrawVert, col)},
    };
    PipelineDescription uiPipelineDecs;
    uiPipelineDecs.Shader = CreateRef<VulkanShader>(ShaderCI{
      .VertexPath = "resources/Shaders/ui.vert", .FragmentPath = "resources/Shaders/ui.frag", .EntryPoint = "main",
      .Name = "UI",
    });
    uiPipelineDecs.SetDescriptions = {
      {
        SetDescription{0, 0, 1, vDT::eCombinedImageSampler, vSS::eFragment},
      }
    };
    uiPipelineDecs.PushConstantRanges = {vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex, 0, sizeof(float) * 4}};
    uiPipelineDecs.RenderPass = SwapChain.m_RenderPass;
    uiPipelineDecs.VertexInputState.attributeDescriptions = vertexInputAttributes;
    uiPipelineDecs.VertexInputState.bindingDescriptions = vertexInputBindings;
    uiPipelineDecs.RasterizerDesc.CullMode = vk::CullModeFlagBits::eNone;
    uiPipelineDecs.BlendStateDesc.RenderTargets[0].BlendEnable = true;
    uiPipelineDecs.BlendStateDesc.RenderTargets[0].SrcBlend = vk::BlendFactor::eSrcAlpha;
    uiPipelineDecs.BlendStateDesc.RenderTargets[0].DestBlend = vk::BlendFactor::eOneMinusSrcAlpha;
    uiPipelineDecs.BlendStateDesc.RenderTargets[0].BlendOp = vk::BlendOp::eAdd;
    uiPipelineDecs.BlendStateDesc.RenderTargets[0].SrcBlendAlpha = vk::BlendFactor::eOne;
    uiPipelineDecs.BlendStateDesc.RenderTargets[0].DestBlendAlpha = vk::BlendFactor::eOneMinusSrcAlpha;
    uiPipelineDecs.BlendStateDesc.RenderTargets[0].BlendOpAlpha = vk::BlendOp::eAdd;
    uiPipelineDecs.BlendStateDesc.RenderTargets[0].WriteMask = {
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB |
      vk::ColorComponentFlagBits::eA
    };
    uiPipelineDecs.DepthSpec.DepthEnable = false;
    uiPipelineDecs.DepthSpec.DepthWriteEnable = false;
    uiPipelineDecs.DepthSpec.CompareOp = vk::CompareOp::eNever;
    uiPipelineDecs.DepthSpec.FrontFace.StencilFunc = vk::CompareOp::eNever;
    uiPipelineDecs.DepthSpec.BackFace.StencilFunc = vk::CompareOp::eNever;
    uiPipelineDecs.DepthSpec.BoundTest = false;
    uiPipelineDecs.DepthSpec.MinDepthBound = 0;
    uiPipelineDecs.DepthSpec.MaxDepthBound = 0;

    s_Pipelines.UIPipeline.CreateGraphicsPipeline(uiPipelineDecs);
  }

  void VulkanRenderer::CreateFramebuffers() {
    VulkanImageDescription colorImageDesc{};
    colorImageDesc.Format = SwapChain.m_ImageFormat;
    colorImageDesc.UsageFlags = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    colorImageDesc.ImageTiling = vk::ImageTiling::eOptimal;
    colorImageDesc.Width = SwapChain.m_Extent.width;
    colorImageDesc.Height = SwapChain.m_Extent.height;
    colorImageDesc.CreateView = true;
    colorImageDesc.CreateSampler = true;
    colorImageDesc.CreateDescriptorSet = true;
    colorImageDesc.AspectFlag = vk::ImageAspectFlagBits::eColor;
    colorImageDesc.FinalImageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    colorImageDesc.SamplerAddressMode = vk::SamplerAddressMode::eClampToEdge;
    {
      VulkanImageDescription depthImageDesc{};
      depthImageDesc.Format = vk::Format::eD32Sfloat;
      depthImageDesc.Height = SwapChain.m_Extent.height;
      depthImageDesc.Width = SwapChain.m_Extent.width;
      depthImageDesc.UsageFlags = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
      depthImageDesc.ImageTiling = vk::ImageTiling::eOptimal;
      depthImageDesc.CreateSampler = true;
      depthImageDesc.CreateDescriptorSet = true;
      depthImageDesc.DescriptorSetLayout = s_RendererData.ImageDescriptorSetLayout;
      depthImageDesc.CreateView = true;
      depthImageDesc.AspectFlag = vk::ImageAspectFlagBits::eDepth;
      depthImageDesc.FinalImageLayout = vk::ImageLayout::eDepthReadOnlyOptimal;
      depthImageDesc.TransitionLayoutAtCreate = true;
      FramebufferDescription framebufferDescription;
      framebufferDescription.DebugName = "Depth Pass";
      framebufferDescription.RenderPass = s_Pipelines.DepthPrePassPipeline.GetRenderPass().Get();
      framebufferDescription.Width = SwapChain.m_Extent.width;
      framebufferDescription.Height = SwapChain.m_Extent.height;
      framebufferDescription.Extent = &Window::GetWindowExtent();
      framebufferDescription.ImageDescription = {depthImageDesc, colorImageDesc};
      framebufferDescription.OnResize = [] {
        s_DepthDescriptorSet.WriteDescriptorSets[0].pBufferInfo = &s_RendererData.VSBuffer.GetDescriptor();
        s_ComputeDescriptorSet.WriteDescriptorSets[6].pImageInfo = &s_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetDescImageInfo();
        UpdateComputeDescriptorSets();
      };
      s_FrameBuffers.DepthNormalPassFB.CreateFramebuffer(framebufferDescription);

      //Direct shadow depth pass
      depthImageDesc.Format = vk::Format::eD32Sfloat;
      depthImageDesc.Width = RendererConfig::Get()->DirectShadowsConfig.Size;
      depthImageDesc.Height = RendererConfig::Get()->DirectShadowsConfig.Size;
      framebufferDescription.Extent = nullptr;
      depthImageDesc.Type = ImageType::TYPE_2DARRAY;
      depthImageDesc.ImageArrayLayerCount = SHADOW_MAP_CASCADE_COUNT;
      depthImageDesc.ViewArrayLayerCount = SHADOW_MAP_CASCADE_COUNT;
      depthImageDesc.FinalImageLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
      depthImageDesc.TransitionLayoutAtCreate = true;
      depthImageDesc.BaseArrayLayerIndex = 0;
      s_Resources.DirectShadowsDepthArray.Create(depthImageDesc);

      framebufferDescription.Width = RendererConfig::Get()->DirectShadowsConfig.Size;
      framebufferDescription.Height = RendererConfig::Get()->DirectShadowsConfig.Size;
      framebufferDescription.DebugName = "Direct Shadow Depth Pass";
      framebufferDescription.RenderPass = s_Pipelines.DirectShadowDepthPipeline.GetRenderPass().Get();
      framebufferDescription.OnResize = [] {
        s_ShadowDepthDescriptorSet.WriteDescriptorSets[0].pBufferInfo = &s_RendererData.DirectShadowBuffer.GetDescriptor();
        s_ShadowDepthDescriptorSet.Update();
      };
      s_FrameBuffers.DirectionalCascadesFB.resize(SHADOW_MAP_CASCADE_COUNT);
      int baseArrayLayer = 0;
      depthImageDesc.ViewArrayLayerCount = 1;
      for (auto& fb : s_FrameBuffers.DirectionalCascadesFB) {
        depthImageDesc.BaseArrayLayerIndex = baseArrayLayer;
        framebufferDescription.ImageDescription = {depthImageDesc};
        vk::ImageViewCreateInfo viewInfo{};
        viewInfo.viewType = vk::ImageViewType::e2DArray;
        viewInfo.format = vk::Format::eD32Sfloat;
        viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.image = s_Resources.DirectShadowsDepthArray.GetImage();
        const auto& LogicalDevice = VulkanContext::Context.Device;
        vk::ImageView view;
        VulkanUtils::CheckResult(LogicalDevice.createImageView(&viewInfo, nullptr, &view));
        fb.CreateFramebufferWithImageView(framebufferDescription, view);
        baseArrayLayer++;
      }
    }
    {
      VulkanImageDescription DepthImageDesc;
      DepthImageDesc.Format = vk::Format::eD32Sfloat;
      DepthImageDesc.UsageFlags = vk::ImageUsageFlagBits::eDepthStencilAttachment |
                                  vk::ImageUsageFlagBits::eInputAttachment;
      DepthImageDesc.ImageTiling = vk::ImageTiling::eOptimal;
      DepthImageDesc.Width = SwapChain.m_Extent.width;
      DepthImageDesc.Height = SwapChain.m_Extent.height;
      DepthImageDesc.CreateView = true;
      DepthImageDesc.CreateSampler = true;
      DepthImageDesc.CreateDescriptorSet = false;
      DepthImageDesc.AspectFlag = vk::ImageAspectFlagBits::eDepth;
      DepthImageDesc.FinalImageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

      FramebufferDescription framebufferDescription;
      framebufferDescription.DebugName = "PBR Pass";
      framebufferDescription.Width = SwapChain.m_Extent.width;
      framebufferDescription.Height = SwapChain.m_Extent.height;
      framebufferDescription.Extent = &Window::GetWindowExtent();
      framebufferDescription.RenderPass = s_Pipelines.PBRPipeline.GetRenderPass().Get();
      framebufferDescription.ImageDescription = {colorImageDesc, DepthImageDesc};
      framebufferDescription.OnResize = [] {
        s_QuadDescriptorSet.WriteDescriptorSets[0].pImageInfo = &s_FrameBuffers.PBRPassFB.GetImage()[0].GetDescImageInfo();
        s_QuadDescriptorSet.Update();
      };
      s_FrameBuffers.PBRPassFB.CreateFramebuffer(framebufferDescription);
    }
    {
      VulkanImageDescription ssaopassimage;
      ssaopassimage.Height = Window::GetHeight();
      ssaopassimage.Width = Window::GetWidth();
      ssaopassimage.CreateDescriptorSet = true;
      ssaopassimage.UsageFlags = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
      ssaopassimage.Format = vk::Format::eR8Unorm;
      ssaopassimage.FinalImageLayout = vk::ImageLayout::eGeneral;
      ssaopassimage.TransitionLayoutAtCreate = true;
      ssaopassimage.SamplerAddressMode = vk::SamplerAddressMode::eClampToBorder;
      ssaopassimage.SamplerBorderColor = vk::BorderColor::eFloatTransparentBlack;
      s_FrameBuffers.SSAOPassImage.Create(ssaopassimage);

      ImagePool::AddToPool(
        &s_FrameBuffers.SSAOPassImage,
        &Window::GetWindowExtent(),
        [] {
          UpdateSSAODescriptorSets();
          s_FrameBuffers.PostProcessPassFB.GetDescription().OnResize();
        });
    }
    {
      VulkanImageDescription ssrPassImage;
      ssrPassImage.Height = Window::GetHeight();
      ssrPassImage.Width = Window::GetWidth();
      ssrPassImage.CreateDescriptorSet = true;
      ssrPassImage.UsageFlags = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
      ssrPassImage.Format = SwapChain.m_ImageFormat;
      ssrPassImage.FinalImageLayout = vk::ImageLayout::eGeneral;
      ssrPassImage.TransitionLayoutAtCreate = true;
      ssrPassImage.SamplerAddressMode = vk::SamplerAddressMode::eClampToBorder;
      ssrPassImage.SamplerBorderColor = vk::BorderColor::eFloatTransparentBlack;
      s_FrameBuffers.SSRPassImage.Create(ssrPassImage);

      ImagePool::AddToPool(
        &s_FrameBuffers.SSRPassImage,
        &Window::GetWindowExtent(),
        [] {
          s_SSRDescriptorSet.WriteDescriptorSets[0].pImageInfo = &s_FrameBuffers.SSRPassImage.GetDescImageInfo();
          s_SSRDescriptorSet.WriteDescriptorSets[1].pImageInfo = &s_FrameBuffers.PBRPassFB.GetImage()[0].GetDescImageInfo();
          s_SSRDescriptorSet.WriteDescriptorSets[2].pImageInfo = &s_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetDescImageInfo();
          s_SSRDescriptorSet.WriteDescriptorSets[3].pImageInfo = &s_Resources.CubeMap.GetDescImageInfo();
          s_SSRDescriptorSet.WriteDescriptorSets[4].pImageInfo = &s_FrameBuffers.DepthNormalPassFB.GetImage()[1].GetDescImageInfo();
          s_SSRDescriptorSet.Update();
          s_FrameBuffers.PostProcessPassFB.GetDescription().OnResize();
        });
    }
    {
      VulkanImageDescription composite;
      composite.Height = Window::GetHeight();
      composite.Width = Window::GetWidth();
      composite.CreateDescriptorSet = true;
      composite.UsageFlags = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
      composite.Format = SwapChain.m_ImageFormat;
      composite.FinalImageLayout = vk::ImageLayout::eGeneral;
      composite.TransitionLayoutAtCreate = true;
      composite.SamplerAddressMode = vk::SamplerAddressMode::eClampToBorder;
      composite.SamplerBorderColor = vk::BorderColor::eFloatTransparentBlack;
      s_FrameBuffers.CompositePassImage.Create(composite);

      ImagePool::AddToPool(
        &s_FrameBuffers.CompositePassImage,
        &Window::GetWindowExtent(),
        [] {
          s_CompositeDescriptorSet.WriteDescriptorSets[0].pImageInfo = &s_FrameBuffers.CompositePassImage.GetDescImageInfo();
          s_CompositeDescriptorSet.WriteDescriptorSets[1].pImageInfo = &s_FrameBuffers.PBRPassFB.GetImage()[0].GetDescImageInfo();
          s_CompositeDescriptorSet.WriteDescriptorSets[2].pImageInfo = &s_FrameBuffers.SSAOPassImage.GetDescImageInfo();
          s_CompositeDescriptorSet.WriteDescriptorSets[3].pImageInfo = &s_FrameBuffers.BloomPassImage.GetDescImageInfo();
          s_CompositeDescriptorSet.WriteDescriptorSets[4].pImageInfo = &s_FrameBuffers.SSRPassImage.GetDescImageInfo();
          s_CompositeDescriptorSet.WriteDescriptorSets[5].pImageInfo = &s_FrameBuffers.PostProcessPassFB.GetImage()[0].GetDescImageInfo();
          s_CompositeDescriptorSet.Update();
          s_FrameBuffers.PostProcessPassFB.GetDescription().OnResize();
        });
    }
    {
      VulkanImageDescription bloompassimage;
      bloompassimage.Height = Window::GetHeight();
      bloompassimage.Width = Window::GetWidth();
      bloompassimage.CreateDescriptorSet = true;
      bloompassimage.UsageFlags = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
      bloompassimage.Format = SwapChain.m_ImageFormat;
      bloompassimage.FinalImageLayout = vk::ImageLayout::eGeneral;
      bloompassimage.TransitionLayoutAtCreate = true;
      bloompassimage.SamplerAddressMode = vk::SamplerAddressMode::eClampToBorder;
      bloompassimage.SamplerBorderColor = vk::BorderColor::eFloatTransparentBlack;
      s_FrameBuffers.BloomPassImage.Create(bloompassimage);
      s_FrameBuffers.BloomDownsampleImage.Create(bloompassimage);
      s_FrameBuffers.BloomUpsampleImage.Create(bloompassimage);
      auto updateBloomSet = []{
        /*s_BloomDescriptorSet.WriteDescriptorSets[0].pImageInfo = &s_FrameBuffers.BloomDownsampleImage.GetDescImageInfo();
        s_BloomDescriptorSet.WriteDescriptorSets[1].pImageInfo = &s_FrameBuffers.BloomUpsampleImage.GetDescImageInfo();
        s_BloomDescriptorSet.WriteDescriptorSets[2].pImageInfo = &s_FrameBuffers.PBRPassFB.GetImage()[0].GetDescImageInfo();
        s_BloomDescriptorSet.WriteDescriptorSets[3].pImageInfo = &s_FrameBuffers.BloomDownsampleImage.GetDescImageInfo();
        s_BloomDescriptorSet.WriteDescriptorSets[4].pImageInfo = &s_FrameBuffers.BloomUpsampleImage.GetDescImageInfo();
        s_BloomDescriptorSet.Update();*/

        s_FrameBuffers.PostProcessPassFB.GetDescription().OnResize();
      };
      ImagePool::AddToPool(
        &s_FrameBuffers.BloomUpsampleImage,
        &Window::GetWindowExtent(),
        [updateBloomSet] {
          updateBloomSet();
        });
      ImagePool::AddToPool(
        &s_FrameBuffers.BloomDownsampleImage,
        &Window::GetWindowExtent(),
        [updateBloomSet] {
          updateBloomSet();
        });
      ImagePool::AddToPool(
        &s_FrameBuffers.BloomPassImage,
        &Window::GetWindowExtent(),
        [updateBloomSet] {
          updateBloomSet();
          s_FrameBuffers.PostProcessPassFB.GetDescription().OnResize();
        });
    }
    {
      FramebufferDescription pbrCompositeDescription;
      pbrCompositeDescription.DebugName = "Post Process Pass";
      pbrCompositeDescription.Width = Window::GetWidth();
      pbrCompositeDescription.Height = Window::GetHeight();
      pbrCompositeDescription.Extent = &Window::GetWindowExtent();
      pbrCompositeDescription.RenderPass = s_Pipelines.PostProcessPipeline.GetRenderPass().Get();
      colorImageDesc.Format = SwapChain.m_ImageFormat;
      pbrCompositeDescription.ImageDescription = {colorImageDesc};
      pbrCompositeDescription.OnResize = [] {
        s_PostProcessDescriptorSet.WriteDescriptorSets[0].pImageInfo = &s_FrameBuffers.CompositePassImage.GetDescImageInfo();
        s_PostProcessDescriptorSet.Update();
      };
      s_FrameBuffers.PostProcessPassFB.CreateFramebuffer(pbrCompositeDescription);
    }
  }

  void VulkanRenderer::ResizeBuffers() {
    WaitDeviceIdle();
    WaitGraphicsQueueIdle();
    SwapChain.RecreateSwapChain();
    FrameBufferPool::ResizeBuffers();
    ImagePool::ResizeImages();
    SwapChain.Resizing = false;
  }

  void VulkanRenderer::UpdateSkyboxDescriptorSets() {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    LogicalDevice.updateDescriptorSets(
      {
        {s_SkyboxDescriptorSet.Get(), 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &s_RendererData.SkyboxBuffer.GetDescriptor()},
        {s_SkyboxDescriptorSet.Get(), 1, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &s_RendererData.CompositeParametersBuffer.GetDescriptor()},
        {s_SkyboxDescriptorSet.Get(), 6, 0, 1, vk::DescriptorType::eCombinedImageSampler, &s_Resources.CubeMap.GetDescImageInfo()},
      },
      nullptr);
  }

  void VulkanRenderer::UpdateComputeDescriptorSets() {
    s_ComputeDescriptorSet.WriteDescriptorSets[6].pImageInfo = &s_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetDescImageInfo();
    s_ComputeDescriptorSet.Update();
  }

  void VulkanRenderer::UpdateSSAODescriptorSets() {
    s_SSAODescriptorSet.WriteDescriptorSets[1].pImageInfo = &s_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetDescImageInfo();
    s_SSAODescriptorSet.WriteDescriptorSets[2].pImageInfo = &s_FrameBuffers.SSAOPassImage.GetDescImageInfo();
    s_SSAODescriptorSet.WriteDescriptorSets[3].pImageInfo = &s_FrameBuffers.DepthNormalPassFB.GetImage()[1].GetDescImageInfo();
    s_SSAODescriptorSet.Update();
  }

  void VulkanRenderer::InitRenderGraph() {
    auto& renderGraph = s_RendererContext.RenderGraph;

    SwapchainPass swapchain{&s_QuadDescriptorSet};
    renderGraph.SetSwapchain(swapchain);

    RenderGraphPass depthPrePass(
      "Depth Pre Pass",
      {&s_RendererContext.DepthPassCommandBuffer},
      &s_Pipelines.DepthPrePassPipeline,
      {&s_FrameBuffers.DepthNormalPassFB},
      [](VulkanCommandBuffer& commandBuffer, int32_t) {
        ZoneScopedN("DepthPrePass");
        OX_TRACE_GPU(commandBuffer.Get(), "Depth Pre Pass")
        commandBuffer.SetViwportWindow().SetScissorWindow();
        for (const auto& mesh : s_MeshDrawList) {
          if (mesh.MeshGeometry.Nodes.empty())
            continue;

          RenderMesh(mesh,
            s_RendererContext.DepthPassCommandBuffer.Get(),
            s_Pipelines.DepthPrePassPipeline,
            [&](const Mesh::Primitive* part) {
              const auto& material = mesh.MeshGeometry.GetMaterial(part->materialIndex);
              if (!material->IsOpaque())
                return false;
              commandBuffer.PushConstants(s_Pipelines.DepthPrePassPipeline.GetPipelineLayout(),
                vk::ShaderStageFlagBits::eVertex,
                0,
                sizeof(glm::mat4),
                &mesh.Transform);
              commandBuffer.PushConstants(s_Pipelines.PBRPipeline.GetPipelineLayout(),
                vk::ShaderStageFlagBits::eFragment,
                sizeof(glm::mat4),
                sizeof Material::Parameters,
                &material->Parameters);
              s_Pipelines.DepthPrePassPipeline.BindDescriptorSets(commandBuffer.Get(), {Material::s_DescriptorSet.Get(), material->MaterialDescriptorSet.Get()}, 0, 2);
              return true;
            });
        }
      },
      {vk::ClearDepthStencilValue{1.0f, 0}, vk::ClearColorValue(std::array{0.0f, 0.0f, 0.0f, 1.0f})},
      &VulkanContext::VulkanQueue.GraphicsQueue);
    renderGraph.AddRenderPass(depthPrePass);

    std::array<vk::ClearValue, 2> clearValues;
    clearValues[0].color = vk::ClearColorValue(std::array{0.0f, 0.0f, 0.0f, 1.0f});
    clearValues[1].depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

    RenderGraphPass directShadowDepthPass(
      "Direct Shadow Depth Pass",
      {&s_RendererContext.DirectShadowCommandBuffer},
      &s_Pipelines.DirectShadowDepthPipeline,
      {
        {
          &s_FrameBuffers.DirectionalCascadesFB[0], &s_FrameBuffers.DirectionalCascadesFB[1],
          &s_FrameBuffers.DirectionalCascadesFB[2], &s_FrameBuffers.DirectionalCascadesFB[3]
        }
      },
      [](VulkanCommandBuffer& commandBuffer, int32_t framebufferIndex) {
        ZoneScopedN("DirectShadowDepthPass");
        OX_TRACE_GPU(commandBuffer.Get(), "Direct Shadow Depth Pass")
        commandBuffer.SetViewport(vk::Viewport{
          0, 0, (float)RendererConfig::Get()->DirectShadowsConfig.Size,
          (float)RendererConfig::Get()->DirectShadowsConfig.Size, 0, 1
        }).SetScissor(vk::Rect2D{
          {}, {RendererConfig::Get()->DirectShadowsConfig.Size, RendererConfig::Get()->DirectShadowsConfig.Size,}
        });
        for (const auto& e : s_SceneLights) {
          const auto& lightComponent = e.GetComponent<LightComponent>();
          if (lightComponent.Type != LightComponent::LightType::Directional)
            continue;

          glm::mat4 transform = e.GetWorldTransform();
          UpdateCascades(transform, s_RendererContext.CurrentCamera, s_RendererData.UBO_DirectShadow);
          s_RendererData.DirectShadowBuffer.Copy(&s_RendererData.UBO_DirectShadow,
            sizeof s_RendererData.UBO_DirectShadow);

          for (const auto& mesh : s_MeshDrawList) {
            if (mesh.MeshGeometry.Nodes.empty())
              continue;
            RenderMesh(mesh,
              commandBuffer.Get(),
              s_Pipelines.DirectShadowDepthPipeline,
              [&](const Mesh::Primitive*) {
                struct PushConst {
                  glm::mat4 modelMatrix{};
                  uint32_t cascadeIndex = 0;
                } pushConst;
                pushConst.modelMatrix = mesh.Transform;
                pushConst.cascadeIndex = framebufferIndex;
                commandBuffer.PushConstants(s_Pipelines.DirectShadowDepthPipeline.GetPipelineLayout(),
                  vk::ShaderStageFlagBits::eVertex,
                  0,
                  sizeof(PushConst),
                  &pushConst);
                s_Pipelines.DirectShadowDepthPipeline.BindDescriptorSets(commandBuffer.Get(), {s_ShadowDepthDescriptorSet.Get()});
                return true;
              });
          }
        }
      },
      {vk::ClearDepthStencilValue{1.0f, 0}, vk::ClearColorValue(std::array{0.0f, 0.0f, 0.0f, 1.0f})},
      &VulkanContext::VulkanQueue.GraphicsQueue);
    directShadowDepthPass.SetRenderArea(vk::Rect2D{
      {}, {RendererConfig::Get()->DirectShadowsConfig.Size, RendererConfig::Get()->DirectShadowsConfig.Size},
    }).AddToGraph(renderGraph);

    RenderGraphPass ssaoPass(
      "SSAO Pass",
      {&s_RendererContext.SSAOCommandBuffer},
      &s_Pipelines.SSAOPassPipeline,
      {},
      [](VulkanCommandBuffer& commandBuffer, int32_t) {
        ZoneScopedN("SSAOPass");
        OX_TRACE_GPU(commandBuffer.Get(), "SSAO Pass")
        vk::ImageSubresourceRange subresourceRange;
        subresourceRange.aspectMask = s_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetDesc().AspectFlag;
        subresourceRange.levelCount = 1;
        subresourceRange.layerCount = 1;
        const vk::ImageMemoryBarrier imageMemoryBarrier = {
          {},
          {},
          vk::ImageLayout::eDepthStencilAttachmentOptimal,
          vk::ImageLayout::eDepthStencilReadOnlyOptimal,
          0,
          0,
          s_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetImage(),
          subresourceRange
        };
        commandBuffer.Get().pipelineBarrier(
          vk::PipelineStageFlagBits::eFragmentShader,
          vk::PipelineStageFlagBits::eComputeShader,
          {},
          0,
          nullptr,
          0,
          nullptr,
          1,
          &imageMemoryBarrier
        );
        s_Pipelines.SSAOPassPipeline.BindPipeline(commandBuffer.Get());
        s_Pipelines.SSAOPassPipeline.BindDescriptorSets(commandBuffer.Get(), {s_SSAODescriptorSet.Get()});
        commandBuffer.Dispatch((Window::GetWidth() + 8 - 1) / 8, (Window::GetHeight() + 8 - 1) / 8, 1);
      },
      {clearValues},
      &VulkanContext::VulkanQueue.GraphicsQueue);
    ssaoPass.RunWithCondition(RendererConfig::Get()->SSAOConfig.Enabled).AddToGraphCompute(renderGraph);

    RenderGraphPass pbrPass(
      "PBR Pass",
      {&s_RendererContext.PBRPassCommandBuffer},
      &s_Pipelines.SkyboxPipeline,
      {&s_FrameBuffers.PBRPassFB},
      [](VulkanCommandBuffer& commandBuffer, int32_t) {
        ZoneScopedN("PBRPass");
        OX_TRACE_GPU(commandBuffer.Get(), "PBR Pass")
        commandBuffer.SetViwportWindow().SetScissorWindow();

        //Skybox pass
        s_Pipelines.SkyboxPipeline.BindPipeline(commandBuffer.Get());
        s_Pipelines.SkyboxPipeline.BindDescriptorSets(commandBuffer.Get(), {s_SkyboxDescriptorSet.Get()});
        commandBuffer.PushConstants(s_Pipelines.SkyboxPipeline.GetPipelineLayout(),
          vk::ShaderStageFlagBits::eVertex,
          0,
          sizeof( glm::mat4),
          &s_RendererContext.CurrentCamera->SkyboxView);
        s_SkyboxCube.Draw(commandBuffer.Get());

        //PBR pipeline
        for (const auto& mesh : s_MeshDrawList) {
          if (mesh.MeshGeometry.Nodes.empty())
            continue;

          RenderMesh(mesh,
            commandBuffer.Get(),
            s_Pipelines.PBRPipeline,
            [&](const Mesh::Primitive* part) {
              const auto& material = mesh.Materials[part->materialIndex];
              commandBuffer.Get().pushConstants(s_Pipelines.PBRPipeline.GetPipelineLayout(),
                vk::ShaderStageFlagBits::eVertex,
                0,
                sizeof(glm::mat4),
                &mesh.Transform);
              commandBuffer.Get().pushConstants(s_Pipelines.PBRPipeline.GetPipelineLayout(),
                vk::ShaderStageFlagBits::eFragment,
                sizeof(glm::mat4),
                sizeof Material::Parameters,
                &material->Parameters);

              s_Pipelines.PBRPipeline.BindDescriptorSets(commandBuffer.Get(), {Material::s_DescriptorSet.Get(), material->MaterialDescriptorSet.Get()}, 0, 2);
              return true;
            });
        }
        s_ForceUpdateMaterials = false;
        s_MeshDrawList.clear();

        if (!s_QuadDrawList.empty()) {
          s_Pipelines.UnlitPipeline.BindPipeline(commandBuffer.Get());
          commandBuffer.Get().pushConstants(s_Pipelines.UnlitPipeline.GetPipelineLayout(),
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(glm::mat4) * 2,
            &s_RendererData.UBO_VS);
          //TODO: Add texturing with batching combined.
          //pipeline.BindDescriptorSets(commandBuffer, drawcmd.DescriptorSet.Get());
          s_QuadVertexBuffer.Copy(s_QuadVertexDataBuffer.data(), s_QuadVertexDataBuffer.size());
          DrawQuad(commandBuffer.Get());
        }
        s_QuadDrawList.clear();
      },
      {clearValues},
      &VulkanContext::VulkanQueue.GraphicsQueue);
    pbrPass.AddToGraph(renderGraph);

    RenderGraphPass bloomPass(
      "Bloom Pass",
      {&s_RendererContext.BloomPassCommandBuffer},
      &s_Pipelines.BloomPipeline,
      {},
      [](VulkanCommandBuffer& commandBuffer, int32_t) {
        ZoneScopedN("BloomPass");
        OX_TRACE_GPU(commandBuffer.Get(), "Bloom Pass")
        //const Vec4 Params = {
        //  RendererConfig::Get()->BloomConfig.Threshold, RendererConfig::Get()->BloomConfig.Clamp,
        //  {}, {}
        //};
        //s_RendererData.BloomUBO.Params = Params;
        //s_RendererData.BloomUBO.Stage = {};
        //s_RendererData.BloomBuffer.Copy(&s_RendererData.BloomUBO, sizeof s_RendererData.BloomUBO);
        //s_Pipelines.BloomPipeline.BindPipeline(commandBuffer.Get());
        //IVec3 indexConst{};
        //commandBuffer.PushConstants(s_Pipelines.BloomPipeline.GetPipelineLayout(),
        //  vk::ShaderStageFlagBits::eCompute,
        //  0,
        //  sizeof(indexConst),
        //  &indexConst);

        //s_Pipelines.BloomPipeline.BindDescriptorSets(commandBuffer.Get(), {s_BloomDescriptorSet.Get()});
        ////Downsample
        //IVec3 size = VulkanImage::GetMipMapLevelSize(
        //  s_FrameBuffers.BloomDownsampleImage.GetWidth(), s_FrameBuffers.BloomDownsampleImage.GetHeight(), 1, 0);
        //commandBuffer.Dispatch((size.x + 8 - 1) / 8, (size.y + 8 - 1) / 8, 1);

        //indexConst.y = 1;
        //commandBuffer.PushConstants(s_Pipelines.BloomPipeline.GetPipelineLayout(),
        //  vk::ShaderStageFlagBits::eCompute,
        //  0,
        //  sizeof(indexConst),
        //  &indexConst);
        ////for (int i = 1; i < lodCount; i++) {
        //  size = VulkanImage::GetMipMapLevelSize(
        //    s_FrameBuffers.BloomDownsampleImage.GetWidth(), s_FrameBuffers.BloomDownsampleImage.GetHeight(), 1, i);

        //  //Set lod in shader
        //  s_RendererData.BloomUBO.Stage.y = 0;//i - 1;
        //  s_RendererData.BloomBuffer.Copy(&s_RendererData.BloomUBO, sizeof s_RendererData.BloomUBO);

        //  indexConst.x = 0;
        //commandBuffer.PushConstants(s_Pipelines.BloomPipeline.GetPipelineLayout(),
        //  vk::ShaderStageFlagBits::eCompute,
        //  0,
        //  sizeof(indexConst),
        //  &indexConst);
        //  //downscaleTexture.BindToImageUnit(0, i, false, 0, TextureAccess.WriteOnly, downscaleTexture.SizedInternalFormat);

        //  //GL.MemoryBarrier(MemoryBarrierFlags.TextureFetchBarrierBit);
        //  commandBuffer.Dispatch((size.x + 8 - 1) / 8, (size.y + 8 - 1) / 8, 1);
        ////}

        ////Upsample
        //indexConst.z = 1;
        //s_RendererData.BloomUBO.Stage.x = 1;//i - 1;
        //s_RendererData.BloomBuffer.Copy(&s_RendererData.BloomUBO, sizeof s_RendererData.BloomUBO);

        //size = VulkanImage::GetMipMapLevelSize(s_FrameBuffers.BloomUpsampleImage.GetWidth(), s_FrameBuffers.BloomUpsampleImage.GetHeight(), 1, 0);//lodCount - 2);
        ////shaderProgram.Upload(3, lodCount - 1);
        //indexConst.x = 1;
        //commandBuffer.PushConstants(s_Pipelines.BloomPipeline.GetPipelineLayout(),
        //  vk::ShaderStageFlagBits::eCompute,
        //  0,
        //  sizeof(indexConst),
        //  &indexConst);

        ////upsampleTexture.BindToImageUnit(0, lodCount - 2, false, 0, TextureAccess.WriteOnly, upsampleTexture.SizedInternalFormat);
        ////GL.MemoryBarrier(MemoryBarrierFlags.TextureFetchBarrierBit);
        //commandBuffer.Dispatch((size.x + 8 - 1) / 8, (size.y + 8 - 1) / 8, 1);

        //indexConst.x = 
        //upsampleTexture.BindToUnit(1);
        //commandBuffer.PushConstants(s_Pipelines.BloomPipeline.GetPipelineLayout(),
        //  vk::ShaderStageFlagBits::eCompute,
        //  0,
        //  sizeof(indexConst),
        //  &indexConst);
        //for (int i = lodCount - 3; i >= 0; i--) {
        //  size = Texture.GetMipMapLevelSize(upsampleTexture.Width, upsampleTexture.Height, 1, i);

        //  shaderProgram.Upload(3, i + 1);
        //  upsampleTexture.BindToImageUnit(0, i, false, 0, TextureAccess.WriteOnly, upsampleTexture.SizedInternalFormat);

        //  GL.MemoryBarrier(MemoryBarrierFlags.TextureFetchBarrierBit);
        //  GL.DispatchCompute((size.X + 8 - 1) / 8, (size.Y + 8 - 1) / 8, 1);
        //}
      },
      clearValues,
      &VulkanContext::VulkanQueue.GraphicsQueue);
    /*bloomPass.RunWithCondition(RendererConfig::Get()->BloomConfig.Enabled)
             .AddToGraphCompute(renderGraph);*/

    RenderGraphPass ssrPass(
      "SSR Pass",
      {&s_RendererContext.SSRCommandBuffer},
      &s_Pipelines.SSRPipeline,
      {},
      [](VulkanCommandBuffer& commandBuffer, int32_t) {
        ZoneScopedN("SSR Pass");
        OX_TRACE_GPU(commandBuffer.Get(), "SSR Pass")
        s_Pipelines.SSRPipeline.BindPipeline(commandBuffer.Get());
        s_Pipelines.SSRPipeline.BindDescriptorSets(commandBuffer.Get(), {s_SSRDescriptorSet.Get()});
        commandBuffer.Dispatch((Window::GetWidth() + 8 - 1) / 8, (Window::GetHeight() + 8 - 1) / 8, 1);
      },
      clearValues,
      &VulkanContext::VulkanQueue.GraphicsQueue);
    ssrPass.RunWithCondition(s_RendererConfig.SSRConfig.Enabled)
           .AddToGraphCompute(renderGraph);

    RenderGraphPass compositePass(
      "Composite Pass",
      {&s_RendererContext.CompositeCommandBuffer},
      &s_Pipelines.CompositePipeline,
      {},
      [](VulkanCommandBuffer& commandBuffer, int32_t) {
        ZoneScopedN("Composite Pass");
        OX_TRACE_GPU(commandBuffer.Get(), "Composite Pass")
        s_Pipelines.CompositePipeline.BindPipeline(commandBuffer.Get());
        s_Pipelines.CompositePipeline.BindDescriptorSets(commandBuffer.Get(), {s_CompositeDescriptorSet.Get()});
        commandBuffer.Dispatch((Window::GetWidth() + 8 - 1) / 8, (Window::GetHeight() + 8 - 1) / 8, 1);
      },
      clearValues,
      &VulkanContext::VulkanQueue.GraphicsQueue);
    compositePass.AddToGraphCompute(renderGraph);

    RenderGraphPass ppPass({
      "PP Pass",
      {&s_RendererContext.PostProcessCommandBuffer},
      &s_Pipelines.PostProcessPipeline,
      {&s_FrameBuffers.PostProcessPassFB},
      [](VulkanCommandBuffer& commandBuffer, int32_t) {
        ZoneScopedN("PP Pass");
        OX_TRACE_GPU(commandBuffer.Get(), "PP Pass")
        commandBuffer.SetFlippedViwportWindow().SetScissorWindow();
        s_Pipelines.PostProcessPipeline.BindPipeline(commandBuffer.Get());
        s_Pipelines.PostProcessPipeline.BindDescriptorSets(commandBuffer.Get(), {s_PostProcessDescriptorSet.Get()});
        DrawFullscreenQuad(commandBuffer.Get(), true);
      },
      clearValues, &VulkanContext::VulkanQueue.GraphicsQueue
    });
    ppPass.AddToGraph(renderGraph);

    RenderGraphPass frustumPass(
      "Frustum Pass",
      {&s_RendererContext.FrustumCommandBuffer},
      {},
      {},
      [](const VulkanCommandBuffer& commandBuffer, int32_t) {
        ZoneScopedN("FrustumPass");
        OX_TRACE_GPU(commandBuffer.Get(), "Frustum Pass")
        s_Pipelines.FrustumGridPipeline.BindPipeline(commandBuffer.Get());
        s_Pipelines.FrustumGridPipeline.BindDescriptorSets(s_RendererContext.FrustumCommandBuffer.Get(), {s_ComputeDescriptorSet.Get()});
        commandBuffer.Dispatch(s_RendererData.UBO_PbrPassParams.numThreadGroups.x, s_RendererData.UBO_PbrPassParams.numThreadGroups.y, 1);
      },
      {},
      &VulkanContext::VulkanQueue.GraphicsQueue);
    //renderGraph.AddComputePass(frustumPass);

    RenderGraphPass lightListPass(
      "Light List Pass",
      {s_RendererContext.ComputeCommandBuffers.data()},
      {},
      {},
      [](const VulkanCommandBuffer& commandBuffer, int32_t) {
        ZoneScopedN("Light List Pass");
        OX_TRACE_GPU(commandBuffer.Get(), "Light List Pass")
        std::vector<vk::BufferMemoryBarrier> barriers1;
        std::vector<vk::BufferMemoryBarrier> barriers2;
        static bool initalizedBarries = false;
        if (!initalizedBarries) {
          barriers1 = {
            s_RendererData.LightsBuffer.CreateMemoryBarrier(vk::AccessFlagBits::eShaderRead,
              vk::AccessFlagBits::eShaderWrite),
            s_RendererData.LighIndexBuffer.CreateMemoryBarrier(vk::AccessFlagBits::eShaderRead,
              vk::AccessFlagBits::eShaderWrite),
            s_RendererData.LighGridBuffer.CreateMemoryBarrier(vk::AccessFlagBits::eShaderRead,
              vk::AccessFlagBits::eShaderWrite),
          };

          barriers2 = {
            s_RendererData.LightsBuffer.CreateMemoryBarrier(vk::AccessFlagBits::eShaderWrite,
              vk::AccessFlagBits::eShaderRead),
            s_RendererData.LighIndexBuffer.CreateMemoryBarrier(vk::AccessFlagBits::eShaderWrite,
              vk::AccessFlagBits::eShaderRead),
            s_RendererData.LighGridBuffer.CreateMemoryBarrier(vk::AccessFlagBits::eShaderWrite,
              vk::AccessFlagBits::eShaderRead),
          };
          initalizedBarries = true;
        }

        commandBuffer.Get().pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader,
          vk::PipelineStageFlagBits::eComputeShader,
          vk::DependencyFlagBits::eByRegion,
          0,
          nullptr,
          (uint32_t)barriers1.size(),
          barriers1.data(),
          0,
          nullptr);
        s_Pipelines.LightListPipeline.BindPipeline(commandBuffer.Get());
        s_Pipelines.LightListPipeline.BindDescriptorSets(commandBuffer.Get(), {s_ComputeDescriptorSet.Get()});
        commandBuffer.Dispatch(s_RendererData.UBO_PbrPassParams.numThreadGroups.x,
          s_RendererData.UBO_PbrPassParams.numThreadGroups.y,
          1);
        commandBuffer.Get().pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader,
          vk::PipelineStageFlagBits::eComputeShader,
          vk::DependencyFlagBits::eByRegion,
          0,
          nullptr,
          (uint32_t)barriers2.size(),
          barriers2.data(),
          0,
          nullptr);
      },
      {},
      &VulkanContext::VulkanQueue.GraphicsQueue);

    //renderGraph.AddComputePass(lightListPass);
  }

  void VulkanRenderer::InitQuadVertexBuffer() {
    static constexpr size_t quadVertexCount = 4;
    static constexpr glm::vec4 QuadVertexPositions[4] = {
      {-0.5f, -0.5f, 0.0f, 1.0f}, {0.5f, -0.5f, 0.0f, 1.0f}, {0.5f, 0.5f, 0.0f, 1.0f}, {-0.5f, 0.5f, 0.0f, 1.0f}
    };

    constexpr glm::vec2 textureCoords[] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};

    for (auto& drawcmd : s_QuadDrawList) {
      for (size_t i = 0; i < quadVertexCount; i++) {
        s_QuadVertexDataBuffer[i].Position = drawcmd.Transform * QuadVertexPositions[i];
        s_QuadVertexDataBuffer[i].Color = drawcmd.Color;
        s_QuadVertexDataBuffer[i].UV = textureCoords[i];
        //s_QuadVertexDataBuffer[i].TexIndex = textureIndex;
        //s_QuadVertexDataBuffer[i].TilingFactor = tilingFactor;
      }
    }

    s_QuadVertexDataBuffer.resize(MAX_PARTICLE_COUNT);
    const uint64_t vBufferSize = (uint32_t)s_QuadVertexDataBuffer.size() * sizeof(RendererData::Vertex);

    s_QuadVertexBuffer.CreateBuffer(vk::BufferUsageFlagBits::eVertexBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      vBufferSize,
      s_QuadVertexDataBuffer.data()).Map();
  }

  void VulkanRenderer::FlushQuadVertexBuffer() {
    s_QuadVertexBuffer.Destroy();
    s_QuadVertexDataBuffer.clear();
    s_QuadVertexDataBuffer.resize(MAX_PARTICLE_COUNT);
  }

  void VulkanRenderer::Init() {
    //Save/Load renderer config
    if (!RendererConfig::Get()->LoadConfig("renderer.oxconfig")) {
      RendererConfig::Get()->SaveConfig("renderer.oxconfig");
    }

    const auto& LogicalDevice = VulkanContext::Context.Device;

    constexpr vk::DescriptorPoolSize poolSizes[] = {
      {vk::DescriptorType::eSampler, 50}, {vk::DescriptorType::eCombinedImageSampler, 50},
      {vk::DescriptorType::eSampledImage, 50}, {vk::DescriptorType::eStorageImage, 50},
      {vk::DescriptorType::eUniformTexelBuffer, 50}, {vk::DescriptorType::eStorageTexelBuffer, 50},
      {vk::DescriptorType::eUniformBuffer, 50}, {vk::DescriptorType::eStorageBuffer, 50},
      {vk::DescriptorType::eUniformBufferDynamic, 50}, {vk::DescriptorType::eStorageBufferDynamic, 50},
      {vk::DescriptorType::eInputAttachment, 50}
    };
    vk::DescriptorPoolCreateInfo poolInfo = {};
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = 1000 * IM_ARRAYSIZE(poolSizes);
    poolInfo.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(poolSizes));
    poolInfo.pPoolSizes = poolSizes;
    VulkanUtils::CheckResult(LogicalDevice.createDescriptorPool(&poolInfo, nullptr, &s_RendererContext.DescriptorPool));

    //Command Buffers
    vk::CommandPoolCreateInfo cmdPoolInfo;
    cmdPoolInfo.queueFamilyIndex = VulkanContext::VulkanQueue.graphicsQueueFamilyIndex;
    cmdPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    s_RendererContext.CommandPool = VulkanContext::Context.Device.createCommandPool(cmdPoolInfo).value;

    SwapChain.SetVsync(RendererConfig::Get()->DisplayConfig.VSync, false);
    SwapChain.CreateSwapChain();

    s_RendererContext.TimelineCommandBuffer.CreateBuffer();

    s_RendererContext.PostProcessCommandBuffer.CreateBuffer();
    s_RendererContext.PBRPassCommandBuffer.CreateBuffer();
    s_RendererContext.BloomPassCommandBuffer.CreateBuffer();
    s_RendererContext.SSRCommandBuffer.CreateBuffer();
    s_RendererContext.FrustumCommandBuffer.CreateBuffer();
    s_RendererContext.ComputeCommandBuffers.resize(1);
    s_RendererContext.ComputeCommandBuffers[0].CreateBuffer();
    s_RendererContext.DepthPassCommandBuffer.CreateBuffer();
    s_RendererContext.SSAOCommandBuffer.CreateBuffer();
    s_RendererContext.DirectShadowCommandBuffer.CreateBuffer();
    s_RendererContext.CompositeCommandBuffer.CreateBuffer();

    vk::DescriptorSetLayoutBinding binding[1];
    binding[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    binding[0].descriptorCount = 1;
    binding[0].stageFlags = vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutCreateInfo info = {};
    info.bindingCount = 1;
    info.pBindings = binding;
    VulkanUtils::CheckResult(
      LogicalDevice.createDescriptorSetLayout(&info, nullptr, &s_RendererData.ImageDescriptorSetLayout));

    s_RendererData.SkyboxBuffer.CreateBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
                                             vk::MemoryPropertyFlagBits::eHostVisible |
                                             vk::MemoryPropertyFlagBits::eHostCoherent,
                                             sizeof RendererData::UBO_VS,
                                             &s_RendererData.UBO_VS).Map();

    s_RendererData.ParametersBuffer.CreateBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
                                                 vk::MemoryPropertyFlagBits::eHostVisible |
                                                 vk::MemoryPropertyFlagBits::eHostCoherent,
                                                 sizeof RendererData::UBO_PbrPassParams,
                                                 &s_RendererData.UBO_PbrPassParams).Map();

    s_RendererData.VSBuffer.CreateBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
                                              vk::MemoryPropertyFlagBits::eHostVisible |
                                              vk::MemoryPropertyFlagBits::eHostCoherent,
                                              sizeof RendererData::UBO_VS,
                                              &s_RendererData.UBO_VS).Map();

    s_RendererData.LightsBuffer.CreateBuffer(
      vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      sizeof(LightingData)).Map();

    s_RendererData.FrustumBuffer.CreateBuffer(
      vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      sizeof RendererData::Frustums,
      &s_RendererData.Frustums).Map();

    s_RendererData.LighGridBuffer.CreateBuffer(vk::BufferUsageFlagBits::eStorageBuffer,
                                               vk::MemoryPropertyFlagBits::eHostVisible |
                                               vk::MemoryPropertyFlagBits::eHostCoherent,
                                               sizeof(uint32_t) * MAX_NUM_FRUSTUMS * MAX_NUM_LIGHTS_PER_TILE).Map();

    s_RendererData.LighIndexBuffer.CreateBuffer(vk::BufferUsageFlagBits::eStorageBuffer,
                                                vk::MemoryPropertyFlagBits::eHostVisible |
                                                vk::MemoryPropertyFlagBits::eHostCoherent,
                                                sizeof(uint32_t) * MAX_NUM_FRUSTUMS).Map();

    s_RendererData.BloomBuffer.CreateBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
                                            vk::MemoryPropertyFlagBits::eHostVisible |
                                            vk::MemoryPropertyFlagBits::eHostCoherent,
                                            sizeof(RendererData::BloomUB)).Map();

    s_RendererData.SSRBuffer.CreateBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
                                            vk::MemoryPropertyFlagBits::eHostVisible |
                                            vk::MemoryPropertyFlagBits::eHostCoherent,
                                            sizeof(RendererData::SSRBuffer)).Map();
    
    s_RendererData.SSAOBuffer.CreateBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
                                           vk::MemoryPropertyFlagBits::eHostVisible |
                                           vk::MemoryPropertyFlagBits::eHostCoherent,
                                           sizeof RendererData::UBO_SSAOParams,
                                           &s_RendererData.UBO_SSAOParams).Map();

    //PBR Composite buffer
    {
      s_RendererData.CompositeParametersBuffer.CreateBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible,
        sizeof RendererData::UBO_CompositeParams,
        &s_RendererData.UBO_CompositeParams).Map();
    }

    //Direct shadow buffer
    {
      s_RendererData.DirectShadowBuffer.CreateBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible,
        sizeof RendererData::UBO_DirectShadow,
        &s_RendererData.UBO_DirectShadow).Map();
    }

    //Create Triangle Buffers for rendering a single triangle.
    {
      std::vector<RendererData::Vertex> vertexBuffer = {
        {{-1.0f, -1.0f, 0.0f}, {}, {0.0f, 1.0f}}, {{-1.0f, 3.0f, 0.0f}, {}, {0.0f, -1.0f}},
        {{3.0f, -1.0f, 0.0f}, {}, {2.0f, 1.0f}},
      };

      VulkanBuffer vertexStaging;

      uint64_t vBufferSize = (uint32_t)vertexBuffer.size() * sizeof(RendererData::Vertex);

      vertexStaging.CreateBuffer(vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        vBufferSize,
        vertexBuffer.data());

      s_TriangleVertexBuffer.CreateBuffer(
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        vBufferSize);

      SubmitOnce([&vBufferSize, &vertexStaging](const VulkanCommandBuffer copyCmd) {
        vk::BufferCopy copyRegion{};

        copyRegion.size = vBufferSize;
        vertexStaging.CopyTo(s_TriangleVertexBuffer.Get(), copyCmd.Get(), copyRegion);
      });

      vertexStaging.Destroy();
      vertexStaging.FreeMemory(0);
    }

    InitQuadVertexBuffer();

    //Lights data
    s_PointLightsData.reserve(MAX_NUM_LIGHTS);

    //Mesh data
    s_MeshDrawList.reserve(MAX_NUM_MESHES);

    Resources::InitEngineResources();

    s_SkyboxCube.LoadFromFile("resources/objects/cube.gltf", Mesh::FlipY | Mesh::DontCreateMaterials);

    VulkanImageDescription CubeMapDesc;
    //temp fail-safe until we have an actual atmosphere
    if (std::filesystem::exists("resources/hdrs/industrial_sky.ktx2"))
      CubeMapDesc.Path = ("resources/hdrs/industrial_sky.ktx2");
    else
      CubeMapDesc.Path = "resources/hdrs/belfast_sunset.ktx2";
    CubeMapDesc.Type = ImageType::TYPE_CUBE;
    s_Resources.CubeMap.Create(CubeMapDesc);

    CreateGraphicsPipelines();
    CreateFramebuffers();

    //s_GaussDescriptorSet.CreateFromPipeline(s_Pipelines.GaussianBlurPipeline);
    s_QuadDescriptorSet.CreateFromPipeline(s_Pipelines.QuadPipeline);
    s_SkyboxDescriptorSet.CreateFromPipeline(s_Pipelines.SkyboxPipeline);
    s_ComputeDescriptorSet.CreateFromPipeline(s_Pipelines.LightListPipeline);
    s_SSAODescriptorSet.CreateFromPipeline(s_Pipelines.SSAOPassPipeline);
    s_PostProcessDescriptorSet.CreateFromPipeline(s_Pipelines.PostProcessPipeline);
    //s_BloomDescriptorSet.CreateFromPipeline(s_Pipelines.BloomPipeline);
    s_DepthDescriptorSet.CreateFromPipeline(s_Pipelines.DepthPrePassPipeline);
    s_ShadowDepthDescriptorSet.CreateFromPipeline(s_Pipelines.DirectShadowDepthPipeline);
    s_SSRDescriptorSet.CreateFromPipeline(s_Pipelines.SSRPipeline);
    s_CompositeDescriptorSet.CreateFromPipeline(s_Pipelines.CompositePipeline);

    GeneratePrefilter();

    UpdateSkyboxDescriptorSets();
    UpdateComputeDescriptorSets();
    UpdateSSAODescriptorSets();
    s_FrameBuffers.PBRPassFB.GetDescription().OnResize();
    s_FrameBuffers.PostProcessPassFB.GetDescription().OnResize();
    for (auto& fb : s_FrameBuffers.DirectionalCascadesFB)
      fb.GetDescription().OnResize();

    ShaderLibrary::UnloadShaders();

    s_RendererContext.Initialized = true;

    //Render graph
    InitRenderGraph();

    //Initalize tracy profiling
#if GPU_PROFILER_ENABLED 
    const VkPhysicalDevice physicalDevice = VulkanContext::Context.PhysicalDevice;
    TracyProfiler::InitTracyForVulkan(physicalDevice,
      LogicalDevice,
      VulkanContext::VulkanQueue.GraphicsQueue,
      s_RendererContext.TimelineCommandBuffer.Get());
#endif
  }

  void VulkanRenderer::Shutdown() {
    RendererConfig::Get()->SaveConfig("renderer.oxconfig");
    //TracyProfiler::DestroyContext();
  }

  void VulkanRenderer::Submit(const std::function<void()>& submitFunc) {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    const auto& CommandBuffer = SwapChain.GetCommandBuffer();
    const auto& GraphicsQueue = VulkanContext::VulkanQueue.GraphicsQueue;

    vk::CommandBufferBeginInfo beginInfo = {};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    vk::SubmitInfo endInfo;
    endInfo.pCommandBuffers = &CommandBuffer.Get();
    endInfo.commandBufferCount = 1;

    LogicalDevice.resetCommandPool(s_RendererContext.CommandPool);
    CommandBuffer.Begin(beginInfo);
    submitFunc();
    CommandBuffer.End();
    VulkanUtils::CheckResult(GraphicsQueue.submit(endInfo));
    WaitDeviceIdle();
  }

  void VulkanRenderer::SubmitOnce(const std::function<void(VulkanCommandBuffer& cmdBuffer)>& submitFunc) {
    VulkanCommandBuffer cmdBuffer;
    cmdBuffer.CreateBuffer();
    cmdBuffer.Begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    submitFunc(cmdBuffer);
    cmdBuffer.End();
    cmdBuffer.FlushBuffer();
    cmdBuffer.FreeBuffer();
  }

  void VulkanRenderer::SubmitQueue(const VulkanCommandBuffer& commandBuffer) {
    vk::SubmitInfo end_info = {};
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &commandBuffer.Get();

    VulkanUtils::CheckResult(VulkanContext::VulkanQueue.GraphicsQueue.submit(1, &end_info, VK_NULL_HANDLE));
    WaitDeviceIdle();
  }

  void VulkanRenderer::SubmitLights(std::vector<Entity>&& lights) {
    s_SceneLights = std::move(lights);
  }

  void VulkanRenderer::SubmitSkyLight(const Entity& entity) {
    s_Skylight = entity;
    const auto& skyLight = s_Skylight.GetComponent<SkyLightComponent>();
    if (skyLight.Cubemap && skyLight.Cubemap->LoadCallback) {
      s_Resources.CubeMap = *skyLight.Cubemap;
      s_ForceUpdateMaterials = true;
      skyLight.Cubemap->LoadCallback = false;
    }
  }

  void VulkanRenderer::SubmitMesh(Mesh& mesh, const glm::mat4& transform, std::vector<Ref<Material>>& materials, uint32_t submeshIndex) {
    s_MeshDrawList.emplace_back(mesh, transform, materials,submeshIndex);
  }

  void VulkanRenderer::SubmitQuad(const glm::mat4& transform, const Ref<VulkanImage>& image, const glm::vec4& color) {
    s_QuadDrawList.emplace_back(transform, image, color);
  }

  const VulkanImage& VulkanRenderer::GetFinalImage() {
    return s_FrameBuffers.PostProcessPassFB.GetImage()[0];
  }

  void VulkanRenderer::SetCamera(Camera& camera) {
    s_RendererContext.CurrentCamera = &camera;
  }

  void VulkanRenderer::RenderNode(const Mesh::Node* node,
                                  const vk::CommandBuffer& commandBuffer,
                                  const VulkanPipeline& pipeline,
                                  const std::function<bool(Mesh::Primitive* prim)>& perMeshFunc) {
    for (const auto& part : node->Primitives) {
      if (!perMeshFunc(part))
        continue;
      commandBuffer.drawIndexed(part->indexCount, 1, part->firstIndex, 0, 0);
    }
    for (const auto& child : node->Children) {
      RenderNode(child, commandBuffer, pipeline, perMeshFunc);
    }
  }

  void VulkanRenderer::RenderMesh(const MeshData& mesh,
                                  const vk::CommandBuffer& commandBuffer,
                                  const VulkanPipeline& pipeline,
                                  const std::function<bool(Mesh::Primitive* prim)>& perMeshFunc) {
    pipeline.BindPipeline(commandBuffer);

    if (mesh.MeshGeometry.ShouldUpdate || s_ForceUpdateMaterials) {
      mesh.MeshGeometry.UpdateMaterials();
      mesh.MeshGeometry.ShouldUpdate = false;
      return;
    }

    constexpr vk::DeviceSize offsets[1] = {0};
    commandBuffer.bindVertexBuffers(0, mesh.MeshGeometry.VerticiesBuffer.Get(), offsets);
    commandBuffer.bindIndexBuffer(mesh.MeshGeometry.IndiciesBuffer.Get(), 0, vk::IndexType::eUint32);

    RenderNode(mesh.MeshGeometry.LinearNodes[mesh.SubmeshIndex], commandBuffer, pipeline, perMeshFunc);
  }

  void VulkanRenderer::Draw() {
    ZoneScoped;
    if (Window::IsMinimized())
      return;

    if (!s_RendererContext.CurrentCamera) {
      OX_CORE_ERROR("Renderer couldn't find a camera!");
      return;
    }

    UpdateUniformBuffers();

    if (!s_RendererContext.RenderGraph.Update(SwapChain, &SwapChain.CurrentFrame)) {
      return;
    }

    SwapChain.SubmitPass([](const VulkanCommandBuffer& commandBuffer) {
      ZoneScopedN("Swapchain pass");
      s_Pipelines.QuadPipeline.BindPipeline(commandBuffer.Get());
      s_Pipelines.QuadPipeline.BindDescriptorSets(commandBuffer.Get(), {s_QuadDescriptorSet.Get()});
      DrawFullscreenQuad(commandBuffer.Get());
      //UI pass
      Application::Get().GetImGuiLayer()->RenderDrawData(commandBuffer.Get(), s_Pipelines.UIPipeline.Get());
    });

    SwapChain.Submit();

    SwapChain.Present();
  }

  void VulkanRenderer::DrawFullscreenQuad(const vk::CommandBuffer& commandBuffer, const bool bindVertex) {
    ZoneScoped;
    if (bindVertex) {
      constexpr vk::DeviceSize offsets[1] = {0};
      commandBuffer.bindVertexBuffers(0, s_TriangleVertexBuffer.Get(), offsets);
      commandBuffer.draw(3, 1, 0, 0);
    }
    else {
      commandBuffer.draw(3, 1, 0, 0);
    }
  }
    
  void VulkanRenderer::DrawQuad(const vk::CommandBuffer& commandBuffer) {
    constexpr vk::DeviceSize offsets[1] = {0};
    commandBuffer.bindVertexBuffers(0, s_QuadVertexBuffer.Get(), offsets);
    commandBuffer.draw(MAX_PARTICLE_COUNT, 1, 0, 0);
  }

  void VulkanRenderer::OnResize() {
    SwapChain.Resizing = true;
  }

  void VulkanRenderer::WaitDeviceIdle() {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    VulkanUtils::CheckResult(LogicalDevice.waitIdle());
  }

  void VulkanRenderer::WaitGraphicsQueueIdle() {
    VulkanUtils::CheckResult(VulkanContext::VulkanQueue.GraphicsQueue.waitIdle());
  }
}
