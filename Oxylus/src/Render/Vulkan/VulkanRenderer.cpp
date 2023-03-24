#include "oxpch.h"
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
  VulkanRenderer::RenderPasses VulkanRenderer::s_RenderPasses;
  VulkanRenderer::FrameBuffers VulkanRenderer::s_FrameBuffers;
  RendererConfig VulkanRenderer::s_RendererConfig;
  std::unordered_map<std::string, Ref<VulkanShader>> VulkanRenderer::s_Shaders;

  static VulkanDescriptorSet s_CompositeDescriptorSet;
  static VulkanDescriptorSet s_SkyboxDescriptorSet;
  static VulkanDescriptorSet s_ComputeDescriptorSet;
  static VulkanDescriptorSet s_SSAODescriptorSet;
  static VulkanDescriptorSet s_SSAOVBlurDescriptorSet;
  static VulkanDescriptorSet s_SSAOHBlurDescriptorSet;
  static VulkanDescriptorSet s_QuadDescriptorSet;
  static VulkanDescriptorSet s_GaussDescriptorSet;
  static VulkanDescriptorSet s_DepthDescriptorSet;
  static VulkanDescriptorSet s_ShadowDepthDescriptorSet;
  static VulkanDescriptorSet s_BloomPrefilterDescriptorSet;
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
    s_RendererData.UboVS.projection = s_RendererContext.CurrentCamera->GetProjectionMatrixFlipped();
    s_RendererData.SkyboxBuffer.Copy(&s_RendererData.UboVS, sizeof s_RendererData.UboVS);

    s_RendererData.UboVS.projection = s_RendererContext.CurrentCamera->GetProjectionMatrixFlipped();
    s_RendererData.UboVS.view = s_RendererContext.CurrentCamera->GetViewMatrix();
    s_RendererData.UboVS.camPos = s_RendererContext.CurrentCamera->GetPosition();
    s_RendererData.ObjectsBuffer.Copy(&s_RendererData.UboVS, sizeof s_RendererData.UboVS);

    s_RendererData.UboParams.numLights = 1;
    s_RendererData.UboParams.numThreads = (glm::ivec2(Window::GetWidth(), Window::GetHeight()) + PIXELS_PER_TILE - 1) /
                                          PIXELS_PER_TILE;
    s_RendererData.UboParams.numThreadGroups = (s_RendererData.UboParams.numThreads + TILES_PER_THREADGROUP - 1) /
                                               TILES_PER_THREADGROUP;
    s_RendererData.UboParams.screenDimensions = glm::ivec2(Window::GetWidth(), Window::GetHeight());

    s_RendererData.ParametersBuffer.Copy(&s_RendererData.UboParams, sizeof s_RendererData.UboParams);

    UpdateLightingData();

    //TODO: Both not needed to be copied every frame.
    //Currently only radius is copied.
    s_RendererData.SSAOParams.radius = RendererConfig::Get()->SSAOConfig.Radius;
    s_RendererData.SSAOBuffer.Copy(&s_RendererData.SSAOParams, sizeof s_RendererData.SSAOParams);

    //Composite buffer
    s_RendererData.UBOCompositeParameters.Tonemapper = RendererConfig::Get()->ColorConfig.Tonemapper;
    s_RendererData.UBOCompositeParameters.Exposure = RendererConfig::Get()->ColorConfig.Exposure;
    s_RendererData.UBOCompositeParameters.Gamma = RendererConfig::Get()->ColorConfig.Gamma;
    s_RendererData.UBOCompositeParameters.EnableSSAO = RendererConfig::Get()->SSAOConfig.Enabled;
    s_RendererData.UBOCompositeParameters.EnableBloom = RendererConfig::Get()->BloomConfig.Enabled;
    s_RendererData.UBOCompositeParameters.VignetteColor.a = RendererConfig::Get()->VignetteConfig.Intensity;
    s_RendererData.UBOCompositeParameters.VignetteOffset.w = RendererConfig::Get()->VignetteConfig.Enabled;
    s_RendererData.CompositeParametersBuffer.Copy(&s_RendererData.UBOCompositeParameters,
      sizeof s_RendererData.UBOCompositeParameters);
  }

  Ref<VulkanShader> VulkanRenderer::GetShaderByID(const UUID& id) {
    for (auto& [name, shader] : GetShaders()) {
      if (shader->GetID() == id) {
        return shader;
      }
    }
    OX_CORE_ERROR("Could not find shader with id: {}", id);
    return {};
  }

  void VulkanRenderer::UnloadShaders() {
    for (auto& [name, shader] : s_Shaders) {
      shader->Unload();
    }
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
    pipelineDescription.RasterizerDesc.CullMode = vk::CullModeFlagBits::eNone;
    pipelineDescription.RasterizerDesc.DepthBias = false;
    pipelineDescription.RasterizerDesc.FrontCounterClockwise = true;
    pipelineDescription.RasterizerDesc.DepthClamppEnable = false;
    pipelineDescription.DepthSpec.DepthWriteEnable = false;
    pipelineDescription.DepthSpec.DepthEnable = true;
    pipelineDescription.DepthSpec.CompareOp = vk::CompareOp::eLessOrEqual;
    pipelineDescription.DepthSpec.FrontFace.StencilFunc = vk::CompareOp::eNever;
    pipelineDescription.DepthSpec.BackFace.StencilFunc = vk::CompareOp::eNever;
    pipelineDescription.DepthSpec.MinDepthBound = 0;
    pipelineDescription.DepthSpec.MaxDepthBound = 0;
    pipelineDescription.DynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    pipelineDescription.RenderPass = s_RenderPasses.PBRRenderPass;
    pipelineDescription.VertexInputState = VertexInputDescription(VertexLayout({
      VertexComponent::POSITION, VertexComponent::NORMAL, VertexComponent::UV
    }));
    pipelineDescription.PushConstantRanges = {
      vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4)}
    };
    pipelineDescription.DescriptorSetLayoutBindings = {
      {
        {0, vDT::eUniformBuffer, 1, vSS::eVertex}, {1, vDT::eUniformBuffer, 1, vSS::eFragment},
        {6, vDT::eCombinedImageSampler, 1, vSS::eFragment},
      }
    };
    s_Pipelines.SkyboxPipeline.CreateGraphicsPipeline(pipelineDescription);

    pipelineDescription.DescriptorSetLayoutBindings = {
      {
        {0, vDT::eUniformBuffer, 1, vSS::eFragment | vSS::eCompute | vSS::eVertex},
        {1, vDT::eUniformBuffer, 1, vSS::eFragment | vSS::eCompute | vSS::eVertex},
        {2, vDT::eStorageBuffer, 1, vSS::eFragment | vSS::eCompute | vSS::eVertex},
        {3, vDT::eStorageBuffer, 1, vSS::eFragment | vSS::eCompute},
        {4, vDT::eStorageBuffer, 1, vSS::eFragment | vSS::eCompute},
        {5, vDT::eStorageBuffer, 1, vSS::eFragment | vSS::eCompute}, {6, vDT::eCombinedImageSampler, 1, vSS::eFragment},
        {7, vDT::eCombinedImageSampler, 1, vSS::eFragment}, {8, vDT::eCombinedImageSampler, 1, vSS::eFragment},
        {9, vDT::eCombinedImageSampler, 1, vSS::eFragment | vSS::eCompute},
        {10, vDT::eCombinedImageSampler, 1, vSS::eFragment}, {11, vDT::eUniformBuffer, 1, vSS::eFragment},
      },
      {
        {0, vDT::eCombinedImageSampler, 1, vSS::eFragment}, {1, vDT::eCombinedImageSampler, 1, vSS::eFragment},
        {2, vDT::eCombinedImageSampler, 1, vSS::eFragment}, {3, vDT::eCombinedImageSampler, 1, vSS::eFragment},
        {4, vDT::eCombinedImageSampler, 1, vSS::eFragment},
      }
    };
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
      unlitPipelineDesc.RenderPass = s_RenderPasses.PBRRenderPass;
      unlitPipelineDesc.RasterizerDesc.CullMode = vk::CullModeFlagBits::eBack;
      unlitPipelineDesc.VertexInputState = VertexInputDescription(VertexLayout({
        VertexComponent::POSITION, VertexComponent::NORMAL, VertexComponent::UV, VertexComponent::COLOR
      }));
      unlitPipelineDesc.BlendStateDesc.RenderTargets[0].BlendEnable = true;
      unlitPipelineDesc.BlendStateDesc.RenderTargets[0].DestBlend = vk::BlendFactor::eOneMinusSrcAlpha;

      s_Pipelines.UnlitPipeline.CreateGraphicsPipeline(unlitPipelineDesc);
    }

    pipelineDescription.RenderPass = s_RenderPasses.DepthNormalRP;
    pipelineDescription.DepthSpec.CompareOp = vk::CompareOp::eLess;
    pipelineDescription.Shader = CreateRef<VulkanShader>(ShaderCI{
      .VertexPath = "resources/shaders/DepthNormalPass.vert", .FragmentPath = "resources/shaders/DepthNormalPass.frag",
      .EntryPoint = "main", .Name = "DepthPass"
    });
    pipelineDescription.DescriptorSetLayoutBindings = {{{0, vDT::eUniformBuffer, 1, vSS::eVertex}}};
    pipelineDescription.DepthSpec.DepthWriteEnable = true;
    pipelineDescription.DepthSpec.DepthEnable = true;
    pipelineDescription.DepthSpec.BoundTest = true;
    pipelineDescription.DepthSpec.MinDepthBound = 0;
    pipelineDescription.DepthSpec.MaxDepthBound = 1;
    pipelineDescription.RasterizerDesc.CullMode = vk::CullModeFlagBits::eBack;
    s_Pipelines.DepthPrePassPipeline.CreateGraphicsPipeline(pipelineDescription);

    pipelineDescription.Shader = CreateRef<VulkanShader>(ShaderCI{
      .VertexPath = "resources/shaders/DirectShadowDepthPass.vert",
      .FragmentPath = "resources/shaders/DirectShadowDepthPass.frag", .EntryPoint = "main", .Name = "DirectShadowDepth"
    });
    pipelineDescription.RenderPass = s_RenderPasses.DirectShadowDepthRP;
    pipelineDescription.PushConstantRanges = {vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex, 0, 68}};
    pipelineDescription.RasterizerDesc.CullMode = vk::CullModeFlagBits::eNone;
    pipelineDescription.RasterizerDesc.DepthClamppEnable = true;
    pipelineDescription.RasterizerDesc.FrontCounterClockwise = false;
    pipelineDescription.DepthSpec.BoundTest = false;
    pipelineDescription.DepthSpec.CompareOp = vk::CompareOp::eLessOrEqual;
    pipelineDescription.DepthSpec.MaxDepthBound = 0;
    pipelineDescription.DepthSpec.BackFace.StencilFunc = vk::CompareOp::eAlways;
    pipelineDescription.DescriptorSetLayoutBindings = {
      {{0, vDT::eUniformBuffer, 1, vSS::eVertex},},
      {
        {0, vDT::eCombinedImageSampler, 1, vSS::eFragment}, {1, vDT::eCombinedImageSampler, 1, vSS::eFragment},
        {2, vDT::eCombinedImageSampler, 1, vSS::eFragment}, {3, vDT::eCombinedImageSampler, 1, vSS::eFragment},
        {4, vDT::eCombinedImageSampler, 1, vSS::eFragment},
      }
    };
    s_Pipelines.DirectShadowDepthPipeline.CreateGraphicsPipeline(pipelineDescription);

    PipelineDescription ssaoDescription;
    ssaoDescription.ColorAttachmentCount = 1;
    ssaoDescription.RenderTargets[0].Format = vk::Format::eR8Unorm;
    ssaoDescription.RenderTargets[0].LoadOp = vk::AttachmentLoadOp::eClear;
    ssaoDescription.RenderTargets[0].StoreOp = vk::AttachmentStoreOp::eStore;
    ssaoDescription.RenderTargets[0].InitialLayout = vk::ImageLayout::eUndefined;
    ssaoDescription.RenderTargets[0].FinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    ssaoDescription.DepthSpec.DepthEnable = false;
    ssaoDescription.DescriptorSetLayoutBindings = {
      {
        {0, vDT::eUniformBuffer, 1, vSS::eFragment}, {1, vDT::eUniformBuffer, 1, vSS::eFragment},
        {2, vDT::eCombinedImageSampler, 1, vSS::eFragment}, {3, vDT::eCombinedImageSampler, 1, vSS::eFragment},
        {4, vDT::eCombinedImageSampler, 1, vSS::eFragment},
      }
    };
    ssaoDescription.RenderPass = s_RenderPasses.SSAORenderPass;
    ssaoDescription.Shader = CreateRef<VulkanShader>(ShaderCI{
      .VertexPath = "resources/shaders/FlatColor.vert", .FragmentPath = "resources/shaders/ssao.frag",
      .EntryPoint = "main", .Name = "SSAO"
    });
    ssaoDescription.RasterizerDesc.CullMode = vk::CullModeFlagBits::eNone;
    ssaoDescription.VertexInputState = VertexInputDescription(VertexLayout({
      VertexComponent::POSITION, VertexComponent::NORMAL, VertexComponent::UV
    }));
    s_Pipelines.SSAOPassPipeline.CreateGraphicsPipeline(ssaoDescription);

    PipelineDescription ssaoBlurDescription;
    ssaoBlurDescription.DescriptorSetLayoutBindings = {
      {
        {0, vDT::eCombinedImageSampler, 1, vSS::eFragment}, {1, vDT::eCombinedImageSampler, 1, vSS::eFragment},
        {2, vDT::eCombinedImageSampler, 1, vSS::eFragment},
      }
    };
    ssaoBlurDescription.PushConstantRanges = {
      vk::PushConstantRange{vk::ShaderStageFlagBits::eFragment, 0, sizeof glm::vec4()}
    };
    ssaoBlurDescription.RenderPass = s_RenderPasses.SSAOHBlurRP;
    ssaoBlurDescription.Shader = CreateRef<VulkanShader>(ShaderCI{
      .VertexPath = "resources/shaders/FlatColor.vert", .FragmentPath = "resources/shaders/SsaoBlur.frag",
      .EntryPoint = "main", .Name = "SSAOBlur"
    });
    ssaoBlurDescription.RasterizerDesc.CullMode = vk::CullModeFlagBits::eNone;
    ssaoBlurDescription.VertexInputState = VertexInputDescription(VertexLayout({
      VertexComponent::POSITION, VertexComponent::NORMAL, VertexComponent::UV
    }));
    s_Pipelines.SSAOHBlurPassPipeline.CreateGraphicsPipeline(ssaoBlurDescription);
    ssaoBlurDescription.RenderPass = s_RenderPasses.SSAOVBlurRP;
    s_Pipelines.SSAOVBlurPassPipeline.CreateGraphicsPipeline(ssaoBlurDescription);

    {
      PipelineDescription gaussianBlurDesc;
      gaussianBlurDesc.ColorAttachmentCount = 1;
      gaussianBlurDesc.DepthSpec.DepthEnable = false;
      gaussianBlurDesc.DescriptorSetLayoutBindings = {{{0, vDT::eCombinedImageSampler, 1, vSS::eFragment},}};
      gaussianBlurDesc.PushConstantRanges = {
        vk::PushConstantRange{vk::ShaderStageFlagBits::eFragment, 0, sizeof(int32_t)}
      };
      gaussianBlurDesc.Shader = CreateRef<VulkanShader>(ShaderCI{
        .VertexPath = "resources/shaders/FlatColor.vert", .FragmentPath = "resources/shaders/GaussianBlur.frag",
        .EntryPoint = "main", .Name = "GaussianBlur"
      });
      gaussianBlurDesc.RasterizerDesc.CullMode = vk::CullModeFlagBits::eNone;
      gaussianBlurDesc.VertexInputState = VertexInputDescription(VertexLayout({
        VertexComponent::POSITION, VertexComponent::NORMAL, VertexComponent::UV
      }));
      s_Pipelines.GaussianBlurPipeline.CreateGraphicsPipeline(gaussianBlurDesc);

      PipelineDescription bloomDesc;
      bloomDesc.ColorAttachmentCount = 1;
      bloomDesc.DepthSpec.DepthEnable = false;
      bloomDesc.DescriptorSetLayoutBindings = {
        {
          {0, vDT::eUniformBuffer, 1, vSS::eFragment}, {1, vDT::eCombinedImageSampler, 1, vSS::eFragment},
          {2, vDT::eCombinedImageSampler, 1, vSS::eFragment}
        }
      };
      bloomDesc.VertexInputState = VertexInputDescription(VertexLayout({
        VertexComponent::POSITION, VertexComponent::NORMAL, VertexComponent::UV
      }));
      bloomDesc.Shader = CreateRef<VulkanShader>(ShaderCI{
        .VertexPath = "resources/shaders/FlatColor.vert", .FragmentPath = "resources/shaders/Bloom.frag",
        .EntryPoint = "main", .Name = "Bloom"
      });
      bloomDesc.RasterizerDesc.CullMode = vk::CullModeFlagBits::eNone;
      s_Pipelines.BloomPipeline.CreateGraphicsPipeline(bloomDesc);
    }

    PipelineDescription pbrCompositePass;
    pbrCompositePass.DescriptorSetLayoutBindings = {
      {
        {0, vDT::eCombinedImageSampler, 1, vSS::eFragment}, {1, vDT::eCombinedImageSampler, 1, vSS::eFragment},
        {2, vDT::eUniformBuffer, 1, vSS::eFragment},
      }
    };
    pbrCompositePass.RenderPass = s_RenderPasses.CompositeRP;
    pbrCompositePass.Shader = CreateRef<VulkanShader>(ShaderCI{
      .VertexPath = "resources/shaders/PbrComposite.vert", .FragmentPath = "resources/shaders/PbrComposite.frag",
      .EntryPoint = "main", .Name = "PBRComposite"
    });
    pbrCompositePass.RasterizerDesc.CullMode = vk::CullModeFlagBits::eNone;
    pbrCompositePass.VertexInputState = VertexInputDescription(VertexLayout({
      VertexComponent::POSITION, VertexComponent::NORMAL, VertexComponent::UV
    }));
    s_Pipelines.CompositePipeline.CreateGraphicsPipeline(pbrCompositePass);

    PipelineDescription quadDescription;
    quadDescription.DescriptorSetLayoutBindings = {
      {{7, vDT::eCombinedImageSampler, 1, vSS::eFragment | vSS::eCompute}}
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

    PipelineDescription computePipelineDesc;
    computePipelineDesc.DescriptorSetLayoutBindings = {
      {
        {0, vDT::eUniformBuffer, 1, vSS::eCompute}, {1, vDT::eUniformBuffer, 1, vSS::eCompute},
        {2, vDT::eStorageBuffer, 1, vSS::eCompute}, {3, vDT::eStorageBuffer, 1, vSS::eCompute},
        {4, vDT::eStorageBuffer, 1, vSS::eCompute}, {5, vDT::eStorageBuffer, 1, vSS::eCompute},
        {6, vDT::eCombinedImageSampler, 1, vSS::eCompute},
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
  }

  void VulkanRenderer::CreateRenderPasses() {
    {
      std::array<vk::AttachmentDescription, 2> attachmentDescription{};
      attachmentDescription[0].format = vk::Format::eD32Sfloat;
      attachmentDescription[0].samples = vk::SampleCountFlagBits::e1;
      attachmentDescription[0].loadOp = vk::AttachmentLoadOp::eClear;
      attachmentDescription[0].storeOp = vk::AttachmentStoreOp::eStore;
      attachmentDescription[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
      attachmentDescription[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
      attachmentDescription[0].initialLayout = vk::ImageLayout::eUndefined;
      attachmentDescription[0].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

      attachmentDescription[1].format = SwapChain.m_SurfaceFormat.format;
      attachmentDescription[1].samples = vk::SampleCountFlagBits::e1;
      attachmentDescription[1].loadOp = vk::AttachmentLoadOp::eClear;
      attachmentDescription[1].storeOp = vk::AttachmentStoreOp::eStore;
      attachmentDescription[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
      attachmentDescription[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
      attachmentDescription[1].initialLayout = vk::ImageLayout::eUndefined;
      attachmentDescription[1].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

      vk::AttachmentReference depthReference;
      depthReference.attachment = 0;
      depthReference.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
      std::array<vk::AttachmentReference, 1> colorReferences;
      colorReferences[0] = {1, vk::ImageLayout::eColorAttachmentOptimal};

      std::array<vk::SubpassDescription, 1> subpasses;

      subpasses[0].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
      subpasses[0].colorAttachmentCount = (uint32_t)colorReferences.size();
      subpasses[0].pColorAttachments = colorReferences.data();
      subpasses[0].pDepthStencilAttachment = &depthReference;

      std::array<vk::SubpassDependency, 2> dependencies;
      dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
      dependencies[0].dstSubpass = 0;
      dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
      dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests;
      dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
      dependencies[0].dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead |
                                      vk::AccessFlagBits::eDepthStencilAttachmentWrite;
      dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

      dependencies[1].srcSubpass = 0;
      dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
      dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
      dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
      dependencies[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
      dependencies[1].dstAccessMask = vk::AccessFlagBits::eShaderRead;
      dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

      vk::RenderPassCreateInfo renderPassCreateInfo = {};
      renderPassCreateInfo.attachmentCount = (uint32_t)attachmentDescription.size();
      renderPassCreateInfo.pAttachments = attachmentDescription.data();
      renderPassCreateInfo.subpassCount = (uint32_t)subpasses.size();
      renderPassCreateInfo.pSubpasses = subpasses.data();
      renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
      renderPassCreateInfo.pDependencies = dependencies.data();

      s_RenderPasses.DepthNormalRP.CreateRenderPass(renderPassCreateInfo);
    }
    {
      //Direct shadow depth render pass
      std::array<vk::AttachmentDescription, 1> attachmentDescription{};
      attachmentDescription[0].format = vk::Format::eD32Sfloat;
      attachmentDescription[0].samples = vk::SampleCountFlagBits::e1;
      attachmentDescription[0].loadOp = vk::AttachmentLoadOp::eClear;
      attachmentDescription[0].storeOp = vk::AttachmentStoreOp::eStore;
      attachmentDescription[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
      attachmentDescription[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
      attachmentDescription[0].initialLayout = vk::ImageLayout::eUndefined;
      attachmentDescription[0].finalLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;

      vk::AttachmentReference depthReference;
      depthReference.attachment = 0;
      depthReference.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

      std::array<vk::SubpassDescription, 1> subpasses;

      subpasses[0].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
      subpasses[0].pDepthStencilAttachment = &depthReference;

      std::array<vk::SubpassDependency, 2> dependencies;
      dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
      dependencies[0].dstSubpass = 0;
      dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eFragmentShader;
      dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests;
      dependencies[0].srcAccessMask = vk::AccessFlagBits::eShaderRead;
      dependencies[0].dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
      dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

      dependencies[1].srcSubpass = 0;
      dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
      dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests;
      dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
      dependencies[1].srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
      dependencies[1].dstAccessMask = vk::AccessFlagBits::eShaderRead;
      dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

      vk::RenderPassCreateInfo renderPassCreateInfo = {};
      renderPassCreateInfo.attachmentCount = (uint32_t)attachmentDescription.size();
      renderPassCreateInfo.pAttachments = attachmentDescription.data();
      renderPassCreateInfo.subpassCount = (uint32_t)subpasses.size();
      renderPassCreateInfo.pSubpasses = subpasses.data();
      renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
      renderPassCreateInfo.pDependencies = dependencies.data();

      s_RenderPasses.DirectShadowDepthRP.CreateRenderPass(renderPassCreateInfo);
    }
    {
      std::array<vk::AttachmentDescription, 2> attachments;
      attachments[0].format = SwapChain.m_SurfaceFormat.format;
      attachments[0].samples = vk::SampleCountFlagBits::e1;
      attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
      attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
      attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
      attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
      attachments[0].initialLayout = vk::ImageLayout::eUndefined;
      attachments[0].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

      attachments[1].format = SwapChain.m_DepthFormat;
      attachments[1].samples = vk::SampleCountFlagBits::e1;
      attachments[1].loadOp = vk::AttachmentLoadOp::eClear;
      attachments[1].storeOp = vk::AttachmentStoreOp::eStore;
      attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eClear;
      attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
      attachments[1].initialLayout = vk::ImageLayout::eUndefined;
      attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

      vk::AttachmentReference depthReference;
      depthReference.attachment = 1;
      depthReference.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

      std::vector<vk::AttachmentReference> colorReferences;
      {
        vk::AttachmentReference colorReference;
        colorReference.attachment = 0;
        colorReference.layout = vk::ImageLayout::eColorAttachmentOptimal;
        colorReferences.push_back(colorReference);
      }

      using vPSFB = vk::PipelineStageFlagBits;
      using vAFB = vk::AccessFlagBits;
      const std::vector<vk::SubpassDependency> subpassDependencies{
        {
          0, VK_SUBPASS_EXTERNAL, vPSFB::eColorAttachmentOutput, vPSFB::eBottomOfPipe,
          vAFB::eColorAttachmentRead | vAFB::eColorAttachmentWrite, vAFB::eMemoryRead, vk::DependencyFlagBits::eByRegion
        },
        {
          VK_SUBPASS_EXTERNAL, 0, vPSFB::eBottomOfPipe, vPSFB::eColorAttachmentOutput, vAFB::eMemoryRead,
          vAFB::eColorAttachmentRead | vAFB::eColorAttachmentWrite, vk::DependencyFlagBits::eByRegion
        },
      };

      std::array<vk::SubpassDescription, 1> subpasses;

      subpasses[0].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
      subpasses[0].colorAttachmentCount = (uint32_t)colorReferences.size();
      subpasses[0].pColorAttachments = colorReferences.data();
      subpasses[0].pDepthStencilAttachment = &depthReference;

      vk::RenderPassCreateInfo renderPassInfo;
      renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
      renderPassInfo.pAttachments = attachments.data();
      renderPassInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
      renderPassInfo.pSubpasses = subpasses.data();
      renderPassInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
      renderPassInfo.pDependencies = subpassDependencies.data();
      s_RenderPasses.PBRRenderPass.CreateRenderPass(renderPassInfo);
    }
    {
      PipelineDescription desc;
      desc.ColorAttachmentCount = 1;
      desc.RenderTargets[0].Format = vk::Format::eR8Unorm;
      desc.RenderTargets[0].LoadOp = vk::AttachmentLoadOp::eClear;
      desc.RenderTargets[0].StoreOp = vk::AttachmentStoreOp::eStore;
      desc.RenderTargets[0].InitialLayout = vk::ImageLayout::eUndefined;
      desc.RenderTargets[0].FinalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
      desc.DepthSpec.DepthEnable = false;

      std::array<vk::AttachmentDescription, 1> attachments;
      attachments[0].format = vk::Format::eR8Unorm;
      attachments[0].samples = vk::SampleCountFlagBits::e1;
      attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
      attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
      attachments[0].initialLayout = vk::ImageLayout::eUndefined;
      attachments[0].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

      std::vector<vk::AttachmentReference> colorReferences;
      {
        vk::AttachmentReference colorReference;
        colorReference.attachment = 0;
        colorReference.layout = vk::ImageLayout::eColorAttachmentOptimal;
        colorReferences.push_back(colorReference);
      }

      std::array<vk::SubpassDescription, 1> subpasses;

      subpasses[0].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
      subpasses[0].colorAttachmentCount = (uint32_t)colorReferences.size();
      subpasses[0].pColorAttachments = colorReferences.data();

      std::vector<vk::SubpassDependency> subpassDependencies;
      subpassDependencies.clear();

      vk::SubpassDependency subpassDependency;
      subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
      subpassDependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
      subpassDependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
      subpassDependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
      subpassDependencies.emplace_back(subpassDependency);

      vk::RenderPassCreateInfo renderPassInfo;
      renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
      renderPassInfo.pAttachments = attachments.data();
      renderPassInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
      renderPassInfo.pSubpasses = subpasses.data();
      renderPassInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
      renderPassInfo.pDependencies = subpassDependencies.data();
      s_RenderPasses.SSAORenderPass.CreateRenderPass(renderPassInfo);
    }

    std::array<vk::AttachmentDescription, 1> attachments;
    attachments[0].format = vk::Format::eR8Unorm;
    attachments[0].samples = vk::SampleCountFlagBits::e1;
    attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
    attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
    attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[0].initialLayout = vk::ImageLayout::eUndefined;
    attachments[0].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    std::vector<vk::AttachmentReference> colorReferences;
    {
      vk::AttachmentReference colorReference;
      colorReference.attachment = 0;
      colorReference.layout = vk::ImageLayout::eColorAttachmentOptimal;
      colorReferences.push_back(colorReference);
    }

    std::array<vk::SubpassDescription, 1> subpasses;

    subpasses[0].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpasses[0].colorAttachmentCount = (uint32_t)colorReferences.size();
    subpasses[0].pColorAttachments = colorReferences.data();

    std::vector<vk::SubpassDependency> subpassDependencies;
    subpassDependencies.clear();

    vk::SubpassDependency subpassDependency;
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    subpassDependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    subpassDependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    subpassDependencies.emplace_back(subpassDependency);

    {
      vk::RenderPassCreateInfo renderPassInfo;
      renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
      renderPassInfo.pAttachments = attachments.data();
      renderPassInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
      renderPassInfo.pSubpasses = subpasses.data();
      renderPassInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
      renderPassInfo.pDependencies = subpassDependencies.data();
      s_RenderPasses.SSAOHBlurRP.CreateRenderPass(renderPassInfo);
      s_RenderPasses.SSAOVBlurRP.CreateRenderPass(renderPassInfo);
    }
    {
      attachments[0].format = vk::Format::eR32G32B32A32Sfloat;
      vk::RenderPassCreateInfo renderPassInfo;
      renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
      renderPassInfo.pAttachments = attachments.data();
      renderPassInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
      renderPassInfo.pSubpasses = subpasses.data();
      renderPassInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
      renderPassInfo.pDependencies = subpassDependencies.data();
      s_RenderPasses.CompositeRP.CreateRenderPass(renderPassInfo);
    }
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
      VulkanImageDescription depthFBImage{};
      depthFBImage.Format = vk::Format::eD32Sfloat;
      depthFBImage.Height = SwapChain.m_Extent.height;
      depthFBImage.Width = SwapChain.m_Extent.width;
      depthFBImage.UsageFlags = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
      depthFBImage.ImageTiling = vk::ImageTiling::eOptimal;
      depthFBImage.CreateSampler = true;
      depthFBImage.CreateDescriptorSet = true;
      depthFBImage.DescriptorSetLayout = s_RendererData.ImageDescriptorSetLayout;
      depthFBImage.CreateView = true;
      depthFBImage.AspectFlag = vk::ImageAspectFlagBits::eDepth;
      depthFBImage.FinalImageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
      FramebufferDescription framebufferDescription;
      framebufferDescription.DebugName = "Depth Pass";
      framebufferDescription.RenderPass = s_RenderPasses.DepthNormalRP.Get();
      framebufferDescription.Width = SwapChain.m_Extent.width;
      framebufferDescription.Height = SwapChain.m_Extent.height;
      framebufferDescription.Extent = &Window::GetWindowExtent();
      framebufferDescription.ImageDescription = {depthFBImage, colorImageDesc};
      framebufferDescription.OnResize = [] {
        const auto& LogicalDevice = VulkanContext::Context.Device;
        LogicalDevice.updateDescriptorSets({
            {
              s_DepthDescriptorSet.Get(), 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr,
              &s_RendererData.ObjectsBuffer.GetDescriptor()
            }
          },
          nullptr);
        UpdateComputeDescriptorSets();
      };
      s_FrameBuffers.DepthNormalPassFB.CreateFramebuffer(framebufferDescription);

      //Direct shadow depth pass
      depthFBImage.Format = vk::Format::eD32Sfloat;
      depthFBImage.Width = RendererConfig::Get()->DirectShadowsConfig.Size;
      depthFBImage.Height = RendererConfig::Get()->DirectShadowsConfig.Size;
      framebufferDescription.Extent = nullptr;
      depthFBImage.Type = ImageType::TYPE_2DARRAY;
      depthFBImage.ImageArrayLayerCount = SHADOW_MAP_CASCADE_COUNT;
      depthFBImage.ViewArrayLayerCount = SHADOW_MAP_CASCADE_COUNT;
      depthFBImage.FinalImageLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
      depthFBImage.TransitionLayoutAtCreate = true;
      depthFBImage.BaseArrayLayerIndex = 0;
      s_Resources.DirectShadowsDepthArray.Create(depthFBImage);

      framebufferDescription.Width = RendererConfig::Get()->DirectShadowsConfig.Size;
      framebufferDescription.Height = RendererConfig::Get()->DirectShadowsConfig.Size;
      framebufferDescription.DebugName = "Direct Shadow Depth Pass";
      framebufferDescription.RenderPass = s_RenderPasses.DirectShadowDepthRP.Get();
      framebufferDescription.OnResize = [] {
        const auto& LogicalDevice = VulkanContext::Context.Device;
        LogicalDevice.updateDescriptorSets({
            {
              s_ShadowDepthDescriptorSet.Get(), 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr,
              &s_RendererData.DirectShadowBuffer.GetDescriptor()
            },
          },
          nullptr);
      };
      s_FrameBuffers.DirectionalCascadesFB.resize(SHADOW_MAP_CASCADE_COUNT);
      int baseArrayLayer = 0;
      depthFBImage.ViewArrayLayerCount = 1;
      for (auto& fb : s_FrameBuffers.DirectionalCascadesFB) {
        depthFBImage.BaseArrayLayerIndex = baseArrayLayer;
        framebufferDescription.ImageDescription = {depthFBImage};
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
      DepthImageDesc.Format = SwapChain.m_DepthFormat;
      DepthImageDesc.UsageFlags = vk::ImageUsageFlagBits::eDepthStencilAttachment |
                                  vk::ImageUsageFlagBits::eInputAttachment;
      DepthImageDesc.ImageTiling = vk::ImageTiling::eOptimal;
      DepthImageDesc.Width = SwapChain.m_Extent.width;
      DepthImageDesc.Height = SwapChain.m_Extent.height;
      DepthImageDesc.CreateView = true;
      DepthImageDesc.CreateSampler = true;
      DepthImageDesc.CreateDescriptorSet = false;
      DepthImageDesc.AspectFlag = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
      DepthImageDesc.FinalImageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

      FramebufferDescription framebufferDescription;
      framebufferDescription.DebugName = "PBR Pass";
      framebufferDescription.Width = SwapChain.m_Extent.width;
      framebufferDescription.Height = SwapChain.m_Extent.height;
      framebufferDescription.Extent = &Window::GetWindowExtent();
      framebufferDescription.RenderPass = s_RenderPasses.PBRRenderPass.Get();
      framebufferDescription.ImageDescription = {colorImageDesc, DepthImageDesc};
      framebufferDescription.OnResize = [] {
        const auto& LogicalDevice = VulkanContext::Context.Device;
        LogicalDevice.updateDescriptorSets({
            {
              s_QuadDescriptorSet.Get(), 7, 0, 1, vk::DescriptorType::eCombinedImageSampler,
              &s_FrameBuffers.PBRPassFB.GetImage()[0].GetDescImageInfo()
            },
          },
          nullptr);
      };
      s_FrameBuffers.PBRPassFB.CreateFramebuffer(framebufferDescription);
    }

    {
      FramebufferDescription bloomDescription;
      bloomDescription.DebugName = "Bloom Pass";
      bloomDescription.Extent = &Window::GetWindowExtent();
      bloomDescription.ImageDescription = {colorImageDesc};
      bloomDescription.Height = Window::GetHeight();
      bloomDescription.Width = Window::GetWidth();
      bloomDescription.RenderPass = s_Pipelines.BloomPipeline.GetRenderPass().Get();
      bloomDescription.OnResize = [] {
        const auto& LogicalDevice = VulkanContext::Context.Device;
        LogicalDevice.updateDescriptorSets({
            {
              s_BloomPrefilterDescriptorSet.Get(), 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr,
              &s_RendererData.BloomBuffer.GetDescriptor()
            },
            {
              s_BloomPrefilterDescriptorSet.Get(), 1, 0, 1, vk::DescriptorType::eCombinedImageSampler,
              &s_FrameBuffers.PBRPassFB.GetImage()[0].GetDescImageInfo()
            },
            {
              s_BloomPrefilterDescriptorSet.Get(), 2, 0, 1, vk::DescriptorType::eCombinedImageSampler,
              &s_FrameBuffers.PBRPassFB.GetImage()[0].GetDescImageInfo()
            },
          },
          nullptr);
      };
      s_FrameBuffers.BloomPrefilteredFB.CreateFramebuffer(bloomDescription);

      bloomDescription.RenderPass = s_Pipelines.GaussianBlurPipeline.GetRenderPass().Get();
      bloomDescription.OnResize = [] {
        const auto& LogicalDevice = VulkanContext::Context.Device;
        LogicalDevice.updateDescriptorSets({
            {
              s_GaussDescriptorSet.Get(), 0, 0, 1, vk::DescriptorType::eCombinedImageSampler,
              &s_FrameBuffers.BloomPrefilteredFB.GetImage()[0].GetDescImageInfo()
            },
          },
          nullptr);
      };
      s_FrameBuffers.BloomDownsampledFB.CreateFramebuffer(bloomDescription);
      s_FrameBuffers.BloomUpsampledFB.CreateFramebuffer(bloomDescription);
    }
    {
      FramebufferDescription ssaoFramebufferDescription;
      ssaoFramebufferDescription.DebugName = "SSAO Pass";
      ssaoFramebufferDescription.Width = SwapChain.m_Extent.width;
      ssaoFramebufferDescription.Height = SwapChain.m_Extent.height;
      ssaoFramebufferDescription.Extent = &Window::GetWindowExtent();
      ssaoFramebufferDescription.RenderPass = s_RenderPasses.SSAORenderPass.Get();
      colorImageDesc.Format = vk::Format::eR8Unorm;
      //colorImageDesc.SamplerFiltering = vk::Filter::eNearest;
      colorImageDesc.SamplerBorderColor = vk::BorderColor::eFloatOpaqueWhite;
      ssaoFramebufferDescription.ImageDescription = {colorImageDesc};
      ssaoFramebufferDescription.OnResize = UpdateSSAODescriptorSets;
      s_FrameBuffers.SSAOPassFB.CreateFramebuffer(ssaoFramebufferDescription);
    }
    {
      FramebufferDescription ssaoFramebufferDescription;
      ssaoFramebufferDescription.DebugName = "SSAO Horizontal Blur Pass";
      ssaoFramebufferDescription.Width = Window::GetWidth();
      ssaoFramebufferDescription.Height = Window::GetHeight();
      ssaoFramebufferDescription.Extent = &Window::GetWindowExtent();
      ssaoFramebufferDescription.RenderPass = s_RenderPasses.SSAOHBlurRP.Get();
      colorImageDesc.Format = vk::Format::eR8Unorm;
      ssaoFramebufferDescription.ImageDescription = {colorImageDesc};
      ssaoFramebufferDescription.OnResize = [] {
        const auto& LogicalDevice = VulkanContext::Context.Device;
        LogicalDevice.updateDescriptorSets({
            {
              s_SSAOHBlurDescriptorSet.Get(), 0, 0, 1, vk::DescriptorType::eCombinedImageSampler,
              &s_FrameBuffers.SSAOPassFB.GetImage()[0].GetDescImageInfo()
            },
            {
              s_SSAOHBlurDescriptorSet.Get(), 1, 0, 1, vk::DescriptorType::eCombinedImageSampler,
              &s_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetDescImageInfo()
            },
            {
              s_SSAOHBlurDescriptorSet.Get(), 2, 0, 1, vk::DescriptorType::eCombinedImageSampler,
              &s_FrameBuffers.DepthNormalPassFB.GetImage()[1].GetDescImageInfo()
            },
          },
          nullptr);
      };
      s_FrameBuffers.SSAOHBlurPassFB.CreateFramebuffer(ssaoFramebufferDescription);

      ssaoFramebufferDescription.DebugName = "SSAO Vertical Blur Pass";
      ssaoFramebufferDescription.RenderPass = s_RenderPasses.SSAOVBlurRP.Get();
      ssaoFramebufferDescription.OnResize = [] {
        const auto& LogicalDevice = VulkanContext::Context.Device;
        LogicalDevice.updateDescriptorSets({
            {
              s_SSAOVBlurDescriptorSet.Get(), 0, 0, 1, vk::DescriptorType::eCombinedImageSampler,
              &s_FrameBuffers.SSAOHBlurPassFB.GetImage()[0].GetDescImageInfo()
            },
            {
              s_SSAOVBlurDescriptorSet.Get(), 1, 0, 1, vk::DescriptorType::eCombinedImageSampler,
              &s_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetDescImageInfo()
            },
            {
              s_SSAOVBlurDescriptorSet.Get(), 2, 0, 1, vk::DescriptorType::eCombinedImageSampler,
              &s_FrameBuffers.DepthNormalPassFB.GetImage()[1].GetDescImageInfo()
            },
          },
          nullptr);
      };
      s_FrameBuffers.SSAOVBlurPassFB.CreateFramebuffer(ssaoFramebufferDescription);
    }
    {
      FramebufferDescription pbrCompositeDescription;
      pbrCompositeDescription.DebugName = "PBR Composite Pass";
      pbrCompositeDescription.Width = Window::GetWidth();
      pbrCompositeDescription.Height = Window::GetHeight();
      pbrCompositeDescription.Extent = &Window::GetWindowExtent();
      pbrCompositeDescription.RenderPass = s_RenderPasses.CompositeRP.Get();
      colorImageDesc.Format = vk::Format::eR32G32B32A32Sfloat;
      pbrCompositeDescription.ImageDescription = {colorImageDesc};
      pbrCompositeDescription.OnResize = [] {
        const auto& LogicalDevice = VulkanContext::Context.Device;
        LogicalDevice.updateDescriptorSets({
            {
              s_CompositeDescriptorSet.Get(), 0, 0, 1, vk::DescriptorType::eCombinedImageSampler,
              &s_FrameBuffers.PBRPassFB.GetImage()[0].GetDescImageInfo()
            },
            {
              s_CompositeDescriptorSet.Get(), 1, 0, 1, vk::DescriptorType::eCombinedImageSampler,
              &s_FrameBuffers.SSAOVBlurPassFB.GetImage()[0].GetDescImageInfo()
            },
            {
              s_CompositeDescriptorSet.Get(), 2, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr,
              &s_RendererData.CompositeParametersBuffer.GetDescriptor()
            },
          },
          nullptr);
      };
      s_FrameBuffers.CompositePassFB.CreateFramebuffer(pbrCompositeDescription);
    }
  }

  void VulkanRenderer::ResizeBuffers() {
    WaitDeviceIdle();
    SwapChain.RecreateSwapChain();
    FrameBufferPool::ResizeBuffers();
    SwapChain.Resizing = false;
  }

  void VulkanRenderer::UpdateSkyboxDescriptorSets() {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    LogicalDevice.updateDescriptorSets({
        {
          s_SkyboxDescriptorSet.Get(), 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr,
          &s_RendererData.SkyboxBuffer.GetDescriptor()
        },
        {
          s_SkyboxDescriptorSet.Get(), 1, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr,
          &s_RendererData.CompositeParametersBuffer.GetDescriptor()
        },
        {
          s_SkyboxDescriptorSet.Get(), 6, 0, 1, vk::DescriptorType::eCombinedImageSampler,
          &s_Resources.CubeMap.GetDescImageInfo()
        },
      },
      nullptr);
  }

  void VulkanRenderer::UpdateComputeDescriptorSets() {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    LogicalDevice.updateDescriptorSets({
        {
          s_ComputeDescriptorSet.Get(), 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr,
          &s_RendererData.ObjectsBuffer.GetDescriptor()
        },
        {
          s_ComputeDescriptorSet.Get(), 1, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr,
          &s_RendererData.ParametersBuffer.GetDescriptor()
        },
        {
          s_ComputeDescriptorSet.Get(), 2, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr,
          &s_RendererData.LightsBuffer.GetDescriptor()
        },
        {
          s_ComputeDescriptorSet.Get(), 3, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr,
          &s_RendererData.FrustumBuffer.GetDescriptor()
        },
        {
          s_ComputeDescriptorSet.Get(), 4, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr,
          &s_RendererData.LighIndexBuffer.GetDescriptor()
        },
        {
          s_ComputeDescriptorSet.Get(), 5, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr,
          &s_RendererData.LighGridBuffer.GetDescriptor()
        },
        {
          s_ComputeDescriptorSet.Get(), 6, 0, 1, vk::DescriptorType::eCombinedImageSampler,
          &s_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetDescImageInfo()
        },
      },
      nullptr);
  }

  void VulkanRenderer::UpdateSSAODescriptorSets() {
    s_SSAODescriptorSet.WriteDescriptorSets = {
      {
        s_SSAODescriptorSet.Get(), 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr,
        &s_RendererData.ObjectsBuffer.GetDescriptor()
      },
      {
        s_SSAODescriptorSet.Get(), 1, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr,
        &s_RendererData.SSAOBuffer.GetDescriptor()
      },
      {
        s_SSAODescriptorSet.Get(), 2, 0, 1, vk::DescriptorType::eCombinedImageSampler,
        &s_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetDescImageInfo()
      },
      {
        s_SSAODescriptorSet.Get(), 3, 0, 1, vk::DescriptorType::eCombinedImageSampler,
        &s_RendererData.SSAONoise.GetDescImageInfo()
      },
      {
        s_SSAODescriptorSet.Get(), 4, 0, 1, vk::DescriptorType::eCombinedImageSampler,
        &s_FrameBuffers.DepthNormalPassFB.GetImage()[1].GetDescImageInfo()
      },
    };
    s_SSAODescriptorSet.Update();
  }

  static void MeshConstDescriptors(const glm::mat4 transform,
                                   const std::vector<Ref<Material>>& materials,
                                   const Mesh::Primitive* part,
                                   const VulkanPipeline& pipeline,
                                   const vk::CommandBuffer& commandBuffer) {
    const auto& material = materials[part->materialIndex];
    commandBuffer.pushConstants(pipeline.GetPipelineLayout(),
      vk::ShaderStageFlagBits::eVertex,
      0,
      sizeof(glm::mat4),
      &transform);
    commandBuffer.pushConstants(pipeline.GetPipelineLayout(),
      vk::ShaderStageFlagBits::eFragment,
      sizeof(glm::mat4),
      sizeof Material::Parameters,
      &material->Parameters);

    const std::vector descriptorSets = {Material::s_DescriptorSet.Get(), material->MaterialDescriptorSet.Get()};
    pipeline.BindDescriptorSets(commandBuffer,
      {Material::s_DescriptorSet.Get(), material->MaterialDescriptorSet.Get()},
      0,
      2);
  }

  void VulkanRenderer::InitRenderGraph() {
    auto& renderGraph = s_RendererContext.RenderGraph;

    SwapchainPass swapchain{&s_QuadDescriptorSet};
    renderGraph.SetSwapchain(swapchain);

    RenderGraphPass depthPrePass("Depth Pre Pass",
      {&s_RendererContext.DepthPassCommandBuffer},
      &s_Pipelines.DepthPrePassPipeline,
      {&s_FrameBuffers.DepthNormalPassFB},
      [](VulkanCommandBuffer& commandBuffer, int32_t) {
        ZoneScopedN("DepthPrePass");
        //TracyVkZone(TracyProfiler::GetContext(), commandBuffer.Get(), "Depth Pre Pass")
        commandBuffer.SetViwportWindow().SetScissorWindow();
        for (const auto& mesh : s_MeshDrawList) {
          if (mesh.MeshGeometry.Nodes.empty())
            continue;

          RenderMesh(mesh,
            s_RendererContext.DepthPassCommandBuffer.Get(),
            s_Pipelines.DepthPrePassPipeline,
            [&](const Mesh::Primitive* part) {
              if (!mesh.MeshGeometry.GetMaterial(part->materialIndex)->IsOpaque())
                return false;
              commandBuffer.PushConstants(s_Pipelines.DepthPrePassPipeline.GetPipelineLayout(),
                vk::ShaderStageFlagBits::eVertex,
                0,
                sizeof(glm::mat4),
                &mesh.Transform);
              s_Pipelines.DepthPrePassPipeline.BindDescriptorSets(commandBuffer.Get(), {s_DepthDescriptorSet.Get()});
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

    RenderGraphPass directShadowDepthPass("Direct Shadow Depth Pass",
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
        //TracyVkZone(TracyProfiler::GetContext(), commandBuffer.Get(), "Direct Shadow Depth Pass")
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
          UpdateCascades(transform, s_RendererContext.CurrentCamera, s_RendererData.DirectShadowUBO);
          s_RendererData.DirectShadowBuffer.Copy(&s_RendererData.DirectShadowUBO,
            sizeof s_RendererData.DirectShadowUBO);

          for (const auto& mesh : s_MeshDrawList) {
            if (mesh.MeshGeometry.Nodes.empty())
              continue;
            RenderMesh(mesh,
              commandBuffer.Get(),
              s_Pipelines.DirectShadowDepthPipeline,
              [&](const Mesh::Primitive* part) {
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

                const auto& material = mesh.MeshGeometry.GetMaterial(part->materialIndex);
                const std::vector descriptorSets = {
                  s_ShadowDepthDescriptorSet.Get(), material->MaterialDescriptorSet.Get()
                };
                commandBuffer.Get().bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                  s_Pipelines.DirectShadowDepthPipeline.GetPipelineLayout(),
                  0,
                  2,
                  descriptorSets.data(),
                  0,
                  nullptr);
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

    RenderGraphPass ssaoPass("SSAO Pass",
      {&s_RendererContext.SSAOCommandBuffer},
      &s_Pipelines.SSAOPassPipeline,
      {&s_FrameBuffers.SSAOPassFB},
      [](VulkanCommandBuffer& commandBuffer, int32_t) {
        ZoneScopedN("SSAOPass");
        //TracyVkZone(TracyProfiler::GetContext(), commandBuffer.Get(), "SSAO Pass")
        if (!RendererConfig::Get()->SSAOConfig.Enabled)
          return;
        commandBuffer.SetFlippedViwportWindow().SetScissorWindow();
        s_Pipelines.SSAOPassPipeline.BindPipeline(commandBuffer.Get());
        s_Pipelines.SSAOPassPipeline.BindDescriptorSets(commandBuffer.Get(), {s_SSAODescriptorSet.Get()});
        DrawFullscreenQuad(commandBuffer.Get(), true);
      },
      {clearValues},
      &VulkanContext::VulkanQueue.GraphicsQueue);
    ssaoPass.AddReadDependency(renderGraph, "Depth Pre Pass").AddInnerPass({
      "SSAO Blur H Pass", {&s_RendererContext.SSAOCommandBuffer}, &s_Pipelines.SSAOHBlurPassPipeline,
      {&s_FrameBuffers.SSAOHBlurPassFB}, [](const VulkanCommandBuffer& commandBuffer, int32_t) {
        ZoneScopedN("SSAOBlurPassH");
        //TracyVkZone(TracyProfiler::GetContext(), commandBuffer.Get(), "SSAOBlurPassH")

        if (!RendererConfig::Get()->SSAOConfig.Enabled)
          return;
        s_Pipelines.SSAOHBlurPassPipeline.BindPipeline(commandBuffer.Get());
        s_Pipelines.SSAOHBlurPassPipeline.BindDescriptorSets(commandBuffer.Get(), {s_SSAOHBlurDescriptorSet.Get()});
        s_RendererData.SSAOBlurParams.texelOffset = glm::vec4(0.0f,
          (float)s_RendererData.SSAOBlurParams.texelRadius / (float)Window::GetHeight(),
          s_RendererContext.CurrentCamera->NearClip,
          s_RendererContext.CurrentCamera->FarClip);
        commandBuffer.Get().pushConstants(s_Pipelines.SSAOHBlurPassPipeline.GetPipelineLayout(),
          vk::ShaderStageFlagBits::eFragment,
          0,
          sizeof glm::vec4,
          &s_RendererData.SSAOBlurParams.texelOffset);
        DrawFullscreenQuad(commandBuffer.Get(), true);
      }
    }).AddInnerPass({
      "SSAO Blur V Pass", {&s_RendererContext.SSAOCommandBuffer}, &s_Pipelines.SSAOVBlurPassPipeline,
      {&s_FrameBuffers.SSAOVBlurPassFB}, [](const VulkanCommandBuffer& commandBuffer, int32_t) {
        ZoneScopedN("SSAOBlurPassV");
        //TracyVkZone(TracyProfiler::GetContext(), commandBuffer.Get(), "SSAOBlurPassV")
        if (!RendererConfig::Get()->SSAOConfig.Enabled)
          return;
        s_Pipelines.SSAOVBlurPassPipeline.BindPipeline(commandBuffer.Get());
        s_Pipelines.SSAOVBlurPassPipeline.BindDescriptorSets(commandBuffer.Get(), {s_SSAOVBlurDescriptorSet.Get()});
        s_RendererData.SSAOBlurParams.texelOffset = glm::vec4(
          (float)s_RendererData.SSAOBlurParams.texelRadius / (float)Window::GetWidth(),
          0.0f,
          s_RendererContext.CurrentCamera->NearClip,
          s_RendererContext.CurrentCamera->FarClip);
        commandBuffer.Get().pushConstants(s_Pipelines.SSAOVBlurPassPipeline.GetPipelineLayout(),
          vk::ShaderStageFlagBits::eFragment,
          0,
          sizeof glm::vec4,
          &s_RendererData.SSAOBlurParams.texelOffset);
        DrawFullscreenQuad(commandBuffer.Get(), true);
      },
    }).AddToGraph(renderGraph).RunWithCondition(RendererConfig::Get()->SSAOConfig.Enabled);

    RenderGraphPass pbrPass("PBR Pass",
      {&s_RendererContext.PBRPassCommandBuffer},
      &s_Pipelines.SkyboxPipeline,
      {&s_FrameBuffers.PBRPassFB},
      [](VulkanCommandBuffer& commandBuffer, int32_t) {
        ZoneScopedN("PBRPass");
        //TracyVkZone(TracyProfiler::GetContext(), commandBuffer.Get(), "PBR Pass")
        commandBuffer.SetViwportWindow().SetScissorWindow();

        //Skybox pass
        s_Pipelines.SkyboxPipeline.BindPipeline(commandBuffer.Get());
        s_Pipelines.SkyboxPipeline.BindDescriptorSets(commandBuffer.Get(), {s_SkyboxDescriptorSet.Get()});
        commandBuffer.PushConstants(s_Pipelines.SkyboxPipeline.GetPipelineLayout(),
          vk::ShaderStageFlagBits::eVertex,
          0,
          sizeof glm::mat4,
          &s_RendererContext.CurrentCamera->SkyboxView);
        s_SkyboxCube.Draw(commandBuffer.Get());

        //PBR pipeline
        for (const auto& mesh : s_MeshDrawList) {
          if (mesh.MeshGeometry.Nodes.empty())
            continue;

          if (mesh.MeshGeometry.ShouldUpdate || s_ForceUpdateMaterials) {
            mesh.MeshGeometry.UpdateMaterials();
            mesh.MeshGeometry.ShouldUpdate = false;
            return;
          }

          constexpr vk::DeviceSize offsets[1] = {0};
          commandBuffer.Get().bindVertexBuffers(0, mesh.MeshGeometry.VerticiesBuffer.Get(), offsets);
          commandBuffer.Get().bindIndexBuffer(mesh.MeshGeometry.IndiciesBuffer.Get(), 0, vk::IndexType::eUint32);

          RenderMesh(mesh,
            commandBuffer.Get(),
            s_Pipelines.PBRPipeline,
            [&](const Mesh::Primitive* part) {
              MeshConstDescriptors(mesh.Transform,
                mesh.Materials,
                part,
                s_Pipelines.PBRPipeline,
                commandBuffer.Get());
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
            sizeof glm::mat4 * 2,
            &s_RendererData.UboVS);
          //TODO: Add texturing with batching combined.
          //pipeline.BindDescriptorSets(commandBuffer, drawcmd.DescriptorSet.Get());
          s_QuadVertexBuffer.Copy(s_QuadVertexDataBuffer.data(), s_QuadVertexDataBuffer.size());
          DrawQuad(commandBuffer.Get());
        }
        s_QuadDrawList.clear();
      },
      {clearValues},
      &VulkanContext::VulkanQueue.GraphicsQueue);
    pbrPass./*AddStatsQuery(s_RendererContext.PBRPipelineStats).*/AddToGraph(renderGraph);

    RenderGraphPass bloomPass("Bloom Pass",
      {&s_RendererContext.BloomPassCommandBuffer},
      &s_Pipelines.BloomPipeline,
      {&s_FrameBuffers.BloomPrefilteredFB},
      [](VulkanCommandBuffer& commandBuffer, int32_t) {
        ZoneScopedN("BloomPass");
        //TracyVkZone(TracyProfiler::GetContext(), commandBuffer.Get(), "Bloom Pass")
        if (!RendererConfig::Get()->BloomConfig.Enabled)
          return;
        //1- Prefilter the pbr output with bloom shader
        const glm::vec4 threshold = {
          RendererConfig::Get()->BloomConfig.Threshold, RendererConfig::Get()->BloomConfig.Knee,
          RendererConfig::Get()->BloomConfig.Knee * 2.0f, 0.25f / RendererConfig::Get()->BloomConfig.Knee
        };
        const glm::vec4 params = {RendererConfig::Get()->BloomConfig.Clamp, 2.0f, 0.0f, 0.0f};
        s_RendererData.BloomUBO.Params = params;
        s_RendererData.BloomUBO.Params = threshold;
        s_RendererData.BloomBuffer.Copy(&s_RendererData.BloomUBO, sizeof s_RendererData.BloomUBO);
        commandBuffer.SetViwportWindow().SetScissorWindow();
        s_Pipelines.BloomPipeline.BindPipeline(commandBuffer.Get());
        s_Pipelines.BloomPipeline.BindDescriptorSets(commandBuffer.Get(), {s_BloomPrefilterDescriptorSet.Get()});
        DrawFullscreenQuad(commandBuffer.Get(), true);
      },
      clearValues,
      &VulkanContext::VulkanQueue.GraphicsQueue);

    RenderGraphPass compositePass({
      "Composite Pass", {&s_RendererContext.CompositeCommandBuffer}, &s_Pipelines.CompositePipeline,
      {&s_FrameBuffers.CompositePassFB}, [](VulkanCommandBuffer& commandBuffer, int32_t) {
        ZoneScopedN("Composite Pass");
        //TracyVkZone(TracyProfiler::GetContext(), commandBuffer.Get(), "Composite Pass")
        commandBuffer.SetFlippedViwportWindow().SetScissorWindow();
        s_Pipelines.CompositePipeline.BindPipeline(commandBuffer.Get());

        s_Pipelines.CompositePipeline.BindDescriptorSets(commandBuffer.Get(), {s_CompositeDescriptorSet.Get()});
        DrawFullscreenQuad(commandBuffer.Get(), true);
      },
      clearValues, &VulkanContext::VulkanQueue.GraphicsQueue
    });
    compositePass.AddReadDependency(renderGraph, "SSAO Pass").AddToGraph(renderGraph);

    RenderGraphPass frustumPass("Frustum Pass",
      {&s_RendererContext.FrustumCommandBuffer},
      {},
      {},
      [](const VulkanCommandBuffer& commandBuffer, int32_t) {
        ZoneScopedN("FrustumPass");
        //TracyVkZone(TracyProfiler::GetContext(), commandBuffer.Get(), "Frustum Pass")
        s_Pipelines.FrustumGridPipeline.BindPipeline(commandBuffer.Get());
        commandBuffer.Get().bindDescriptorSets(vk::PipelineBindPoint::eCompute,
          s_Pipelines.FrustumGridPipeline.GetPipelineLayout(),
          0,
          1,
          &s_ComputeDescriptorSet.Get(),
          0,
          nullptr);
        commandBuffer.Dispatch(s_RendererData.UboParams.numThreadGroups.x,
          s_RendererData.UboParams.numThreadGroups.y,
          1);
      },
      {},
      &VulkanContext::VulkanQueue.GraphicsQueue);
    //renderGraph.AddComputePass(frustumPass);

    RenderGraphPass lightListPass("Light List Pass",
      {s_RendererContext.ComputeCommandBuffers.data()},
      {},
      {},
      [](const VulkanCommandBuffer& commandBuffer, int32_t) {
        ZoneScopedN("Light List Pass");
        //TracyVkZone(TracyProfiler::GetContext(), commandBuffer.Get(), "Light List Pass")
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
        commandBuffer.Get().bindDescriptorSets(vk::PipelineBindPoint::eCompute,
          s_Pipelines.LightListPipeline.GetPipelineLayout(),
          0,
          1,
          &s_ComputeDescriptorSet.Get(),
          0,
          nullptr);
        commandBuffer.Dispatch(s_RendererData.UboParams.numThreadGroups.x,
          s_RendererData.UboParams.numThreadGroups.y,
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
    s_RendererContext.CommandPool = VulkanContext::Context.Device.createCommandPool(cmdPoolInfo);

    SwapChain.SetVsync(RendererConfig::Get()->DisplayConfig.VSync, false);
    SwapChain.CreateSwapChain();

    s_RendererContext.TimelineCommandBuffer.CreateBuffer();

    s_RendererContext.CompositeCommandBuffer.CreateBuffer();
    s_RendererContext.PBRPassCommandBuffer.CreateBuffer();
    s_RendererContext.BloomPassCommandBuffer.CreateBuffer();
    s_RendererContext.FrustumCommandBuffer.CreateBuffer();
    s_RendererContext.ComputeCommandBuffers.resize(1);
    s_RendererContext.ComputeCommandBuffers[0].CreateBuffer();
    s_RendererContext.DepthPassCommandBuffer.CreateBuffer();
    s_RendererContext.SSAOCommandBuffer.CreateBuffer();
    s_RendererContext.DirectShadowCommandBuffer.CreateBuffer();

    vk::DescriptorSetLayoutBinding binding[1];
    binding[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    binding[0].descriptorCount = 1;
    binding[0].stageFlags = vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutCreateInfo info = {};
    info.bindingCount = 1;
    info.pBindings = binding;
    VulkanUtils::CheckResult(
      LogicalDevice.createDescriptorSetLayout(&info, nullptr, &s_RendererData.ImageDescriptorSetLayout));

    CreateRenderPasses();
    CreateGraphicsPipelines();
    CreateFramebuffers();

    InitUIPipeline();

    s_GaussDescriptorSet.Allocate(s_Pipelines.GaussianBlurPipeline.GetDescriptorSetLayout());
    s_BloomPrefilterDescriptorSet.Allocate(s_Pipelines.BloomPipeline.GetDescriptorSetLayout());
    s_QuadDescriptorSet.Allocate(s_Pipelines.QuadPipeline.GetDescriptorSetLayout());
    s_SkyboxDescriptorSet.Allocate(s_Pipelines.SkyboxPipeline.GetDescriptorSetLayout());
    s_ComputeDescriptorSet.Allocate(s_Pipelines.LightListPipeline.GetDescriptorSetLayout());
    s_SSAODescriptorSet.Allocate(s_Pipelines.SSAOPassPipeline.GetDescriptorSetLayout());
    s_SSAOHBlurDescriptorSet.Allocate(s_Pipelines.SSAOHBlurPassPipeline.GetDescriptorSetLayout());
    s_SSAOVBlurDescriptorSet.Allocate(s_Pipelines.SSAOVBlurPassPipeline.GetDescriptorSetLayout());
    s_CompositeDescriptorSet.Allocate(s_Pipelines.CompositePipeline.GetDescriptorSetLayout());
    s_DepthDescriptorSet.Allocate(s_Pipelines.DepthPrePassPipeline.GetDescriptorSetLayout());
    s_ShadowDepthDescriptorSet.Allocate(s_Pipelines.DirectShadowDepthPipeline.GetDescriptorSetLayout());

    s_RendererData.SkyboxBuffer.CreateBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      sizeof RendererData::UboVS,
      &s_RendererData.UboVS).Map();

    s_RendererData.ParametersBuffer.CreateBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      sizeof RendererData::UboParams,
      &s_RendererData.UboParams).Map();

    s_RendererData.ObjectsBuffer.CreateBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      sizeof RendererData::UboVS,
      &s_RendererData.UboVS).Map();

    s_RendererData.LightsBuffer.CreateBuffer(
      vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      sizeof LightingData).Map();

    s_RendererData.FrustumBuffer.CreateBuffer(
      vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      sizeof RendererData::Frustums,
      &s_RendererData.Frustums).Map();

    s_RendererData.LighGridBuffer.CreateBuffer(vk::BufferUsageFlagBits::eStorageBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      sizeof(uint32_t) * MAX_NUM_FRUSTUMS * MAX_NUM_LIGHTS_PER_TILE).Map();

    s_RendererData.LighIndexBuffer.CreateBuffer(vk::BufferUsageFlagBits::eStorageBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      sizeof(uint32_t) * MAX_NUM_FRUSTUMS).Map();

    s_RendererData.BloomBuffer.CreateBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      sizeof RendererData::BloomUB).Map();

    //SSAO
    {
      constexpr int32_t SSAO_NOISE_DIM = 4;
      constexpr int32_t SSAO_KERNEL_SIZE = 64;

      std::default_random_engine rndEngine((unsigned)time(nullptr));
      std::uniform_real_distribution rndDist(0.0f, 1.0f);

      for (uint32_t i = 0; i < SSAO_KERNEL_SIZE; ++i) {
        glm::vec3 sample(rndDist(rndEngine) * 2.0 - 1.0, rndDist(rndEngine) * 2.0 - 1.0, rndDist(rndEngine));
        sample = glm::normalize(sample);
        sample *= rndDist(rndEngine);
        float scale = float(i) / float(SSAO_KERNEL_SIZE);
        scale = Math::Lerp(0.1f, 1.0f, scale * scale);
        s_RendererData.SSAOParams.ssaoSamples[i] = glm::vec4(sample * scale, 0.0f);
      }

      // Random noise
      std::vector<glm::vec4> ssaoNoise(SSAO_NOISE_DIM * SSAO_NOISE_DIM);
      for (auto& i : ssaoNoise) {
        i = glm::vec4(rndDist(rndEngine) * 2.0f - 1.0f, rndDist(rndEngine) * 2.0f - 1.0f, 0.0f, 0.0f);
      }
      VulkanImageDescription SSAONoiseDesc;
      SSAONoiseDesc.Format = vk::Format::eR32G32B32A32Sfloat;
      SSAONoiseDesc.Height = SSAO_NOISE_DIM;
      SSAONoiseDesc.Width = SSAO_NOISE_DIM;
      SSAONoiseDesc.EmbeddedStbData = (uint8_t*)ssaoNoise.data();
      SSAONoiseDesc.EmbeddedDataLength = ssaoNoise.size() * sizeof(glm::vec4);
      SSAONoiseDesc.CreateDescriptorSet = true;
      SSAONoiseDesc.CreateSampler = true;
      SSAONoiseDesc.CreateView = true;
      SSAONoiseDesc.TransitionLayoutAtCreate = true;
      SSAONoiseDesc.SamplerFiltering = vk::Filter::eLinear;
      SSAONoiseDesc.SamplerBorderColor = vk::BorderColor::eFloatTransparentBlack;
      s_RendererData.SSAONoise.Create(SSAONoiseDesc);

      s_RendererData.SSAOBuffer.CreateBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        sizeof RendererData::SSAOParams,
        &s_RendererData.SSAOParams).Map();
    }

    //PBR Composite buffer
    {
      s_RendererData.CompositeParametersBuffer.CreateBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible,
        sizeof RendererData::UBOCompositeParameters,
        &s_RendererData.UBOCompositeParameters).Map();
    }

    //Direct shadow buffer
    {
      s_RendererData.DirectShadowBuffer.CreateBuffer(vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible,
        sizeof RendererData::DirectShadowUBO,
        &s_RendererData.DirectShadowUBO).Map();
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
    CubeMapDesc.Path = "resources/hdrs/belfast_sunset.ktx2";
    CubeMapDesc.Type = ImageType::TYPE_CUBE;
    s_Resources.CubeMap.Create(CubeMapDesc);

    GeneratePrefilter();

    UpdateSkyboxDescriptorSets();
    UpdateComputeDescriptorSets();
    UpdateSSAODescriptorSets();
    s_FrameBuffers.PBRPassFB.GetDescription().OnResize();
    s_FrameBuffers.SSAOHBlurPassFB.GetDescription().OnResize();
    s_FrameBuffers.SSAOVBlurPassFB.GetDescription().OnResize();
    s_FrameBuffers.CompositePassFB.GetDescription().OnResize();
    for (auto& fb : s_FrameBuffers.DirectionalCascadesFB)
      fb.GetDescription().OnResize();

    UnloadShaders();

    s_RendererContext.Initialized = true;

    //Render graph
    InitRenderGraph();

    //Initalize tracy profiling
    //VkPhysicalDevice physicalDevice = VulkanContext::Context.PhysicalDevice;
    //TracyProfiler::InitTracyForVulkan(physicalDevice,
    //  LogicalDevice,
    //  VulkanContext::VulkanQueue.GraphicsQueue,
    //  s_RendererContext.TimelineCommandBuffer.Get());
  }

  void VulkanRenderer::InitUIPipeline() {
    const std::vector vertexInputBindings = {
      vk::VertexInputBindingDescription{0, sizeof(ImDrawVert), vk::VertexInputRate::eVertex},
    };
    const std::vector vertexInputAttributes = {
      vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32Sfloat, offsetof(ImDrawVert, pos)},
      vk::VertexInputAttributeDescription{1, 0, vk::Format::eR32G32Sfloat, offsetof(ImDrawVert, uv)},
      vk::VertexInputAttributeDescription{2, 0, vk::Format::eR8G8B8A8Unorm, offsetof(ImDrawVert, col)},
    };
    PipelineDescription uiPipelineDecs;
    uiPipelineDecs.Shader = CreateRef<VulkanShader>(ShaderCI{
      .VertexPath = "resources/shaders/ui.vert", .FragmentPath = "resources/shaders/ui.frag", .EntryPoint = "main",
      .Name = "UI",
    });
    uiPipelineDecs.DescriptorSetLayoutBindings = {{{0, vDT::eCombinedImageSampler, 1, vSS::eFragment},}};
    uiPipelineDecs.PushConstantRanges = {vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex, 0, sizeof(float) * 4}};
    uiPipelineDecs.RenderPass = SwapChain.m_RenderPass;
    uiPipelineDecs.VertexInputState.attributeDescriptions = vertexInputAttributes;
    uiPipelineDecs.VertexInputState.bindingDescriptions = vertexInputBindings;
    uiPipelineDecs.RasterizerDesc.CullMode = vk::CullModeFlagBits::eNone;
    uiPipelineDecs.RasterizerDesc.FrontCounterClockwise = true;
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
    GraphicsQueue.submit(endInfo);
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
    return s_FrameBuffers.CompositePassFB.GetImage()[0];
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

    //for (const auto& node : mesh.MeshGeometry.Nodes) {
    RenderNode(mesh.MeshGeometry.LinearNodes[mesh.SubmeshIndex], commandBuffer, pipeline, perMeshFunc);
    //}
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
    LogicalDevice.waitIdle();
  }

  void VulkanRenderer::WaitGraphicsQueueIdle() {
    VulkanContext::VulkanQueue.GraphicsQueue.waitIdle();
  }

  void VulkanRenderer::AddShader(const Ref<VulkanShader>& shader) {
    s_Shaders.emplace(shader->GetName(), shader);
  }

  void VulkanRenderer::RemoveShader(const std::string& name) {
    s_Shaders.erase(name);
  }
}
