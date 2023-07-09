#include "DefaultRenderPipeline.h"

#include "DebugRenderer.h"
#include "ResourcePool.h"
#include "ShaderLibrary.h"
#include "Assets/AssetManager.h"
#include "Core/Entity.h"
#include "Core/Resources.h"
#include "PBR/Prefilter.h"
#include "Utils/Profiler.h"
#include "Vulkan/VulkanContext.h"

#include "Vulkan/VulkanRenderer.h"
#include "Vulkan/Utils/VulkanUtils.h"

namespace Oxylus {
  void DefaultRenderPipeline::OnInit() {
    m_RendererContext.PostProcessCommandBuffer.CreateBuffer();
    m_RendererContext.PBRPassCommandBuffer.CreateBuffer();
    m_RendererContext.BloomPassCommandBuffer.CreateBuffer();
    m_RendererContext.SSRCommandBuffer.CreateBuffer();
    m_RendererContext.FrustumCommandBuffer.CreateBuffer();
    m_RendererContext.LightListCommandBuffer.CreateBuffer();
    m_RendererContext.DepthPassCommandBuffer.CreateBuffer();
    m_RendererContext.SSAOCommandBuffer.CreateBuffer();
    m_RendererContext.DirectShadowCommandBuffer.CreateBuffer();
    m_RendererContext.CompositeCommandBuffer.CreateBuffer();
    m_RendererContext.AtmosphereCommandBuffer.CreateBuffer();
    m_RendererContext.DepthOfFieldCommandBuffer.CreateBuffer();

    m_RendererData.SkyboxBuffer.CreateBuffer(vBU::eUniformBuffer, vMP::eHostVisible | vMP::eHostCoherent, sizeof RendererData::UBO_VS, &m_RendererData.UBO_VS).Map();
    m_RendererData.ParametersBuffer.CreateBuffer(vBU::eUniformBuffer, vMP::eHostVisible | vMP::eHostCoherent, sizeof RendererData::UBO_PbrPassParams, &m_RendererData.UBO_PbrPassParams).Map();
    m_RendererData.VSBuffer.CreateBuffer(vBU::eUniformBuffer, vMP::eHostVisible | vMP::eHostCoherent, sizeof RendererData::UBO_VS, &m_RendererData.UBO_VS).Map();
    m_RendererData.LightsBuffer.CreateBuffer(vBU::eStorageBuffer | vBU::eTransferDst,
      vMP::eHostVisible | vMP::eHostCoherent,
      sizeof(LightingData)).Map().SetOnUpdate(
      [this] {
        UpdateLightingData();
      }).Sink<LightChangeEvent>(m_LightBufferDispatcher);

    m_RendererData.FrustumBuffer.CreateBuffer(vBU::eStorageBuffer | vBU::eTransferDst, vMP::eHostVisible | vMP::eHostCoherent, sizeof RendererData::Frustums, &m_RendererData.Frustums).Map();
    m_RendererData.LighGridBuffer.CreateBuffer(vBU::eStorageBuffer, vMP::eHostVisible | vMP::eHostCoherent, sizeof(uint32_t) * MAX_NUM_FRUSTUMS * MAX_NUM_LIGHTS_PER_TILE).Map();
    m_RendererData.LighIndexBuffer.CreateBuffer(vBU::eStorageBuffer, vMP::eHostVisible | vMP::eHostCoherent, sizeof(uint32_t) * MAX_NUM_FRUSTUMS).Map();
    m_RendererData.SSRBuffer.CreateBuffer(vBU::eUniformBuffer,
      vMP::eHostVisible | vMP::eHostCoherent,
      sizeof RendererData::UBO_SSR).Map().SetOnUpdate([this] {
      m_RendererData.UBO_SSR.Samples = RendererConfig::Get()->SSRConfig.Samples;
      m_RendererData.UBO_SSR.MaxDist = RendererConfig::Get()->SSRConfig.MaxDist;
      m_RendererData.SSRBuffer.Copy(&m_RendererData.UBO_SSR, sizeof m_RendererData.UBO_SSR);
    }).Sink<RendererConfig::ConfigChangeEvent>(RendererConfig::Get()->ConfigChangeDispatcher);

    {
      m_RendererData.UBO_Atmosphere.InvViews[0] = glm::inverse(Camera::GenerateViewMatrix({}, Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f))); // PositiveX
      m_RendererData.UBO_Atmosphere.InvViews[1] = glm::inverse(Camera::GenerateViewMatrix({}, Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f))); // NegativeX

      m_RendererData.UBO_Atmosphere.InvViews[2] = glm::inverse(Camera::GenerateViewMatrix({}, Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f))); // PositiveY
      m_RendererData.UBO_Atmosphere.InvViews[3] = glm::inverse(Camera::GenerateViewMatrix({}, Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f))); // NegativeY

      m_RendererData.UBO_Atmosphere.InvViews[4] = glm::inverse(Camera::GenerateViewMatrix({}, Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f))); // PositiveZ
      m_RendererData.UBO_Atmosphere.InvViews[5] = glm::inverse(Camera::GenerateViewMatrix({}, Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f))); // NegativeZ
      m_RendererData.AtmosphereBuffer.CreateBuffer(vBU::eUniformBuffer,
        vMP::eHostVisible |
        vMP::eHostCoherent,
        sizeof RendererData::UBO_Atmosphere).Map();
    }

    m_RendererData.SSAOBuffer.CreateBuffer(vBU::eUniformBuffer,
      vMP::eHostVisible |
      vMP::eHostCoherent,
      sizeof RendererData::UBO_SSAOParams,
      &m_RendererData.UBO_SSAOParams).Map().SetOnUpdate([this] {
      m_RendererData.UBO_SSAOParams.radius = RendererConfig::Get()->SSAOConfig.Radius;
      m_RendererData.SSAOBuffer.Copy(&m_RendererData.UBO_SSAOParams, sizeof m_RendererData.UBO_SSAOParams);
    }).Sink<RendererConfig::ConfigChangeEvent>(RendererConfig::Get()->ConfigChangeDispatcher);

    // Postprocessing buffer
    {
      m_RendererData.PostProcessBuffer.CreateBuffer(vBU::eUniformBuffer,
        vMP::eHostVisible,
        sizeof RendererData::UBO_PostProcessParams,
        &m_RendererData.UBO_PostProcessParams).Map().SetOnUpdate([this] {
        m_RendererData.UBO_PostProcessParams.Tonemapper = RendererConfig::Get()->ColorConfig.Tonemapper;
        m_RendererData.UBO_PostProcessParams.Exposure = RendererConfig::Get()->ColorConfig.Exposure;
        m_RendererData.UBO_PostProcessParams.Gamma = RendererConfig::Get()->ColorConfig.Gamma;
        m_RendererData.UBO_PostProcessParams.EnableSSAO = RendererConfig::Get()->SSAOConfig.Enabled;
        m_RendererData.UBO_PostProcessParams.EnableBloom = RendererConfig::Get()->BloomConfig.Enabled;
        m_RendererData.UBO_PostProcessParams.EnableSSR = RendererConfig::Get()->SSRConfig.Enabled;
        m_RendererData.PostProcessBuffer.Copy(&m_RendererData.UBO_PostProcessParams, sizeof m_RendererData.UBO_PostProcessParams);
      }).Sink<RendererConfig::ConfigChangeEvent>(RendererConfig::Get()->ConfigChangeDispatcher);
    }

    // Direct shadow buffer
    {
      m_RendererData.DirectShadowBuffer.CreateBuffer(vBU::eUniformBuffer,
        vMP::eHostVisible,
        sizeof RendererData::UBO_DirectShadow,
        &m_RendererData.UBO_DirectShadow).Map();
    }

    // Lights data
    m_PointLightsData.reserve(MAX_NUM_LIGHTS);

    // Mesh data
    m_MeshDrawList.reserve(MAX_NUM_MESHES);
    m_TransparentMeshDrawList.reserve(MAX_NUM_MESHES);

    m_SkyboxCube.LoadFromFile(Resources::GetResourcesPath("Objects/cube.glb"), Mesh::FlipY | Mesh::DontCreateMaterials);

    VulkanImageDescription cubeMapDesc{};
    const auto path = Resources::GetResourcesPath("HDRs/belfast_sunset.ktx2");
    cubeMapDesc.Type = ImageType::TYPE_CUBE;
    if (!std::filesystem::exists(path)) {
      cubeMapDesc.Width = 1;
      cubeMapDesc.Height = 1;
      cubeMapDesc.CreateDescriptorSet = true;
      m_Resources.CubeMap = CreateRef<VulkanImage>(cubeMapDesc);
    }
    else {
      cubeMapDesc.Path = path;
      m_Resources.CubeMap = CreateRef<VulkanImage>();
      m_Resources.CubeMap = AssetManager::GetImageAsset(cubeMapDesc).Data;
    }
    CreateGraphicsPipelines();
    CreateFramebuffers();

    m_SkyboxDescriptorSet.CreateFromShader(m_Pipelines.SkyboxPipeline.GetShader());
    m_SkyboxDescriptorSet.WriteDescriptorSets[0].pBufferInfo = &m_RendererData.SkyboxBuffer.GetDescriptor();
    m_SkyboxDescriptorSet.WriteDescriptorSets[1].pImageInfo = &m_Resources.CubeMap->GetDescImageInfo();
    m_SkyboxDescriptorSet.Update();

    m_LightListDescriptorSet.CreateFromShader(m_Pipelines.LightListPipeline.GetShader());
    m_SSAODescriptorSet.CreateFromShader(m_Pipelines.SSAOPassPipeline.GetShader())
                       .WriteDescriptorSets[0].pBufferInfo = &m_RendererData.VSBuffer.GetDescriptor();

    m_SSAOBlurDescriptorSet.CreateFromShader(m_Pipelines.GaussianBlurPipeline.GetShader());
    m_PostProcessDescriptorSet.CreateFromShader(m_Pipelines.PostProcessPipeline.GetShader());
    m_PostProcessDescriptorSet.WriteDescriptorSets[1].pBufferInfo = &m_RendererData.PostProcessBuffer.GetDescriptor();

    m_BloomDescriptorSet.CreateFromShader(m_Pipelines.BloomPipeline.GetShader());
    m_DepthDescriptorSet.CreateFromShader(m_Pipelines.DepthPrePassPipeline.GetShader())
                        .WriteDescriptorSets[0].pBufferInfo = &m_RendererData.VSBuffer.GetDescriptor();
    m_DepthDescriptorSet.Update();

    m_ShadowDepthDescriptorSet.CreateFromShader(m_Pipelines.DirectShadowDepthPipeline.GetShader())
                              .WriteDescriptorSets[0].pBufferInfo = &m_RendererData.DirectShadowBuffer.GetDescriptor();

    m_SSRDescriptorSet.CreateFromShader(m_Pipelines.SSRPipeline.GetShader());

    m_CompositeDescriptorSet.CreateFromShader(m_Pipelines.CompositePipeline.GetShader());
    m_CompositeDescriptorSet.WriteDescriptorSets[5].pBufferInfo = &m_RendererData.PostProcessBuffer.GetDescriptor();

    m_AtmosphereDescriptorSet.CreateFromShader(m_Pipelines.AtmospherePipeline.GetShader());
    m_AtmosphereDescriptorSet.WriteDescriptorSets[1].pBufferInfo = &m_RendererData.AtmosphereBuffer.GetDescriptor();

    m_DepthOfFieldDescriptorSet.CreateFromShader(m_Pipelines.DepthOfFieldPipeline.GetShader());

    GeneratePrefilter();

    InitRenderGraph();

    Material::s_DescriptorSet.CreateFromShader(m_Pipelines.PBRPipeline.GetShader());
    Material::s_DescriptorSet.WriteDescriptorSets[0].pBufferInfo = &m_RendererData.VSBuffer.GetDescriptor();
    Material::s_DescriptorSet.WriteDescriptorSets[1].pBufferInfo = &m_RendererData.ParametersBuffer.GetDescriptor();
    Material::s_DescriptorSet.WriteDescriptorSets[2].pBufferInfo = &m_RendererData.LightsBuffer.GetDescriptor();
    //Material::s_DescriptorSet.WriteDescriptorSets[3].pBufferInfo = &m_RendererData.FrustumBuffer.GetDescriptor();
    Material::s_DescriptorSet.WriteDescriptorSets[3].pBufferInfo = &m_RendererData.LighIndexBuffer.GetDescriptor();
    //Material::s_DescriptorSet.WriteDescriptorSets[4].pBufferInfo = &m_RendererData.LighGridBuffer.GetDescriptor();
    Material::s_DescriptorSet.WriteDescriptorSets[4].pImageInfo = &m_Resources.IrradianceCube.GetDescImageInfo();
    Material::s_DescriptorSet.WriteDescriptorSets[5].pImageInfo = &m_Resources.LutBRDF.GetDescImageInfo();
    Material::s_DescriptorSet.WriteDescriptorSets[6].pImageInfo = &m_Resources.PrefilteredCube.GetDescImageInfo();
    //Material::s_DescriptorSet.WriteDescriptorSets[9].pImageInfo = &m_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetDescImageInfo();
    Material::s_DescriptorSet.WriteDescriptorSets[7].pImageInfo = &m_Resources.DirectShadowsDepthArray.GetDescImageInfo();
    Material::s_DescriptorSet.WriteDescriptorSets[8].pBufferInfo = &m_RendererData.DirectShadowBuffer.GetDescriptor();
    Material::s_DescriptorSet.Update();

    ShaderLibrary::UnloadShaders();
  }

  void DefaultRenderPipeline::OnRender(Scene* scene) {
    m_Scene = scene;

    // TODO:(hatrickek) Temporary solution for camera.
    m_RendererContext.CurrentCamera = VulkanRenderer::s_RendererContext.CurrentCamera;

    if (!m_RendererContext.CurrentCamera)
      OX_CORE_FATAL("No camera is set for rendering!");

    UpdateUniformBuffers();

    // Mesh
    {
      OX_SCOPED_ZONE_N("Mesh System");
      const auto view = scene->m_Registry.view<TransformComponent, MeshRendererComponent, MaterialComponent, TagComponent>();
      for (const auto&& [entity, transform, meshrenderer, material, tag] : view.each()) {
        auto e = Entity(entity, scene);
        auto parent = e.GetParent();
        bool parentEnabled = true;
        if (parent)
            parentEnabled = parent.GetComponent<TagComponent>().Enabled;
        if (tag.Enabled && parentEnabled)
          m_MeshDrawList.emplace_back(*meshrenderer.MeshGeometry, e.GetWorldTransform(), material.Materials, meshrenderer.SubmesIndex);
      }
    }

    // Particle system
    {
      OX_SCOPED_ZONE_N("Particle System");
      const auto particleSystemView = scene->m_Registry.view<TransformComponent, ParticleSystemComponent>();
      for (auto&& [e, tc, psc] : particleSystemView.each()) {
        psc.System->OnUpdate(Application::GetTimestep(), tc.Translation);
        psc.System->OnRender();
      }
    }

    // Lighting
    {
      OX_SCOPED_ZONE_N("Lighting System");
      // Scene lights
      {
        std::vector<Entity> lights;
        const auto view = scene->m_Registry.view<LightComponent>();
        lights.reserve(view.size());
        for (auto&& [e, lc] : view.each()) {
          Entity entity = {e, scene};
          if (!entity.GetComponent<TagComponent>().Enabled)
            continue;
          lights.emplace_back(entity);
        }
        m_SceneLights = std::move(lights);
        m_LightBufferDispatcher.trigger(LightChangeEvent{});
      }
      // Sky light
      {
        const auto view = scene->m_Registry.view<SkyLightComponent>();
        if (!view.empty()) {
          const auto& skyLight = Entity(*view.begin(), scene).GetComponent<SkyLightComponent>();
          m_RendererData.UBO_PbrPassParams.lodBias = skyLight.CubemapLodBias;
          if (skyLight.Cubemap && skyLight.Cubemap->LoadCallback) {
            m_Resources.CubeMap = skyLight.Cubemap;
            m_ForceUpdateMaterials = true;
            skyLight.Cubemap->LoadCallback = false;
          }
        }
      }
    }
  }

  const VulkanImage& DefaultRenderPipeline::GetFinalImage() {
    return m_FrameBuffers.PostProcessPassFB.GetImage()[0];
  }

  void DefaultRenderPipeline::OnDispatcherEvents(EventDispatcher& dispatcher) {
    dispatcher.sink<SceneRenderer::ProbeChangeEvent>().connect<&DefaultRenderPipeline::UpdateProbes>(*this);
    m_RendererData.PostProcessBuffer.Sink<SceneRenderer::ProbeChangeEvent>(dispatcher);

    dispatcher.sink<SceneRenderer::SkyboxLoadEvent>().connect<&DefaultRenderPipeline::UpdateSkybox>(*this);
  }

  void DefaultRenderPipeline::OnShutdown() { }

  void DefaultRenderPipeline::RenderNode(const Mesh::Node* node, const vk::CommandBuffer& commandBuffer, const VulkanPipeline& pipeline, const std::function<bool(Mesh::Primitive* prim)>& perMeshFunc) {
    for (const auto& part : node->Primitives) {
      if (!perMeshFunc(part))
        continue;
      commandBuffer.drawIndexed(part->indexCount, 1, part->firstIndex, 0, 0);
    }
    for (const auto& child : node->Children) {
      RenderNode(child, commandBuffer, pipeline, perMeshFunc);
    }
  }

  void DefaultRenderPipeline::RenderMesh(const MeshData& mesh, const vk::CommandBuffer& commandBuffer, const VulkanPipeline& pipeline, const std::function<bool(Mesh::Primitive* prim)>& perMeshFunc) {
    pipeline.BindPipeline(commandBuffer);

    if (mesh.MeshGeometry.ShouldUpdate || m_ForceUpdateMaterials) {
      mesh.MeshGeometry.UpdateMaterials();
      mesh.MeshGeometry.ShouldUpdate = false;
      return;
    }

    constexpr vk::DeviceSize offsets[1] = {0};
    commandBuffer.bindVertexBuffers(0, mesh.MeshGeometry.VerticiesBuffer.Get(), offsets);
    commandBuffer.bindIndexBuffer(mesh.MeshGeometry.IndiciesBuffer.Get(), 0, vk::IndexType::eUint32);

    RenderNode(mesh.MeshGeometry.LinearNodes[mesh.SubmeshIndex], commandBuffer, pipeline, perMeshFunc);
  }

  void DefaultRenderPipeline::UpdateSkybox(const SceneRenderer::SkyboxLoadEvent& e) {
    m_ForceUpdateMaterials = true;
    m_Resources.CubeMap = e.CubeMap;
    GeneratePrefilter();
    m_SkyboxDescriptorSet.WriteDescriptorSets[1].pImageInfo = &m_Resources.CubeMap->GetDescImageInfo();
    m_SkyboxDescriptorSet.Update();

    Material::s_DescriptorSet.WriteDescriptorSets[4].pImageInfo = &m_Resources.IrradianceCube.GetDescImageInfo();
    Material::s_DescriptorSet.WriteDescriptorSets[5].pImageInfo = &m_Resources.LutBRDF.GetDescImageInfo();
    Material::s_DescriptorSet.WriteDescriptorSets[6].pImageInfo = &m_Resources.PrefilteredCube.GetDescImageInfo();
    Material::s_DescriptorSet.Update();
  }

  void DefaultRenderPipeline::UpdateCascades(const Entity& dirLightEntity, Camera* camera, RendererData::DirectShadowUB& cascadesUbo) const {
    OX_SCOPED_ZONE;
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

    Mat4 invCam = inverse(camera->GetProjectionMatrix() * (camera->GetViewMatrix()));

    // Calculate orthographic projection matrix for each cascade
    for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
      float splitDist = cascadeSplits[i];
      float lastSplitDist = i == 0 ? 0.0f : cascadeSplits[i - 1];

      Vec3 frustumCorners[8] = {
        Vec3(-1.0f, 1.0f, 0.0f),
        Vec3(1.0f, 1.0f, 0.0f),
        Vec3(1.0f, -1.0f, 0.0f),
        Vec3(-1.0f, -1.0f, 0.0f),
        Vec3(-1.0f, 1.0f, 1.0f),
        Vec3(1.0f, 1.0f, 1.0f),
        Vec3(1.0f, -1.0f, 1.0f),
        Vec3(-1.0f, -1.0f, 1.0f),
      };

      // Project frustum corners into world space
      for (uint32_t j = 0; j < 8; j++) {
        glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[j], 1.0f);
        frustumCorners[j] = (invCorner / invCorner.w);
      }

      for (uint32_t j = 0; j < 4; j++) {
        Vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
        frustumCorners[j + 4] = frustumCorners[j] + dist * splitDist;
        frustumCorners[j] = frustumCorners[j] + dist * lastSplitDist;
      }

      // Get frustum center
      auto frustumCenter = Vec3(0.0f);
      for (uint32_t j = 0; j < 8; j++) {
        frustumCenter += frustumCorners[j];
      }
      frustumCenter /= 8.0f;

      float radius = 0.0f;
      for (uint32_t j = 0; j < 8; j++) {
        float distance = glm::length(frustumCorners[j] - frustumCenter);
        radius = glm::max(radius, distance);
      }

      // Temp workaround to flickering when rotating camera
      // Sphere radius for lightOrthoMatrix should fix this
      // But radius changes as the camera is rotated which causes flickering
      constexpr float value = 1.0f; // 16.0f;
      radius = std::ceil(radius * value) / value;

      auto maxExtents = Vec3(radius);
      Vec3 minExtents = -maxExtents;

      auto worldTransform = dirLightEntity.GetWorldTransform();
      Vec4 zDir = worldTransform * glm::vec4(0, 0, 1, 0);
      Vec3 lightDir = glm::normalize(Vec3(zDir));
      float CascadeFarPlaneOffset = 10.0f, CascadeNearPlaneOffset = -100.0f; // TODO: Make configurable.
      Mat4 lightOrthoMatrix = glm::ortho(minExtents.x,
        maxExtents.x,
        minExtents.y,
        maxExtents.y,
        CascadeNearPlaneOffset,
        maxExtents.z - minExtents.z - CascadeFarPlaneOffset);
      Mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, Vec3(0.0f, 0.0f, 1.0f));
      auto shadowProj = lightOrthoMatrix * lightViewMatrix;

      constexpr bool stabilizeCascades = true;
      if (stabilizeCascades) {
        // Create the rounding matrix, by projecting the world-space origin and determining
        // the fractional offset in texel space
        glm::vec4 shadowOrigin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        shadowOrigin = shadowProj * shadowOrigin;
        shadowOrigin *= (float)RendererConfig::Get()->DirectShadowsConfig.Size * 0.5f;

        glm::vec4 roundedOrigin = glm::round(shadowOrigin);
        glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
        roundOffset = roundOffset * (2.0f / (float)RendererConfig::Get()->DirectShadowsConfig.Size);
        roundOffset.z = 0.0f;
        roundOffset.w = 0.0f;

        lightOrthoMatrix[3] += roundOffset;
      }

      // Store split distance and matrix in cascade
      cascadesUbo.cascadeSplits[i] = (camera->NearClip + splitDist * clipRange) * -1.0f;
      cascadesUbo.cascadeViewProjMat[i] = lightOrthoMatrix * lightViewMatrix;
    }
  }

  void DefaultRenderPipeline::UpdateLightingData() {
    OX_SCOPED_ZONE;
    for (auto& e : m_SceneLights) {
      auto& lightComponent = e.GetComponent<LightComponent>();
      auto& transformComponent = e.GetComponent<TransformComponent>();
      switch (lightComponent.Type) {
        case LightComponent::LightType::Directional: break;
        case LightComponent::LightType::Point: {
          m_PointLightsData.emplace_back(LightingData{
            Vec4{transformComponent.Translation, lightComponent.Intensity},
            Vec4{lightComponent.Color, lightComponent.Range}, Vec4{transformComponent.Rotation, 1.0f}
          });
        }
        break;
        case LightComponent::LightType::Spot: break;
      }
    }

    if (!m_PointLightsData.empty()) {
      m_RendererData.LightsBuffer.Copy(m_PointLightsData);
      m_PointLightsData.clear();
    }
  }

  void DefaultRenderPipeline::GeneratePrefilter() {
    OX_SCOPED_ZONE;
    const auto vertexLayout = VertexLayout({VertexComponent::POSITION, VertexComponent::NORMAL, VertexComponent::UV});
    Prefilter::GenerateBRDFLUT(m_Resources.LutBRDF);
    Prefilter::GenerateIrradianceCube(m_Resources.IrradianceCube, m_SkyboxCube, vertexLayout, m_Resources.CubeMap->GetDescImageInfo());
    Prefilter::GeneratePrefilteredCube(m_Resources.PrefilteredCube, m_SkyboxCube, vertexLayout, m_Resources.CubeMap->GetDescImageInfo());
  }

  void DefaultRenderPipeline::InitRenderGraph() {
    m_RenderGraph = CreateRef<RenderGraph>();

    std::array<vk::ClearValue, 2> clearValues;
    clearValues[0].color = vk::ClearColorValue(std::array{0.0f, 0.0f, 0.0f, 1.0f});
    clearValues[1].depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

    RenderGraphPass depthPrePass(
      "Depth Pre Pass",
      {&m_RendererContext.DepthPassCommandBuffer},
      &m_Pipelines.DepthPrePassPipeline,
      {&m_FrameBuffers.DepthNormalPassFB},
      [this](VulkanCommandBuffer& commandBuffer, int32_t) {
        OX_SCOPED_ZONE_N("DepthPrePass");
        OX_TRACE_GPU(commandBuffer.Get(), "Depth Pre Pass")
        commandBuffer.SetViwportWindow().SetScissorWindow();
        for (const auto& mesh : m_MeshDrawList) {
          if (!mesh.MeshGeometry)
            continue;

          RenderMesh(mesh,
            m_RendererContext.DepthPassCommandBuffer.Get(),
            m_Pipelines.DepthPrePassPipeline,
            [&](const Mesh::Primitive* part) {
              const auto& material = mesh.Materials[part->materialIndex];
              const auto& layout = m_Pipelines.DepthPrePassPipeline.GetPipelineLayout();
              commandBuffer.PushConstants(layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &mesh.Transform);
              commandBuffer.PushConstants(layout, vk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), sizeof Material::Parameters, &material->Parameters);

              m_Pipelines.DepthPrePassPipeline.BindDescriptorSets(commandBuffer.Get(), {m_DepthDescriptorSet.Get(), material->DepthDescriptorSet.Get()});
              return true;
            });
        }
      },
      {clearValues},
      &VulkanContext::VulkanQueue.GraphicsQueue);
    m_RenderGraph->AddRenderPass(depthPrePass);

    RenderGraphPass directShadowDepthPass(
      "Direct Shadow Depth Pass",
      {&m_RendererContext.DirectShadowCommandBuffer},
      &m_Pipelines.DirectShadowDepthPipeline,
      {
        {
          &m_FrameBuffers.DirectionalCascadesFB[0], &m_FrameBuffers.DirectionalCascadesFB[1],
          &m_FrameBuffers.DirectionalCascadesFB[2], &m_FrameBuffers.DirectionalCascadesFB[3]
        }
      },
      [this](VulkanCommandBuffer& commandBuffer, int32_t framebufferIndex) {
        OX_SCOPED_ZONE_N("DirectShadowDepthPass");
        OX_TRACE_GPU(commandBuffer.Get(), "Direct Shadow Depth Pass")
        commandBuffer.SetViewport(vk::Viewport{
          0, 0, (float)RendererConfig::Get()->DirectShadowsConfig.Size,
          (float)RendererConfig::Get()->DirectShadowsConfig.Size, 0, 1
        }).SetScissor(vk::Rect2D{
          {}, {RendererConfig::Get()->DirectShadowsConfig.Size, RendererConfig::Get()->DirectShadowsConfig.Size,}
        });
        for (const auto& e : m_SceneLights) {
          const auto& lightComponent = e.GetComponent<LightComponent>();
          if (lightComponent.Type != LightComponent::LightType::Directional)
            continue;

          UpdateCascades(e, m_RendererContext.CurrentCamera, m_RendererData.UBO_DirectShadow);
          m_RendererData.DirectShadowBuffer.Copy(&m_RendererData.UBO_DirectShadow, sizeof m_RendererData.UBO_DirectShadow);

          for (const auto& mesh : m_MeshDrawList) {
            if (!mesh.MeshGeometry)
              continue;
            m_Pipelines.DirectShadowDepthPipeline.BindDescriptorSets(commandBuffer.Get(), {m_ShadowDepthDescriptorSet.Get()});
            struct PushConst {
              glm::mat4 modelMatrix{};
              uint32_t cascadeIndex = 0;
            } pushConst;
            pushConst.modelMatrix = mesh.Transform;
            pushConst.cascadeIndex = framebufferIndex;
            const auto& layout = m_Pipelines.DirectShadowDepthPipeline.GetPipelineLayout();
            commandBuffer.PushConstants(layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(PushConst), &pushConst);
            RenderMesh(mesh,
              commandBuffer.Get(),
              m_Pipelines.DirectShadowDepthPipeline,
              [&](const Mesh::Primitive*) {
                return true;
              }
            );
          }
        }
      },
      {vk::ClearDepthStencilValue{1.0f, 0}, vk::ClearColorValue(std::array{0.0f, 0.0f, 0.0f, 1.0f})},
      &VulkanContext::VulkanQueue.GraphicsQueue);
    directShadowDepthPass.SetRenderArea(vk::Rect2D{
      {}, {RendererConfig::Get()->DirectShadowsConfig.Size, RendererConfig::Get()->DirectShadowsConfig.Size},
    }).AddToGraph(m_RenderGraph);

    RenderGraphPass ssaoPass(
      "SSAO Pass",
      {&m_RendererContext.SSAOCommandBuffer},
      &m_Pipelines.SSAOPassPipeline,
      {},
      [this](const VulkanCommandBuffer& commandBuffer, int32_t) {
        OX_SCOPED_ZONE_N("SSAOPass");
        OX_TRACE_GPU(commandBuffer.Get(), "SSAO Pass")
        m_Pipelines.SSAOPassPipeline.BindPipeline(commandBuffer.Get());
        m_Pipelines.SSAOPassPipeline.BindDescriptorSets(commandBuffer.Get(), {m_SSAODescriptorSet.Get()});
        commandBuffer.Dispatch((Window::GetWidth() + 8 - 1) / 8, (Window::GetHeight() + 8 - 1) / 8, 1);
      },
      {clearValues},
      &VulkanContext::VulkanQueue.GraphicsQueue);
    ssaoPass
     .RunWithCondition(RendererConfig::Get()->SSAOConfig.Enabled)
     .AddInnerPass(RenderGraphPass(
        "SSAO Blur Pass",
        {},
        &m_Pipelines.GaussianBlurPipeline,
        {},
        [this](VulkanCommandBuffer& commandBuffer, int32_t) {
          OX_SCOPED_ZONE_N("SSAO Blur Pass");
          OX_TRACE_GPU(commandBuffer.Get(), "SSAO Blur Pass")
          vk::ImageMemoryBarrier imageMemoryBarrier{};
          imageMemoryBarrier.image = m_FrameBuffers.SSAOPassImage.GetImage();
          imageMemoryBarrier.oldLayout = vk::ImageLayout::eGeneral;
          imageMemoryBarrier.newLayout = vk::ImageLayout::eGeneral;
          imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
          imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
          imageMemoryBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
          imageMemoryBarrier.subresourceRange.levelCount = 1;
          imageMemoryBarrier.subresourceRange.layerCount = 1;
          commandBuffer.Get().pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eComputeShader,
            vk::DependencyFlagBits::eByRegion,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &imageMemoryBarrier);
          const auto& layout = m_Pipelines.GaussianBlurPipeline.GetPipelineLayout();
          m_Pipelines.GaussianBlurPipeline.BindPipeline(commandBuffer.Get());
          struct PushConst {
            GLSL_BOOL Horizontal = false;
          } pushConst;
          m_Pipelines.GaussianBlurPipeline.BindDescriptorSets(commandBuffer.Get(), {m_SSAOBlurDescriptorSet.Get()});
          commandBuffer.Dispatch((Window::GetWidth() + 8 - 1) / 8, (Window::GetHeight() + 8 - 1) / 8, 1);

          pushConst.Horizontal = true;
          commandBuffer.PushConstants(layout, vk::ShaderStageFlagBits::eCompute, 0, 4, &pushConst);
          m_Pipelines.GaussianBlurPipeline.BindDescriptorSets(commandBuffer.Get(), {m_SSAOBlurDescriptorSet.Get()});
          commandBuffer.Dispatch((Window::GetWidth() + 8 - 1) / 8, (Window::GetHeight() + 8 - 1) / 8, 1);
        }
      )).AddToGraphCompute(m_RenderGraph);

    RenderGraphPass pbrPass(
      "PBR Pass",
      {&m_RendererContext.PBRPassCommandBuffer},
      &m_Pipelines.SkyboxPipeline,
      {&m_FrameBuffers.PBRPassFB},
      [this](VulkanCommandBuffer& commandBuffer, int32_t) {
        OX_SCOPED_ZONE_N("PBRPass");
        OX_TRACE_GPU(commandBuffer.Get(), "PBR Pass")
        commandBuffer.SetViwportWindow().SetScissorWindow();

        //Skybox pass
        m_Pipelines.SkyboxPipeline.BindPipeline(commandBuffer.Get());
        m_Pipelines.SkyboxPipeline.BindDescriptorSets(commandBuffer.Get(), {m_SkyboxDescriptorSet.Get()});
        const auto& skyboxLayout = m_Pipelines.SkyboxPipeline.GetPipelineLayout();
        commandBuffer.PushConstants(skyboxLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &m_RendererContext.CurrentCamera->SkyboxView);
        m_SkyboxCube.Draw(commandBuffer.Get());

        //PBR pipeline
        for (const auto& mesh : m_MeshDrawList) {
          if (!mesh.MeshGeometry)
            continue;

          RenderMesh(mesh,
            commandBuffer.Get(),
            m_Pipelines.PBRPipeline,
            [&](const Mesh::Primitive* part) {
              const auto& material = mesh.Materials[part->materialIndex];
              if (material->AlphaMode == Material::AlphaMode::Blend) {
                m_TransparentMeshDrawList.emplace_back(mesh);
                return false;
              }
              const auto& layout = m_Pipelines.PBRPipeline.GetPipelineLayout();
              commandBuffer.PushConstants(layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &mesh.Transform);
              commandBuffer.PushConstants(layout, vk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), sizeof Material::Parameters, &material->Parameters);

              m_Pipelines.PBRPipeline.BindDescriptorSets(commandBuffer.Get(), {Material::s_DescriptorSet.Get(), material->MaterialDescriptorSet.Get()}, 0);
              return true;
            });
        }
        m_ForceUpdateMaterials = false;
        m_MeshDrawList.clear();

        // Transparency pass
        for (const auto& mesh : m_TransparentMeshDrawList) {
          if (!mesh.MeshGeometry)
            continue;

          RenderMesh(mesh,
            commandBuffer.Get(),
            m_Pipelines.PBRBlendPipeline,
            [&](const Mesh::Primitive* part) {
              const auto& material = mesh.Materials[part->materialIndex];
              const auto& layout = m_Pipelines.PBRBlendPipeline.GetPipelineLayout();
              commandBuffer.PushConstants(layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &mesh.Transform);
              commandBuffer.PushConstants(layout, vk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), sizeof Material::Parameters, &material->Parameters);

              m_Pipelines.PBRBlendPipeline.BindDescriptorSets(commandBuffer.Get(), {Material::s_DescriptorSet.Get(), material->MaterialDescriptorSet.Get()}, 0);
              return true;
            });
        }
        m_TransparentMeshDrawList.clear();

        // Depth-tested debug renderer pass
        auto& shapes = DebugRenderer::GetInstance()->GetShapes();

        struct DebugPassData {
          Mat4 ViewProjection = {};
          Mat4 Model = {};
          Vec4 Color = {};
        } pushConst;

        m_Pipelines.DebugRenderPipeline.BindPipeline(commandBuffer.Get());
        const auto& debugLayout = m_Pipelines.DebugRenderPipeline.GetPipelineLayout();

        pushConst.ViewProjection = m_RendererContext.CurrentCamera->GetProjectionMatrixFlipped() * m_RendererContext.CurrentCamera->GetViewMatrix();
        for (auto& shape : shapes) {
          pushConst.Model = shape.ModelMatrix;
          pushConst.Color = shape.Color;
          commandBuffer.PushConstants(debugLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(DebugPassData), &pushConst);
          shape.Mesh->Draw(commandBuffer.Get());
        }

        DebugRenderer::Reset(false);
      },
      {clearValues},
      &VulkanContext::VulkanQueue.GraphicsQueue);

    pbrPass.AddInnerPass(RenderGraphPass(
      "Debug Renderer NDT Pass",
      {&m_RendererContext.PBRPassCommandBuffer},
      &m_Pipelines.DebugRenderPipelineNDT,
      {&m_FrameBuffers.PBRPassFB},
      [this](VulkanCommandBuffer& commandBuffer, int32_t) {
        // Not depth tested pass

        auto& shapes = DebugRenderer::GetInstance()->GetShapes(false);

        struct DebugPassData {
          Mat4 ViewProjection = {};
          Mat4 Model = {};
          Vec4 Color = {};
        } pushConst;

        m_Pipelines.DebugRenderPipelineNDT.BindPipeline(commandBuffer.Get());
        const auto& debugLayout = m_Pipelines.DebugRenderPipelineNDT.GetPipelineLayout();

        pushConst.ViewProjection = m_RendererContext.CurrentCamera->GetProjectionMatrixFlipped() * m_RendererContext.CurrentCamera->GetViewMatrix();
        for (auto& shape : shapes) {
          pushConst.Model = shape.ModelMatrix;
          pushConst.Color = shape.Color;
          commandBuffer.PushConstants(debugLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(DebugPassData), &pushConst);
          shape.Mesh->Draw(commandBuffer.Get());
        }

        DebugRenderer::Reset();
      },
      clearValues,
      &VulkanContext::VulkanQueue.GraphicsQueue
    ));
    pbrPass.AddToGraph(m_RenderGraph);

    RenderGraphPass ssrPass(
      "SSR Pass",
      {&m_RendererContext.SSRCommandBuffer},
      &m_Pipelines.SSRPipeline,
      {},
      [this](const VulkanCommandBuffer& commandBuffer, int32_t) {
        OX_SCOPED_ZONE_N("SSR Pass");
        OX_TRACE_GPU(commandBuffer.Get(), "SSR Pass")
        m_Pipelines.SSRPipeline.BindPipeline(commandBuffer.Get());
        m_Pipelines.SSRPipeline.BindDescriptorSets(commandBuffer.Get(), {m_SSRDescriptorSet.Get()});
        commandBuffer.Dispatch((Window::GetWidth() + 8 - 1) / 8, (Window::GetHeight() + 8 - 1) / 8, 1);
      },
      clearValues,
      &VulkanContext::VulkanQueue.GraphicsQueue);
    ssrPass.RunWithCondition(RendererConfig::Get()->SSRConfig.Enabled)
           .AddToGraphCompute(m_RenderGraph);

    RenderGraphPass bloomPass(
      "Bloom Pass",
      {&m_RendererContext.BloomPassCommandBuffer},
      &m_Pipelines.BloomPipeline,
      {},
      [this](VulkanCommandBuffer& commandBuffer, int32_t) {
        OX_SCOPED_ZONE_N("BloomPass");
        OX_TRACE_GPU(commandBuffer.Get(), "Bloom Pass")
        struct PushConst {
          Vec4 Params = {
            RendererConfig::Get()->BloomConfig.Threshold,
            RendererConfig::Get()->BloomConfig.Clamp,
            {}, {}
          };
          IVec2 Stage{};
        } pushConst;
        enum STAGES {
          PREFILTER_STAGE  = 0,
          DOWNSAMPLE_STAGE = 1,
          UPSAMPLE_STAGE   = 2,
        };
        const int32_t lodCount = (int32_t)m_FrameBuffers.BloomDownsampleImage.GetDesc().MipLevels - 3;

        const auto& layout = m_Pipelines.BloomPipeline.GetPipelineLayout();

        vk::ImageMemoryBarrier imageMemoryBarrier{};
        imageMemoryBarrier.image = m_FrameBuffers.BloomDownsampleImage.GetImage();
        imageMemoryBarrier.oldLayout = vk::ImageLayout::eGeneral;
        imageMemoryBarrier.newLayout = vk::ImageLayout::eGeneral;
        imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
        imageMemoryBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        imageMemoryBarrier.subresourceRange.levelCount = lodCount;
        imageMemoryBarrier.subresourceRange.layerCount = 1;
        //imageMemoryBarrier.subresourceRange.baseMipLevel = 0;

        m_Pipelines.BloomPipeline.BindPipeline(commandBuffer.Get());
        pushConst.Stage.x = PREFILTER_STAGE;
        commandBuffer.PushConstants(layout, vSS::eCompute, 0, sizeof pushConst, &pushConst);

        m_Pipelines.BloomPipeline.BindDescriptorSets(commandBuffer.Get(), {m_BloomDescriptorSet.Get()});
        IVec3 size = VulkanImage::GetMipMapLevelSize(m_FrameBuffers.BloomDownsampleImage.GetWidth(), m_FrameBuffers.BloomDownsampleImage.GetHeight(), 1, 0);
        commandBuffer.Dispatch((size.x + 8 - 1) / 8, (size.y + 8 - 1) / 8, 1);
        commandBuffer.Get().pipelineBarrier(vPS::eComputeShader, vPS::eComputeShader, vDF::eByRegion, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

        // Downsample
        pushConst.Stage.x = DOWNSAMPLE_STAGE;
        commandBuffer.PushConstants(layout, vSS::eCompute, 0, sizeof pushConst, &pushConst);
        for (int32_t i = 1; i < lodCount; i++) {
          size = VulkanImage::GetMipMapLevelSize(m_FrameBuffers.BloomDownsampleImage.GetWidth(), m_FrameBuffers.BloomDownsampleImage.GetHeight(), 1, i);

          // Set lod in shader
          pushConst.Stage.y = i - 1;
          //imageMemoryBarrier.subresourceRange.baseMipLevel = i;

          m_Pipelines.BloomPipeline.BindDescriptorSets(commandBuffer.Get(), {m_BloomDescriptorSet.Get()});
          commandBuffer.PushConstants(layout, vSS::eCompute, 0, sizeof pushConst, &pushConst);
          commandBuffer.Dispatch((size.x + 8 - 1) / 8, (size.y + 8 - 1) / 8, 1);
          commandBuffer.Get().pipelineBarrier(vPS::eComputeShader, vPS::eComputeShader, vDF::eByRegion, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        }

        // Upsample
        pushConst.Stage.x = UPSAMPLE_STAGE;

        size = VulkanImage::GetMipMapLevelSize(m_FrameBuffers.BloomUpsampleImage.GetWidth(), m_FrameBuffers.BloomUpsampleImage.GetHeight(), 1, lodCount - 1);
        pushConst.Stage.y = lodCount - 1;
        commandBuffer.PushConstants(layout, vSS::eCompute, 0, sizeof pushConst, &pushConst);

        imageMemoryBarrier.image = m_FrameBuffers.BloomUpsampleImage.GetImage();
        //imageMemoryBarrier.subresourceRange.baseMipLevel = lodCount - 1;
        commandBuffer.Dispatch((size.x + 8 - 1) / 8, (size.y + 8 - 1) / 8, 1);
        commandBuffer.Get().pipelineBarrier(vPS::eComputeShader, vPS::eComputeShader, {}, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

        for (int32_t i = lodCount - 1; i >= 0; i--) {
          size = VulkanImage::GetMipMapLevelSize(m_FrameBuffers.BloomUpsampleImage.GetWidth(), m_FrameBuffers.BloomUpsampleImage.GetHeight(), 1, i);

          // Set lod in shader
          pushConst.Stage.y = i;

          //imageMemoryBarrier.subresourceRange.baseMipLevel = i;
          commandBuffer.PushConstants(layout, vSS::eCompute, 0, sizeof pushConst, &pushConst);
          commandBuffer.Dispatch((size.x + 8 - 1) / 8, (size.y + 8 - 1) / 8, 1);
          commandBuffer.Get().pipelineBarrier(vPS::eComputeShader, vPS::eComputeShader, vDF::eByRegion, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        }
      },
      clearValues,
      &VulkanContext::VulkanQueue.GraphicsQueue);
    bloomPass.RunWithCondition(RendererConfig::Get()->BloomConfig.Enabled)
             .AddToGraphCompute(m_RenderGraph);

    RenderGraphPass dofPass(
      "DepthOfField Pass",
      {&m_RendererContext.DepthOfFieldCommandBuffer},
      &m_Pipelines.DepthOfFieldPipeline,
      {},
      [this](const VulkanCommandBuffer& commandBuffer, int32_t) {
        OX_SCOPED_ZONE_N("DepthOfField Pass");
        OX_TRACE_GPU(commandBuffer.Get(), "DepthOfField Pass");
        m_Pipelines.DepthOfFieldPipeline.BindPipeline(commandBuffer.Get());
        m_Pipelines.DepthOfFieldPipeline.BindDescriptorSets(commandBuffer.Get(), {m_DepthOfFieldDescriptorSet.Get()});
        commandBuffer.Dispatch((Window::GetWidth() + 8 - 1) / 8, (Window::GetHeight() + 8 - 1) / 8, 6);
      },
      clearValues,
      &VulkanContext::VulkanQueue.GraphicsQueue
    );
    dofPass.AddToGraphCompute(m_RenderGraph);

    RenderGraphPass atmospherePass(
      "Atmosphere Pass",
      {&m_RendererContext.AtmosphereCommandBuffer},
      &m_Pipelines.AtmospherePipeline,
      {},
      [this](const VulkanCommandBuffer& commandBuffer, int32_t) {
        OX_SCOPED_ZONE_N("Atmosphere Pass");
        OX_TRACE_GPU(commandBuffer.Get(), "Atmosphere Pass")
        vk::ImageSubresourceRange subresourceRange;
        subresourceRange.aspectMask = m_FrameBuffers.AtmosphereImage.GetDesc().AspectFlag;
        subresourceRange.levelCount = 1;
        subresourceRange.layerCount = 6;
        const vk::ImageMemoryBarrier imageMemoryBarrier = {
          {},
          {},
          vk::ImageLayout::eUndefined,
          vk::ImageLayout::eGeneral,
          0,
          0,
          m_FrameBuffers.AtmosphereImage.GetImage(),
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
        m_Pipelines.AtmospherePipeline.BindPipeline(commandBuffer.Get());
        m_Pipelines.AtmospherePipeline.BindDescriptorSets(commandBuffer.Get(), {m_AtmosphereDescriptorSet.Get()});
        commandBuffer.Dispatch((Window::GetWidth() + 8 - 1) / 8, (Window::GetHeight() + 8 - 1) / 8, 6);
      },
      clearValues,
      &VulkanContext::VulkanQueue.GraphicsQueue);
    //atmospherePass.AddToGraphCompute(renderGraph);

    RenderGraphPass compositePass(
      "Composite Pass",
      {&m_RendererContext.CompositeCommandBuffer},
      &m_Pipelines.CompositePipeline,
      {},
      [this](VulkanCommandBuffer& commandBuffer, int32_t) {
        OX_SCOPED_ZONE_N("Composite Pass");
        OX_TRACE_GPU(commandBuffer.Get(), "Composite Pass")
        m_Pipelines.CompositePipeline.BindPipeline(commandBuffer.Get());
        m_Pipelines.CompositePipeline.BindDescriptorSets(commandBuffer.Get(), {m_CompositeDescriptorSet.Get()});
        commandBuffer.Dispatch((Window::GetWidth() + 8 - 1) / 8, (Window::GetHeight() + 8 - 1) / 8, 1);
      },
      clearValues,
      &VulkanContext::VulkanQueue.GraphicsQueue);
    compositePass.AddToGraphCompute(m_RenderGraph);

    RenderGraphPass ppPass({
      "PP Pass",
      {&m_RendererContext.PostProcessCommandBuffer},
      &m_Pipelines.PostProcessPipeline,
      {&m_FrameBuffers.PostProcessPassFB},
      [this](VulkanCommandBuffer& commandBuffer, int32_t) {
        OX_SCOPED_ZONE_N("PP Pass");
        OX_TRACE_GPU(commandBuffer.Get(), "PP Pass")
        commandBuffer.SetFlippedViwportWindow().SetScissorWindow();
        m_Pipelines.PostProcessPipeline.BindPipeline(commandBuffer.Get());
        m_Pipelines.PostProcessPipeline.BindDescriptorSets(commandBuffer.Get(), {m_PostProcessDescriptorSet.Get()});
        VulkanRenderer::DrawFullscreenQuad(commandBuffer.Get(), true);
      },
      clearValues, &VulkanContext::VulkanQueue.GraphicsQueue
    });
    ppPass.AddToGraph(m_RenderGraph);

    RenderGraphPass frustumPass(
      "Frustum Pass",
      {&m_RendererContext.FrustumCommandBuffer},
      {},
      {},
      [this](const VulkanCommandBuffer& commandBuffer, int32_t) {
        OX_SCOPED_ZONE_N("FrustumPass");
        OX_TRACE_GPU(commandBuffer.Get(), "Frustum Pass")
        m_Pipelines.FrustumGridPipeline.BindPipeline(commandBuffer.Get());
        m_Pipelines.FrustumGridPipeline.BindDescriptorSets(m_RendererContext.FrustumCommandBuffer.Get(), {m_LightListDescriptorSet.Get()});
        commandBuffer.Dispatch(m_RendererData.UBO_PbrPassParams.numThreadGroups.x, m_RendererData.UBO_PbrPassParams.numThreadGroups.y, 1);
      },
      {},
      &VulkanContext::VulkanQueue.GraphicsQueue);
    //m_RenderGraph->AddComputePass(frustumPass);

    RenderGraphPass lightListPass(
      "Light List Pass",
      {&m_RendererContext.LightListCommandBuffer},
      {},
      {},
      [this](const VulkanCommandBuffer& commandBuffer, int32_t) {
        OX_SCOPED_ZONE_N("Light List Pass");
        OX_TRACE_GPU(commandBuffer.Get(), "Light List Pass")
        std::vector<vk::BufferMemoryBarrier> barriers1;
        std::vector<vk::BufferMemoryBarrier> barriers2;
        static bool initalizedBarries = false;
        if (!initalizedBarries) {
          barriers1 = {
            m_RendererData.LightsBuffer.CreateMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
            m_RendererData.LighIndexBuffer.CreateMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
            m_RendererData.LighGridBuffer.CreateMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
          };

          barriers2 = {
            m_RendererData.LightsBuffer.CreateMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
            m_RendererData.LighIndexBuffer.CreateMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
            m_RendererData.LighGridBuffer.CreateMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
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
        m_Pipelines.LightListPipeline.BindPipeline(commandBuffer.Get());
        m_Pipelines.LightListPipeline.BindDescriptorSets(commandBuffer.Get(), {m_LightListDescriptorSet.Get()});
        commandBuffer.Dispatch(m_RendererData.UBO_PbrPassParams.numThreadGroups.x,
          m_RendererData.UBO_PbrPassParams.numThreadGroups.y,
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

    //m_RenderGraph->AddComputePass(lightListPass);
  }

  void DefaultRenderPipeline::CreateGraphicsPipelines() {
    OX_SCOPED_ZONE;
    // Create shaders
    auto skyboxShader = ShaderLibrary::CreateShaderAsync(ShaderCI{
      .VertexPath = Resources::GetResourcesPath("Shaders/Skybox.vert"),
      .FragmentPath = Resources::GetResourcesPath("Shaders/Skybox.frag"),
      .EntryPoint = "main", .Name = "Skybox"
    });
    auto pbrShader = ShaderLibrary::CreateShaderAsync(ShaderCI{
      .VertexPath = Resources::GetResourcesPath("Shaders/PBRTiled.vert"),
      .FragmentPath = Resources::GetResourcesPath("Shaders/PBRTiled.frag"),
      .EntryPoint = "main", .Name = "PBRTiled",
    });
    auto unlitShader = ShaderLibrary::CreateShaderAsync(ShaderCI{
      .VertexPath = Resources::GetResourcesPath("Shaders/Unlit.vert"),
      .FragmentPath = Resources::GetResourcesPath("Shaders/Unlit.frag"),
      .EntryPoint = "main", .Name = "Unlit",
    });
    auto directShadowShader = ShaderLibrary::CreateShaderAsync(ShaderCI{
      .VertexPath = Resources::GetResourcesPath("Shaders/DirectShadowDepthPass.vert"),
      .FragmentPath = Resources::GetResourcesPath("Shaders/DirectShadowDepthPass.frag"),
      .EntryPoint = "main", .Name = "DirectShadowDepth"
    });
    auto depthPassShader = ShaderLibrary::CreateShaderAsync(ShaderCI{
      .VertexPath = Resources::GetResourcesPath("Shaders/DepthNormalPass.vert"),
      .FragmentPath = Resources::GetResourcesPath("Shaders/DepthNormalPass.frag"),
      .EntryPoint = "main", .Name = "DepthPass"
    });
    auto ssaoShader = ShaderLibrary::CreateShaderAsync(ShaderCI{
      .EntryPoint = "main",
      .Name = "SSAO",
      .ComputePath = Resources::GetResourcesPath("Shaders/SSAO.comp"),
    });
    auto bloomShader = ShaderLibrary::CreateShaderAsync(ShaderCI{
      .EntryPoint = "main",
      .Name = "Bloom",
      .ComputePath = Resources::GetResourcesPath("Shaders/Bloom.comp"),
    });
    auto ssrShader = ShaderLibrary::CreateShaderAsync(ShaderCI{
      .EntryPoint = "main",
      .Name = "SSR",
      .ComputePath = Resources::GetResourcesPath("Shaders/SSR.comp"),
    });
    auto atmosphereShader = ShaderLibrary::CreateShaderAsync(ShaderCI{
      .EntryPoint = "main",
      .Name = "AtmosphericScattering",
      .ComputePath = Resources::GetResourcesPath("Shaders/AtmosphericScattering.comp"),
    });
    auto compositeShader = ShaderLibrary::CreateShaderAsync(ShaderCI{
      .EntryPoint = "main",
      .Name = "Composite",
      .ComputePath = Resources::GetResourcesPath("Shaders/Composite.comp"),
    });
    auto postProcessShader = ShaderLibrary::CreateShaderAsync(ShaderCI{
      .VertexPath = Resources::GetResourcesPath("Shaders/PostProcess.vert"),
      .FragmentPath = Resources::GetResourcesPath("Shaders/PostProcess.frag"),
      .EntryPoint = "main", .Name = "PostProcess"
    });
    auto frustumGridShader = ShaderLibrary::CreateShaderAsync(ShaderCI{
      .EntryPoint = "main", .Name = "FrustumGrid",
      .ComputePath = Resources::GetResourcesPath("Shaders/ComputeFrustumGrid.comp"),
    });
    auto lightListShader = ShaderLibrary::CreateShaderAsync(ShaderCI{
      .EntryPoint = "main", .Name = "LightList",
      .ComputePath = Resources::GetResourcesPath("Shaders/ComputeLightList.comp"),
    });
    auto depthOfFieldShader = ShaderLibrary::CreateShaderAsync(ShaderCI{
      .EntryPoint = "main",
      .Name = "DepthOfField",
      .ComputePath = Resources::GetResourcesPath("Shaders/DepthOfField.comp"),
    });
    auto gaussianBlurShader = ShaderLibrary::CreateShaderAsync(ShaderCI{
      .EntryPoint = "main",
      .Name = "GaussianBlur",
      .ComputePath = Resources::GetResourcesPath("Shaders/GaussianBlur.comp"),
    });

    PipelineDescription pbrPipelineDesc{};
    pbrPipelineDesc.Name = "Skybox Pipeline";
    pbrPipelineDesc.Shader = skyboxShader.get();
    pbrPipelineDesc.ColorAttachmentCount = 1;
    pbrPipelineDesc.RenderTargets[0].Format = VulkanRenderer::SwapChain.m_ImageFormat;
    pbrPipelineDesc.RasterizerDesc.CullMode = vk::CullModeFlagBits::eNone;
    pbrPipelineDesc.RasterizerDesc.DepthBias = false;
    pbrPipelineDesc.RasterizerDesc.FrontCounterClockwise = true;
    pbrPipelineDesc.RasterizerDesc.DepthClampEnable = false;
    pbrPipelineDesc.DepthDesc.DepthWriteEnable = false;
    pbrPipelineDesc.DepthDesc.DepthReferenceAttachment = 1;
    pbrPipelineDesc.DepthDesc.DepthEnable = true;
    pbrPipelineDesc.DepthDesc.CompareOp = vk::CompareOp::eLessOrEqual;
    pbrPipelineDesc.DepthDesc.FrontFace.StencilFunc = vk::CompareOp::eNever;
    pbrPipelineDesc.DepthDesc.BackFace.StencilFunc = vk::CompareOp::eNever;
    pbrPipelineDesc.DepthDesc.MinDepthBound = 0;
    pbrPipelineDesc.DepthDesc.MaxDepthBound = 0;
    pbrPipelineDesc.DepthDesc.DepthStenctilFormat = vk::Format::eD32Sfloat;
    pbrPipelineDesc.DynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    pbrPipelineDesc.VertexInputState = VertexInputDescription(VertexLayout({
      VertexComponent::POSITION, VertexComponent::NORMAL, VertexComponent::UV
    }));
    m_Pipelines.SkyboxPipeline.CreateGraphicsPipeline(pbrPipelineDesc);

    pbrPipelineDesc.Name = "PBR Pipeline";
    pbrPipelineDesc.Shader = pbrShader.get();
    pbrPipelineDesc.DepthDesc.DepthWriteEnable = true;
    pbrPipelineDesc.DepthDesc.DepthEnable = true;
    pbrPipelineDesc.RasterizerDesc.CullMode = vk::CullModeFlagBits::eBack;
    m_Pipelines.PBRPipeline.CreateGraphicsPipeline(pbrPipelineDesc);

    pbrPipelineDesc.Name = "PBR Blend Pipeline";
    pbrPipelineDesc.BlendStateDesc.RenderTargets[0].BlendEnable = true;
    pbrPipelineDesc.BlendStateDesc.RenderTargets[0].SrcBlend = vk::BlendFactor::eSrcAlpha;
    pbrPipelineDesc.BlendStateDesc.RenderTargets[0].DestBlend = vk::BlendFactor::eOneMinusSrcAlpha;
    pbrPipelineDesc.BlendStateDesc.RenderTargets[0].BlendOp = vk::BlendOp::eAdd;
    pbrPipelineDesc.BlendStateDesc.RenderTargets[0].BlendOpAlpha = vk::BlendOp::eAdd;
    m_Pipelines.PBRBlendPipeline.CreateGraphicsPipeline(pbrPipelineDesc);

    PipelineDescription depthpassdescription;
    depthpassdescription.Name = "Depth Pass Pipeline";
    depthpassdescription.Shader = depthPassShader.get();
    depthpassdescription.DepthDesc.BoundTest = true;
    depthpassdescription.DepthDesc.DepthReferenceAttachment = 1;
    depthpassdescription.DepthAttachmentLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
    depthpassdescription.VertexInputState = VertexInputDescription(VertexLayout({
      VertexComponent::POSITION,
      VertexComponent::NORMAL,
      VertexComponent::UV,
      VertexComponent::TANGENT
    }));
    m_Pipelines.DepthPrePassPipeline.CreateGraphicsPipeline(depthpassdescription);

    PipelineDescription directShadowPipelineDescription;
    directShadowPipelineDescription.VertexInputState = VertexInputDescription(VertexLayout({
      VertexComponent::POSITION, VertexComponent::NORMAL, VertexComponent::UV
    }));
    directShadowPipelineDescription.Name = "Direct Shadow Pipeline";
    directShadowPipelineDescription.Shader = directShadowShader.get();
    directShadowPipelineDescription.ColorAttachmentCount = 0;
    directShadowPipelineDescription.RasterizerDesc.CullMode = vk::CullModeFlagBits::eFront;
    directShadowPipelineDescription.DepthDesc.DepthEnable = true;
    directShadowPipelineDescription.RasterizerDesc.DepthClampEnable = true;
    directShadowPipelineDescription.RasterizerDesc.FrontCounterClockwise = false;
    directShadowPipelineDescription.DepthDesc.BoundTest = false;
    directShadowPipelineDescription.DepthDesc.CompareOp = vk::CompareOp::eLessOrEqual;
    directShadowPipelineDescription.DepthDesc.MaxDepthBound = 0; // SUSSY
    directShadowPipelineDescription.DepthDesc.DepthReferenceAttachment = 0;
    directShadowPipelineDescription.DepthDesc.BackFace.StencilFunc = vk::CompareOp::eAlways;
    directShadowPipelineDescription.DepthAttachmentLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
    m_Pipelines.DirectShadowDepthPipeline.CreateGraphicsPipeline(directShadowPipelineDescription);

    PipelineDescription debugDescription;
    debugDescription.Name = "Debug Renderer Pipeline";
    debugDescription.Shader = unlitShader.get();
    debugDescription.ColorAttachmentCount = 1;
    debugDescription.DepthDesc.DepthEnable = true;
    debugDescription.DepthDesc.DepthReferenceAttachment = 1;
    debugDescription.RasterizerDesc.CullMode = vk::CullModeFlagBits::eBack;
    debugDescription.RasterizerDesc.FillMode = vk::PolygonMode::eLine;
    debugDescription.RasterizerDesc.LineWidth = 1.0f;
    debugDescription.VertexInputState = VertexInputDescription(VertexLayout({
      VertexComponent::POSITION, VertexComponent::NORMAL, VertexComponent::UV, VertexComponent::COLOR
    }));
    m_Pipelines.DebugRenderPipeline.CreateGraphicsPipeline(debugDescription);

    debugDescription.RenderTargets[0].InitialLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    debugDescription.RenderTargets[0].LoadOp = vk::AttachmentLoadOp::eLoad;
    m_Pipelines.DebugRenderPipelineNDT.CreateGraphicsPipeline(debugDescription);

    PipelineDescription ssaoDescription;
    ssaoDescription.Name = "SSAO Pipeline";
    ssaoDescription.RenderTargets[0].Format = vk::Format::eR8Unorm;
    ssaoDescription.DepthDesc.DepthEnable = false;
    ssaoDescription.SubpassDescription[0].DstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    ssaoDescription.SubpassDescription[0].SrcAccessMask = {};
    ssaoDescription.Shader = ssaoShader.get();
    m_Pipelines.SSAOPassPipeline.CreateComputePipeline(ssaoDescription);

    PipelineDescription gaussianBlur;
    gaussianBlur.Name = "GaussianBlur Pipeline";
    gaussianBlur.DepthDesc.DepthEnable = false;
    gaussianBlur.Shader = gaussianBlurShader.get();
    m_Pipelines.GaussianBlurPipeline.CreateComputePipeline(gaussianBlur);

    PipelineDescription bloomDesc;
    bloomDesc.Name = "Bloom Pipeline";
    bloomDesc.ColorAttachmentCount = 1;
    bloomDesc.DepthDesc.DepthEnable = false;
    bloomDesc.Shader = bloomShader.get();
    m_Pipelines.BloomPipeline.CreateComputePipeline(bloomDesc);

    PipelineDescription ssrDesc;
    ssrDesc.Name = "SSR Pipeline";
    ssrDesc.Shader = ssrShader.get();
    m_Pipelines.SSRPipeline.CreateComputePipeline(ssrDesc);

    PipelineDescription atmDesc;
    atmDesc.Name = "Atmosphere Pipeline";
    atmDesc.Shader = atmosphereShader.get();
    m_Pipelines.AtmospherePipeline.CreateComputePipeline(atmDesc);

    PipelineDescription depthOfField;
    depthOfField.Name = "DOF Pipeline";
    depthOfField.Shader = depthOfFieldShader.get();
    m_Pipelines.DepthOfFieldPipeline.CreateComputePipeline(depthOfField);

    PipelineDescription composite;
    composite.DepthDesc.DepthEnable = false;
    composite.Shader = compositeShader.get();
    m_Pipelines.CompositePipeline.CreateComputePipeline(composite);

    PipelineDescription ppPass;
    ppPass.Shader = postProcessShader.get();
    ppPass.RasterizerDesc.CullMode = vk::CullModeFlagBits::eNone;
    ppPass.VertexInputState = VertexInputDescription(VertexLayout({
      VertexComponent::POSITION, VertexComponent::NORMAL, VertexComponent::UV
    }));
    ppPass.DepthDesc.DepthEnable = false;
    m_Pipelines.PostProcessPipeline.CreateGraphicsPipeline(ppPass);

    PipelineDescription computePipelineDesc;
    computePipelineDesc.Shader = frustumGridShader.get();
    m_Pipelines.FrustumGridPipeline.CreateComputePipeline(computePipelineDesc);

    computePipelineDesc.Shader = lightListShader.get();
    m_Pipelines.LightListPipeline.CreateComputePipeline(computePipelineDesc);
  }

  void DefaultRenderPipeline::CreateFramebuffers() {
    OX_SCOPED_ZONE;
    VulkanImageDescription colorImageDesc{};
    colorImageDesc.Format = VulkanRenderer::SwapChain.m_ImageFormat;
    colorImageDesc.UsageFlags = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    colorImageDesc.ImageTiling = vk::ImageTiling::eOptimal;
    colorImageDesc.Width = VulkanRenderer::SwapChain.m_Extent.width;
    colorImageDesc.Height = VulkanRenderer::SwapChain.m_Extent.height;
    colorImageDesc.CreateView = true;
    colorImageDesc.CreateSampler = true;
    colorImageDesc.CreateDescriptorSet = true;
    colorImageDesc.AspectFlag = vk::ImageAspectFlagBits::eColor;
    colorImageDesc.FinalImageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    colorImageDesc.SamplerAddressMode = vk::SamplerAddressMode::eClampToEdge;
    {
      VulkanImageDescription depthImageDesc{};
      depthImageDesc.Format = vk::Format::eD32Sfloat;
      depthImageDesc.Height = VulkanRenderer::SwapChain.m_Extent.height;
      depthImageDesc.Width = VulkanRenderer::SwapChain.m_Extent.width;
      depthImageDesc.UsageFlags = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
      depthImageDesc.ImageTiling = vk::ImageTiling::eOptimal;
      depthImageDesc.CreateSampler = true;
      depthImageDesc.CreateDescriptorSet = true;
      depthImageDesc.DescriptorSetLayout = m_RendererData.ImageDescriptorSetLayout;
      depthImageDesc.CreateView = true;
      depthImageDesc.AspectFlag = vk::ImageAspectFlagBits::eDepth;
      depthImageDesc.FinalImageLayout = vk::ImageLayout::eDepthReadOnlyOptimal;
      depthImageDesc.TransitionLayoutAtCreate = true;
      FramebufferDescription framebufferDescription;
      framebufferDescription.DebugName = "Depth Pass";
      framebufferDescription.RenderPass = m_Pipelines.DepthPrePassPipeline.GetRenderPass().Get();
      framebufferDescription.Width = VulkanRenderer::SwapChain.m_Extent.width;
      framebufferDescription.Height = VulkanRenderer::SwapChain.m_Extent.height;
      framebufferDescription.Extent = &Window::GetWindowExtent();
      framebufferDescription.ImageDescription = {colorImageDesc, depthImageDesc};
      framebufferDescription.OnResize = [this] {
        /* m_LightListDescriptorSet.WriteDescriptorSets[6].pImageInfo = &m_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetDescImageInfo();
         m_LightListDescriptorSet.WriteDescriptorSets[6].pImageInfo = &m_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetDescImageInfo();
         m_LightListDescriptorSet.Update();*/
      };
      m_FrameBuffers.DepthNormalPassFB.CreateFramebuffer(framebufferDescription);

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
      m_Resources.DirectShadowsDepthArray.Create(depthImageDesc);

      framebufferDescription.Width = RendererConfig::Get()->DirectShadowsConfig.Size;
      framebufferDescription.Height = RendererConfig::Get()->DirectShadowsConfig.Size;
      framebufferDescription.DebugName = "Direct Shadow Depth Pass";
      framebufferDescription.RenderPass = m_Pipelines.DirectShadowDepthPipeline.GetRenderPass().Get();
      framebufferDescription.OnResize = [this] {
        m_ShadowDepthDescriptorSet.WriteDescriptorSets[0].pBufferInfo = &m_RendererData.DirectShadowBuffer.GetDescriptor();
        m_ShadowDepthDescriptorSet.Update();
      };
      m_FrameBuffers.DirectionalCascadesFB.resize(SHADOW_MAP_CASCADE_COUNT);
      int baseArrayLayer = 0;
      depthImageDesc.ViewArrayLayerCount = 1;
      for (auto& fb : m_FrameBuffers.DirectionalCascadesFB) {
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
        viewInfo.image = m_Resources.DirectShadowsDepthArray.GetImage();
        const auto& LogicalDevice = VulkanContext::GetDevice();
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
      DepthImageDesc.Width = VulkanRenderer::SwapChain.m_Extent.width;
      DepthImageDesc.Height = VulkanRenderer::SwapChain.m_Extent.height;
      DepthImageDesc.CreateView = true;
      DepthImageDesc.CreateSampler = true;
      DepthImageDesc.CreateDescriptorSet = false;
      DepthImageDesc.AspectFlag = vk::ImageAspectFlagBits::eDepth;
      DepthImageDesc.FinalImageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

      FramebufferDescription framebufferDescription;
      framebufferDescription.DebugName = "PBR Pass";
      framebufferDescription.Width = VulkanRenderer::SwapChain.m_Extent.width;
      framebufferDescription.Height = VulkanRenderer::SwapChain.m_Extent.height;
      framebufferDescription.Extent = &Window::GetWindowExtent();
      framebufferDescription.RenderPass = m_Pipelines.PBRPipeline.GetRenderPass().Get();
      framebufferDescription.ImageDescription = {colorImageDesc, DepthImageDesc};
      m_FrameBuffers.PBRPassFB.CreateFramebuffer(framebufferDescription);
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
      ssaopassimage.SamplerAddressMode = vk::SamplerAddressMode::eClampToEdge;
      ssaopassimage.SamplerBorderColor = vk::BorderColor::eFloatTransparentBlack;
      m_FrameBuffers.SSAOPassImage.Create(ssaopassimage);

      auto updateSSAODescriptorSet = [this] {
        m_SSAODescriptorSet.WriteDescriptorSets[1].pImageInfo = &m_FrameBuffers.DepthNormalPassFB.GetImage()[1].GetDescImageInfo();
        m_SSAODescriptorSet.WriteDescriptorSets[2].pImageInfo = &m_FrameBuffers.SSAOPassImage.GetDescImageInfo();
        m_SSAODescriptorSet.WriteDescriptorSets[3].pImageInfo = &m_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetDescImageInfo();
        m_SSAODescriptorSet.Update();

        m_SSAOBlurDescriptorSet.WriteDescriptorSets[0].pImageInfo = &m_FrameBuffers.SSAOBlurPassImage.GetDescImageInfo();
        m_SSAOBlurDescriptorSet.WriteDescriptorSets[1].pImageInfo = &m_FrameBuffers.SSAOPassImage.GetDescImageInfo();
        m_SSAOBlurDescriptorSet.Update();
      };

      ImagePool::AddToPool(
        &m_FrameBuffers.SSAOPassImage,
        &Window::GetWindowExtent(),
        [this, updateSSAODescriptorSet] {
          updateSSAODescriptorSet();
          m_FrameBuffers.PostProcessPassFB.GetDescription().OnResize();
        });

      m_FrameBuffers.SSAOBlurPassImage.Create(ssaopassimage);

      ImagePool::AddToPool(
        &m_FrameBuffers.SSAOBlurPassImage,
        &Window::GetWindowExtent(),
        [this, updateSSAODescriptorSet] {
          updateSSAODescriptorSet();
          m_FrameBuffers.PostProcessPassFB.GetDescription().OnResize();
        });
    }
    {
      VulkanImageDescription ssrPassImage;
      ssrPassImage.Height = Window::GetHeight();
      ssrPassImage.Width = Window::GetWidth();
      ssrPassImage.CreateDescriptorSet = true;
      ssrPassImage.UsageFlags = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
      ssrPassImage.Format = VulkanRenderer::SwapChain.m_ImageFormat;
      ssrPassImage.FinalImageLayout = vk::ImageLayout::eGeneral;
      ssrPassImage.TransitionLayoutAtCreate = true;
      ssrPassImage.SamplerAddressMode = vk::SamplerAddressMode::eClampToBorder;
      ssrPassImage.SamplerBorderColor = vk::BorderColor::eFloatTransparentBlack;
      m_FrameBuffers.SSRPassImage.Create(ssrPassImage);

      ImagePool::AddToPool(
        &m_FrameBuffers.SSRPassImage,
        &Window::GetWindowExtent(),
        [this] {
          m_SSRDescriptorSet.WriteDescriptorSets[0].pImageInfo = &m_FrameBuffers.SSRPassImage.GetDescImageInfo();
          m_SSRDescriptorSet.WriteDescriptorSets[1].pImageInfo = &m_FrameBuffers.PBRPassFB.GetImage()[0].GetDescImageInfo();
          m_SSRDescriptorSet.WriteDescriptorSets[2].pImageInfo = &m_FrameBuffers.DepthNormalPassFB.GetImage()[1].GetDescImageInfo();
          m_SSRDescriptorSet.WriteDescriptorSets[3].pImageInfo = &m_Resources.CubeMap->GetDescImageInfo();
          m_SSRDescriptorSet.WriteDescriptorSets[4].pImageInfo = &m_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetDescImageInfo();
          m_SSRDescriptorSet.WriteDescriptorSets[5].pBufferInfo = &m_RendererData.VSBuffer.GetDescriptor();
          m_SSRDescriptorSet.WriteDescriptorSets[6].pBufferInfo = &m_RendererData.SSRBuffer.GetDescriptor();
          m_SSRDescriptorSet.Update();
          m_FrameBuffers.PostProcessPassFB.GetDescription().OnResize();
        });
    }
    {
      VulkanImageDescription atmImage;
      atmImage.Height = 128;
      atmImage.Width = 128;
      atmImage.CreateDescriptorSet = true;
      atmImage.Type = ImageType::TYPE_CUBE;
      atmImage.UsageFlags = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
      atmImage.Format = VulkanRenderer::SwapChain.m_ImageFormat;
      atmImage.FinalImageLayout = vk::ImageLayout::eGeneral;
      atmImage.TransitionLayoutAtCreate = true;
      m_FrameBuffers.AtmosphereImage.Create(atmImage);

      ImagePool::AddToPool(
        &m_FrameBuffers.AtmosphereImage,
        nullptr,
        [this] {
          m_AtmosphereDescriptorSet.WriteDescriptorSets[0].pImageInfo = &m_FrameBuffers.AtmosphereImage.GetDescImageInfo();
          m_AtmosphereDescriptorSet.Update();
          m_FrameBuffers.PostProcessPassFB.GetDescription().OnResize();
        });
    }
    {
      VulkanImageDescription dofImage;
      dofImage.Height = Window::GetHeight();
      dofImage.Width = Window::GetWidth();
      dofImage.CreateDescriptorSet = true;
      dofImage.UsageFlags = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
      dofImage.Format = VulkanRenderer::SwapChain.m_ImageFormat;
      dofImage.FinalImageLayout = vk::ImageLayout::eGeneral;
      dofImage.TransitionLayoutAtCreate = true;
      dofImage.SamplerAddressMode = vk::SamplerAddressMode::eClampToBorder;
      dofImage.SamplerBorderColor = vk::BorderColor::eFloatTransparentBlack;
      m_FrameBuffers.DepthOfFieldImage.Create(dofImage);

      ImagePool::AddToPool(
        &m_FrameBuffers.DepthOfFieldImage,
        &Window::GetWindowExtent(),
        [this] {
          m_DepthOfFieldDescriptorSet.WriteDescriptorSets[0].pImageInfo = &m_FrameBuffers.DepthOfFieldImage.GetDescImageInfo();
          m_DepthOfFieldDescriptorSet.WriteDescriptorSets[1].pImageInfo = &m_FrameBuffers.PBRPassFB.GetImage()[0].GetDescImageInfo();
          //m_DepthOfFieldDescriptorSet.WriteDescriptorSets[2].pImageInfo = &m_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetDescImageInfo();
          m_DepthOfFieldDescriptorSet.Update();
          m_FrameBuffers.PostProcessPassFB.GetDescription().OnResize();
        });
    }
    {
      const auto lodCount = std::max((int32_t)VulkanImage::GetMaxMipmapLevel(Window::GetWidth(), Window::GetHeight(), 1), 2);
      VulkanImageDescription bloompassimage;
      bloompassimage.Width = Window::GetWidth();
      bloompassimage.Height = Window::GetHeight();
      bloompassimage.CreateDescriptorSet = true;
      bloompassimage.UsageFlags = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
      bloompassimage.Format = VulkanRenderer::SwapChain.m_ImageFormat;
      bloompassimage.FinalImageLayout = vk::ImageLayout::eGeneral;
      bloompassimage.TransitionLayoutAtCreate = true;
      bloompassimage.SamplerAddressMode = vk::SamplerAddressMode::eClampToEdge;
      bloompassimage.SamplerBorderColor = vk::BorderColor::eFloatTransparentBlack;
      bloompassimage.MinFiltering = vk::Filter::eLinear;
      bloompassimage.MagFiltering = vk::Filter::eLinear;
      bloompassimage.MipLevels = lodCount;
      m_FrameBuffers.BloomDownsampleImage.Create(bloompassimage);
      bloompassimage.MipLevels = lodCount - 1;
      m_FrameBuffers.BloomUpsampleImage.Create(bloompassimage);
      auto updateBloomSet = [this] {
        const std::vector samplerImageInfos = {
          m_FrameBuffers.PBRPassFB.GetImage()[0].GetDescImageInfo(),
          m_FrameBuffers.BloomDownsampleImage.GetDescImageInfo(),
          m_FrameBuffers.BloomUpsampleImage.GetDescImageInfo()
        };
        m_BloomDescriptorSet.WriteDescriptorSets[0].pImageInfo = samplerImageInfos.data();
        const auto& downsamplerViews = m_FrameBuffers.BloomDownsampleImage.GetMipDescriptors();
        m_BloomDescriptorSet.WriteDescriptorSets[1].pImageInfo = downsamplerViews.data();
        const auto& upsamplerViews = m_FrameBuffers.BloomUpsampleImage.GetMipDescriptors();
        m_BloomDescriptorSet.WriteDescriptorSets[2].pImageInfo = upsamplerViews.data();
        m_BloomDescriptorSet.Update();

        m_FrameBuffers.PostProcessPassFB.GetDescription().OnResize();
      };
      ImagePool::AddToPool(
        &m_FrameBuffers.BloomUpsampleImage,
        &Window::GetWindowExtent(),
        [updateBloomSet] {
          updateBloomSet();
        },
        2);
      ImagePool::AddToPool(
        &m_FrameBuffers.BloomDownsampleImage,
        &Window::GetWindowExtent(),
        [updateBloomSet] {
          updateBloomSet();
        },
        2);
    }
    {
      VulkanImageDescription composite;
      composite.Height = Window::GetHeight();
      composite.Width = Window::GetWidth();
      composite.CreateDescriptorSet = true;
      composite.UsageFlags = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
      composite.Format = VulkanRenderer::SwapChain.m_ImageFormat;
      composite.FinalImageLayout = vk::ImageLayout::eGeneral;
      composite.TransitionLayoutAtCreate = true;
      composite.SamplerAddressMode = vk::SamplerAddressMode::eClampToBorder;
      composite.SamplerBorderColor = vk::BorderColor::eFloatTransparentBlack;
      m_FrameBuffers.CompositePassImage.Create(composite);

      ImagePool::AddToPool(
        &m_FrameBuffers.CompositePassImage,
        &Window::GetWindowExtent(),
        [this] {
          m_CompositeDescriptorSet.WriteDescriptorSets[0].pImageInfo = &m_FrameBuffers.CompositePassImage.GetDescImageInfo();
          m_CompositeDescriptorSet.WriteDescriptorSets[1].pImageInfo = &m_FrameBuffers.DepthOfFieldImage.GetDescImageInfo();
          m_CompositeDescriptorSet.WriteDescriptorSets[2].pImageInfo = &m_FrameBuffers.SSAOBlurPassImage.GetDescImageInfo();
          m_CompositeDescriptorSet.WriteDescriptorSets[3].pImageInfo = &m_FrameBuffers.BloomUpsampleImage.GetDescImageInfo();
          m_CompositeDescriptorSet.WriteDescriptorSets[4].pImageInfo = &m_FrameBuffers.SSRPassImage.GetDescImageInfo();
          m_CompositeDescriptorSet.WriteDescriptorSets[5].pImageInfo = &m_FrameBuffers.PostProcessPassFB.GetImage()[0].GetDescImageInfo();
          m_CompositeDescriptorSet.Update();
          m_FrameBuffers.PostProcessPassFB.GetDescription().OnResize();
        });
    }
    {
      FramebufferDescription postProcess;
      postProcess.DebugName = "Post Process Pass";
      postProcess.Width = Window::GetWidth();
      postProcess.Height = Window::GetHeight();
      postProcess.Extent = &Window::GetWindowExtent();
      postProcess.RenderPass = m_Pipelines.PostProcessPipeline.GetRenderPass().Get();
      colorImageDesc.Format = VulkanRenderer::SwapChain.m_ImageFormat;
      postProcess.ImageDescription = {colorImageDesc};
      postProcess.OnResize = [this] {
        m_PostProcessDescriptorSet.WriteDescriptorSets[0].pImageInfo = &m_FrameBuffers.CompositePassImage.GetDescImageInfo();
        m_PostProcessDescriptorSet.Update();
      };
      m_FrameBuffers.PostProcessPassFB.CreateFramebuffer(postProcess);
    }
  }

  void DefaultRenderPipeline::UpdateUniformBuffers() {
    OX_SCOPED_ZONE;
    m_RendererData.UBO_VS.projection = m_RendererContext.CurrentCamera->GetProjectionMatrixFlipped();
    m_RendererData.SkyboxBuffer.Copy(&m_RendererData.UBO_VS, sizeof m_RendererData.UBO_VS);

    m_RendererData.UBO_VS.projection = m_RendererContext.CurrentCamera->GetProjectionMatrixFlipped();
    m_RendererData.UBO_VS.view = m_RendererContext.CurrentCamera->GetViewMatrix();
    m_RendererData.UBO_VS.camPos = m_RendererContext.CurrentCamera->GetPosition();
    m_RendererData.VSBuffer.Copy(&m_RendererData.UBO_VS, sizeof m_RendererData.UBO_VS);

    m_RendererData.UBO_PbrPassParams.numLights = 1;
    m_RendererData.UBO_PbrPassParams.numThreads = (glm::ivec2(Window::GetWidth(), Window::GetHeight()) + PIXELS_PER_TILE - 1) /
                                                  PIXELS_PER_TILE;
    m_RendererData.UBO_PbrPassParams.numThreadGroups = (m_RendererData.UBO_PbrPassParams.numThreads + TILES_PER_THREADGROUP - 1) /
                                                       TILES_PER_THREADGROUP;
    m_RendererData.UBO_PbrPassParams.screenDimensions = glm::ivec2(Window::GetWidth(), Window::GetHeight());

    m_RendererData.ParametersBuffer.Copy(&m_RendererData.UBO_PbrPassParams, sizeof m_RendererData.UBO_PbrPassParams);

    m_RendererData.UBO_Atmosphere.LightPos = Vec4{
      Vec3(0.0f,
        glm::sin(glm::radians(m_RendererData.UBO_Atmosphere.Time * 360.0f)),
        glm::cos(glm::radians(m_RendererData.UBO_Atmosphere.Time * 360.0f))) * 149600000e3f,
      m_RendererData.UBO_Atmosphere.LightPos.w
    };
    m_RendererData.UBO_Atmosphere.InvProjection = glm::inverse(m_RendererContext.CurrentCamera->GetProjectionMatrixFlipped());
    m_RendererData.AtmosphereBuffer.Copy(&m_RendererData.UBO_Atmosphere, sizeof m_RendererData.UBO_Atmosphere);
  }

  void DefaultRenderPipeline::UpdateProbes() {
    // Post Process
    {
      OX_SCOPED_ZONE_N("PostProcess Probe System");
      const auto view = m_Scene->m_Registry.view<PostProcessProbe>();
      if (!view.empty()) {
        //TODO: Check if the camera is inside this probe.
        for (const auto&& [entity, component] : view.each()) {
          auto& ubo = m_RendererData.UBO_PostProcessParams;
          ubo.FilmGrain = {component.FilmGrainEnabled, component.FilmGrainIntensity};
          ubo.ChromaticAberration = {component.ChromaticAberrationEnabled, component.ChromaticAberrationIntensity};
          ubo.FilmGrain = {component.FilmGrainEnabled, component.FilmGrainIntensity};
          ubo.VignetteOffset.w = component.VignetteEnabled;
          ubo.VignetteColor.a = component.VignetteIntensity;
        }
      }
    }
  }
}
