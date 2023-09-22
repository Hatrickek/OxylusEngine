#include "DefaultRenderPipeline.h"

#include <vuk/Partials.hpp>

#include "DebugRenderer.h"
#include "Window.h"
#include "Assets/AssetManager.h"
#include "Core/Entity.h"
#include "Core/Resources.h"
#include "PBR/DirectShadowPass.h"
#include "PBR/Prefilter.h"
#include "Utils/FileUtils.h"
#include "Utils/Profiler.h"
#include "Vulkan/VukUtils.h"
#include "Vulkan/VulkanContext.h"

#include "Vulkan/VulkanRenderer.h"

namespace Oxylus {
void DefaultRenderPipeline::Init(Scene* scene) {
  m_Scene = scene;
  // Lights data
  m_PointLightsData.reserve(MAX_NUM_LIGHTS);

  // Mesh data
  m_MeshDrawList.reserve(MAX_NUM_MESHES);
  m_TransparentMeshDrawList.reserve(MAX_NUM_MESHES);

  m_SkyboxCube = CreateRef<Mesh>();
  m_SkyboxCube = AssetManager::GetMeshAsset(Resources::GetResourcesPath("Objects/cube.glb"));

  m_Resources.CubeMap = CreateRef<TextureAsset>();
  m_Resources.CubeMap = AssetManager::GetTextureAsset(Resources::GetResourcesPath("HDRs/sunflowers_puresky_4k.hdr"));

  GeneratePrefilter();

  InitRenderGraph();
}

void DefaultRenderPipeline::Update(Scene* scene) {
  // TODO:(hatrickek) Temporary solution for camera.
  m_RendererContext.CurrentCamera = VulkanRenderer::s_RendererContext.m_CurrentCamera;

  if (!m_RendererContext.CurrentCamera)
    OX_CORE_FATAL("No camera is set for rendering!");

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
      if (tag.Enabled && parentEnabled && !material.Materials.empty())
        m_MeshDrawList.emplace_back(meshrenderer.MeshGeometry.get(), e.GetWorldTransform(), material.Materials, meshrenderer.SubmesIndex);
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
  }
}

void DefaultRenderPipeline::OnDispatcherEvents(EventDispatcher& dispatcher) {
  dispatcher.sink<SceneRenderer::SkyboxLoadEvent>().connect<&DefaultRenderPipeline::UpdateSkybox>(*this);
  dispatcher.sink<SceneRenderer::ProbeChangeEvent>().connect<&DefaultRenderPipeline::UpdateParameters>(*this);
}

void DefaultRenderPipeline::Shutdown() { }

void DefaultRenderPipeline::InitRenderGraph() {
  const auto vkContext = VulkanContext::Get();

  auto [parBuffer, parBufferFut] = create_buffer(*vkContext->superframe_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&m_RendererData.m_FinalPassData, 1));
  m_ParametersBuffer = std::move(parBuffer);
  EnqueueFuture(std::move(parBufferFut));

  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(*FileUtils::ReadShaderFile("DepthNormalPass.vert"), FileUtils::GetShaderPath("DepthNormalPass.vert"));
    pci.add_glsl(*FileUtils::ReadShaderFile("DepthNormalPass.frag"), FileUtils::GetShaderPath("DepthNormalPass.frag"));
    vkContext->context->create_named_pipeline("DepthPrePassPipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(*FileUtils::ReadShaderFile("DirectShadowDepthPass.vert"), "DirectShadowDepthPass.vert");
    pci.add_glsl(*FileUtils::ReadShaderFile("DirectShadowDepthPass.frag"), "DirectShadowDepthPass.frag");
    vkContext->context->create_named_pipeline("ShadowPipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(*FileUtils::ReadShaderFile("Skybox.vert"), "Skybox.vert");
    pci.add_glsl(*FileUtils::ReadShaderFile("Skybox.frag"), "Skybox.frag");
    vkContext->context->create_named_pipeline("SkyboxPipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(*FileUtils::ReadShaderFile("PBRTiled.vert"), FileUtils::GetShaderPath("PBRTiled.vert"));
    pci.add_glsl(*FileUtils::ReadShaderFile("PBRTiled.frag"), FileUtils::GetShaderPath("PBRTiled.frag"));
    vkContext->context->create_named_pipeline("PBRPipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(*FileUtils::ReadShaderFile("Unlit.vert"), "Unlit.vert");
    pci.add_glsl(*FileUtils::ReadShaderFile("Unlit.frag"), "Unlit.frag");
    vkContext->context->create_named_pipeline("DebugPipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(*FileUtils::ReadShaderFile("SSR.comp"), "SSR.comp");
    vkContext->context->create_named_pipeline("SSRPipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(*FileUtils::ReadShaderFile("FinalPass.vert"), FileUtils::GetShaderPath("FinalPass.vert"));
    pci.add_glsl(*FileUtils::ReadShaderFile("FinalPass.frag"), FileUtils::GetShaderPath("FinalPass.frag"));
    vkContext->context->create_named_pipeline("FinalPipeline", pci);
  }

  WaitForFutures(vkContext);

#if 0
  /*const auto cacaoPass = m_RenderGraph->AddRenderPass<CACAOPass>("CACAOPass");
  ctx.Output = &cacaoPass->AddStorageTextureOutput("CACAO-Output", info);
  ctx.Depth = &cacaoPass->AddTextureInput("Depth-Output");
  ctx.Normal = &cacaoPass->AddTextureInput("Normal-Output");*/

  const auto ssrPass = m_RenderGraph->AddRenderPass<SSRPass>("SSRPass");
  ssrPass->AddResource("SSR-Image"_image >> vuk::eComputeRW >> "SSR-Output");
  ssrPass->AddResource("PBR-Output"_image >> vuk::eFragmentSampled);
  ssrPass->AddResource("Depth-Output"_image >> vuk::eFragmentSampled);
  ssrPass->AddResource("Normal-Ouput"_image >> vuk::eFragmentSampled);

  /*const auto bloomPass = m_RenderGraph->AddRenderPass<BloomPass>("BloomPass", vuk::DomainFlagBits::eComputeOnGraphics);
  bloomPass->AddResource("Bloom-Output"_image >> vuk::eColorRW);
  bloomPass->SetInput(0,
    RenderPassInput({
      geometryPass->m_PBRPassFB->GetImage()[0],
      bloomPass->m_BloomDownsampleImage,
      bloomPass->m_BloomUpsampleImage
    })
  );
  bloomPass->SetInput(1, {{bloomPass->m_BloomDownsampleImage}});
  bloomPass->SetInput(2, {{bloomPass->m_BloomUpsampleImage}});
  bloomPass->SetRunCondition(&RendererConfig::Get()->BloomConfig.Enabled);*/

  /*const auto debugNDTPass = m_RenderGraph->AddRenderPass<DebugNDTPass>("DebugNDTPass");
  debugNDTPass->AddResource("PBR-Output"_image >> vuk::eColorRW >> "PBR-Output+");*/

  const auto compositePass = m_RenderGraph->AddRenderPass<CompositePass>("CompositePass");
  compositePass->AddResource("Composite-Image"_image >> vuk::eComputeRW >> "Composite-Output");
  compositePass->AddResource("PBR-Output"_image >> vuk::eFragmentSampled);
  compositePass->AddResource("SSAO-Output"_image >> vuk::eFragmentSampled);
  compositePass->AddResource("SSR-Output"_image >> vuk::eFragmentSampled);

  const auto ppPass = m_RenderGraph->AddRenderPass<PPPass>("PPPass");
  ppPass->AddResource("PP-Image"_image >> vuk::eColorRW >> "PP-Output");
  ppPass->AddResource("Composite-Output"_image >> vuk::eFragmentSampled);

#endif
}

Scope<vuk::Future> DefaultRenderPipeline::OnRender(vuk::Allocator& frameAllocator, const vuk::Future& target) {
  Update(m_Scene);

  const auto rg = CreateRef<vuk::RenderGraph>("DefaultRenderPipelineRenderGraph");

  m_RendererData.m_UBO_VS.View = m_RendererContext.CurrentCamera->GetViewMatrix();
  m_RendererData.m_UBO_VS.CamPos = m_RendererContext.CurrentCamera->GetPosition();
  m_RendererData.m_UBO_VS.Projection = m_RendererContext.CurrentCamera->GetProjectionMatrixFlipped();
  auto [vsBuff, vsBufferFut] = create_buffer(frameAllocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&m_RendererData.m_UBO_VS, 1));
  auto& vsBuffer = *vsBuff;

  rg->attach_in("Final_Image", target);

  std::vector<Material::Parameters> materials = {};

  for (auto& mesh : m_MeshDrawList) {
    for (auto& mat : mesh.Materials)
      materials.emplace_back(mat->m_Parameters);
  }

  auto [matBuff, matBufferFut] = create_buffer(frameAllocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(materials));
  auto& matBuffer = *matBuff;

  rg->add_pass({
    .name = "DepthPrePass",
    .resources = {"Normal_Image"_image >> vuk::eColorRW >> "Normal_Output", "Depth_Image"_image >> vuk::eDepthStencilRW >> "Depth_Output"},
    .execute = [this, vsBuffer, matBuffer](vuk::CommandBuffer& commandBuffer) {
      commandBuffer.set_viewport(0, vuk::Rect2D::framebuffer())
                   .set_scissor(0, vuk::Rect2D::framebuffer())
                   .broadcast_color_blend(vuk::BlendPreset::eOff)
                   .set_rasterization({.cullMode = vuk::CullModeFlagBits::eBack})
                   .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
                      .depthTestEnable = true,
                      .depthWriteEnable = true,
                      .depthCompareOp = vuk::CompareOp::eLessOrEqual,
                    })
                   .bind_graphics_pipeline("DepthPrePassPipeline")
                   .bind_buffer(0, 0, vsBuffer)
                   .bind_buffer(2, 0, matBuffer);

      for (const auto& mesh : m_MeshDrawList) {
        VulkanRenderer::RenderMesh(mesh,
          commandBuffer,
          [&mesh, &commandBuffer](const Mesh::Primitive* part) {
            const auto& material = mesh.Materials[part->materialIndex];

            commandBuffer.push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, mesh.Transform)
                         .push_constants(vuk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), part->materialIndex)
                         .bind_sampler(1, 0, vuk::linear_sampler)
                         .bind_image(1, 0, *material->NormalTexture->GetTexture().view)
                         .bind_sampler(1, 1, {})
                         .bind_image(1, 1, *material->MetallicRoughnessTexture->GetTexture().view);
            return true;
          });
      }
    }
  });

  rg->attach_and_clear_image("Normal_Image", {.format = vuk::Format::eR16G16B16A16Sfloat, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->attach_and_clear_image("Depth_Image", {.format = vuk::Format::eD32Sfloat, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::DepthOne);
  rg->inference_rule("Normal_Image", vuk::same_shape_as("Final_Image"));
  rg->inference_rule("Depth_Image", vuk::same_shape_as("Normal_Image"));

  DirectShadowPass::DirectShadowUB directShadowUb = {};

  auto [buffer, bufferFut] = create_buffer(frameAllocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&directShadowUb, 1));
  auto& shadowBuffer = *buffer;

  rg->add_pass({
    .name = "DirectShadowPass",
    .resources = {"ShadowArray"_image >> vuk::eDepthStencilRW >> "ShadowArray_Output"},
    .execute = [this, shadowBuffer, &directShadowUb](vuk::CommandBuffer& commandBuffer) {
      const auto shadowSize = (float)RendererConfig::Get()->DirectShadowsConfig.Size;
      commandBuffer.bind_graphics_pipeline("ShadowPipeline")
                   .broadcast_color_blend({})
                   .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                   .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
                      .depthTestEnable = true,
                      .depthWriteEnable = true,
                      .depthCompareOp = vuk::CompareOp::eLessOrEqual,
                    })
                   .set_viewport(0, vuk::Viewport{0, 0, shadowSize, shadowSize, 0, 1})
                   .set_scissor(0, vuk::Rect2D{vuk::Sizing::eAbsolute, {}, {(unsigned)shadowSize, (unsigned)shadowSize}})
                   .bind_buffer(0, 0, shadowBuffer);

      for (const auto& e : m_SceneLights) {
        const auto& lightComponent = e.GetComponent<LightComponent>();
        if (lightComponent.Type != LightComponent::LightType::Directional)
          continue;

        DirectShadowPass::UpdateCascades(e, m_RendererContext.CurrentCamera, directShadowUb);

        for (const auto& mesh : m_MeshDrawList) {
          struct PushConst {
            glm::mat4 modelMatrix{};
            uint32_t cascadeIndex = 0;
          } pushConst;
          pushConst.modelMatrix = mesh.Transform;
          pushConst.cascadeIndex = 0; //todo

          commandBuffer.push_constants(vuk::ShaderStageFlagBits::eVertex, 0, &pushConst);

          VulkanRenderer::RenderMesh(mesh, commandBuffer, [&](const Mesh::Primitive*) { return true; });
        }
      }
    }
  });

  auto shadowSize = RendererConfig::Get()->DirectShadowsConfig.Size;
  vuk::ImageAttachment shadowArrayAttachment = {
    .extent = {.extent = {shadowSize, shadowSize, 1}}, .format = vuk::Format::eD32Sfloat, .sample_count = vuk::SampleCountFlagBits::e1, .view_type = vuk::ImageViewType::e2DArray, .level_count = 4
  };
  rg->attach_and_clear_image("ShadowArray", shadowArrayAttachment, vuk::DepthOne);

  auto [pointLightsBuf, pointLightsBufferFut] = create_buffer(frameAllocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(m_PointLightsData.data(), 1));
  auto pointLightsBuffer = *pointLightsBuf;

  struct PBRPassParams {
    int numLights = 0;
    IVec2 numThreads = {};
    IVec2 screenDimensions = {};
    IVec2 numThreadGroups = {};
  } pbrPassParams;

  pbrPassParams.numLights = 1;
  pbrPassParams.numThreads = (glm::ivec2(Window::GetWidth(), Window::GetHeight()) + PIXELS_PER_TILE - 1) / PIXELS_PER_TILE;
  pbrPassParams.numThreadGroups = (pbrPassParams.numThreads + TILES_PER_THREADGROUP - 1) / TILES_PER_THREADGROUP;
  pbrPassParams.screenDimensions = glm::ivec2(Window::GetWidth(), Window::GetHeight());

  auto [pbrBuf, pbrBufferFut] = create_buffer(frameAllocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&pbrPassParams, 1));
  auto pbrBuffer = *pbrBuf;

  rg->add_pass({
    .name = "GeomertyPass",
    .resources = {"PBR_Image"_image >> vuk::eColorRW >> "PBR_Output", "PBR_Depth"_image >> vuk::eDepthStencilRW, "ShadowArray_Output"_image >> vuk::eFragmentSampled},
    .execute = [this, vsBuffer, pointLightsBuffer, shadowBuffer, pbrBuffer, matBuffer](vuk::CommandBuffer& commandBuffer) {
      // Skybox pass
      struct SkyboxPushConstant {
        Mat4 View;
        float LodBias;
      } skyboxPushConstant = {};

      skyboxPushConstant.View = m_RendererContext.CurrentCamera->SkyboxView;

      const auto skyLightView = m_Scene->m_Registry.view<SkyLightComponent>();
      for (auto&& [e, sc] : skyLightView.each()) {
        skyboxPushConstant.LodBias = sc.LoadBias;
      }

      commandBuffer.bind_graphics_pipeline("SkyboxPipeline")
                   .set_viewport(0, vuk::Rect2D::framebuffer())
                   .set_scissor(0, vuk::Rect2D::framebuffer())
                   .broadcast_color_blend({})
                   .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                   .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
                      .depthTestEnable = false,
                      .depthWriteEnable = false,
                      .depthCompareOp = vuk::CompareOp::eLessOrEqual,
                    })
                   .bind_buffer(0, 0, vsBuffer)
                   .bind_sampler(0, 1, {})
                   .bind_image(0, 1, m_Resources.CubeMap->AsAttachment())
                   .push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, skyboxPushConstant);
      m_SkyboxCube->Draw(commandBuffer);

      auto irradianceAtt = vuk::ImageAttachment{
        .image = *m_IrradianceImage,
        .image_flags = vuk::ImageCreateFlagBits::eCubeCompatible,
        .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
        .extent = vuk::Dimension3D::absolute(64, 64, 1),
        .format = vuk::Format::eR32G32B32A32Sfloat,
        .sample_count = vuk::Samples::e1,
        .view_type = vuk::ImageViewType::eCube,
        .base_level = 0,
        .level_count = 1,
        .base_layer = 0,
        .layer_count = 6
      };

      auto prefilterAtt = vuk::ImageAttachment{
        .image = *m_PrefilteredImage,
        .image_flags = vuk::ImageCreateFlagBits::eCubeCompatible,
        .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
        .extent = vuk::Dimension3D::absolute(512, 512, 1),
        .format = vuk::Format::eR32G32B32A32Sfloat,
        .sample_count = vuk::Samples::e1,
        .view_type = vuk::ImageViewType::eCube,
        .base_level = 0,
        .level_count = 1,
        .base_layer = 0,
        .layer_count = 6
      };

      auto brdfAtt = vuk::ImageAttachment{
        .image = *m_BRDFImage,
        .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
        .extent = vuk::Dimension3D::absolute(512, 512, 1),
        .format = vuk::Format::eR32G32B32A32Sfloat,
        .sample_count = vuk::Samples::e1,
        .view_type = vuk::ImageViewType::e2D,
        .base_level = 0,
        .level_count = 1,
        .base_layer = 0,
        .layer_count = 1
      };

      commandBuffer.bind_graphics_pipeline("PBRPipeline")
                   .set_viewport(0, vuk::Rect2D::framebuffer())
                   .set_scissor(0, vuk::Rect2D::framebuffer())
                   .broadcast_color_blend({})
                   .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
                      .depthTestEnable = true,
                      .depthWriteEnable = true,
                      .depthCompareOp = vuk::CompareOp::eLessOrEqual,
                    })
                   .bind_buffer(0, 0, vsBuffer)
                   .bind_buffer(0, 1, pbrBuffer)
                   .bind_buffer(0, 2, pointLightsBuffer)
                   .bind_sampler(0, 4, {})
                   .bind_image(0, 4, irradianceAtt)
                   .bind_sampler(0, 5, {})
                   .bind_image(0, 5, brdfAtt)
                   .bind_sampler(0, 6, {})
                   .bind_image(0, 6, prefilterAtt)
                   .bind_sampler(0, 7, {})
                   .bind_image(0, 7, "ShadowArray_Output")
                   .bind_buffer(0, 8, shadowBuffer)
                   .bind_buffer(2, 0, matBuffer);

      // PBR pipeline
      for (const auto& mesh : m_MeshDrawList) {
        VulkanRenderer::RenderMesh(mesh,
          commandBuffer,
          [&](const Mesh::Primitive* part) {
            const auto& material = mesh.Materials[part->materialIndex];
            if (material->AlphaMode == Material::AlphaMode::Blend) {
              m_TransparentMeshDrawList.emplace_back(mesh);
              return false;
            }

            commandBuffer.push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, mesh.Transform)
                         .push_constants(vuk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), part->materialIndex);

            material->BindTextures(commandBuffer);
            return true;
          });
      }

      m_MeshDrawList.clear();

      commandBuffer.broadcast_color_blend(vuk::BlendPreset::eAlphaBlend);

      // Transparency pass
      for (const auto& mesh : m_TransparentMeshDrawList) {
        VulkanRenderer::RenderMesh(mesh,
          commandBuffer,
          [&](const Mesh::Primitive* part) {
            const auto& material = mesh.Materials[part->materialIndex];
            commandBuffer.push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, &mesh.Transform)
                         .push_constants(vuk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), part->materialIndex);

            material->BindTextures(commandBuffer);
            return true;
          });
      }
      m_TransparentMeshDrawList.clear();

      // Depth-tested debug renderer pass
#if 0
      auto& shapes = DebugRenderer::GetInstance()->GetShapes();

      struct DebugPassData {
        Mat4 ViewProjection = {};
        Mat4 Model = {};
        Vec4 Color = {};
      } pushConst;

      commandBuffer.bind_graphics_pipeline("DebugPipeline")
                   .set_viewport(0, vuk::Rect2D::framebuffer())
                   .set_scissor(0, vuk::Rect2D::framebuffer());

      pushConst.ViewProjection = m_RendererContext.CurrentCamera->GetProjectionMatrixFlipped() * m_RendererContext.CurrentCamera->GetViewMatrix();
      for (auto& shape : shapes) {
        pushConst.Model = shape.ModelMatrix;
        pushConst.Color = shape.Color;
        commandBuffer.push_constants(vuk::ShaderStageFlagBits::eVertex, 0, &pushConst);
        shape.ShapeMesh->Draw(commandBuffer);
      }

      DebugRenderer::Reset(false);
#endif
    }
  });

  rg->attach_and_clear_image("PBR_Image", {.format = VulkanContext::Get()->swapchain->format, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->attach_and_clear_image("PBR_Depth", {.format = vuk::Format::eD32Sfloat, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::DepthOne);
  rg->inference_rule("PBR_Image", vuk::same_shape_as("Final_Image"));
  rg->inference_rule("PBR_Depth", vuk::same_shape_as("Final_Image"));

  struct SSRData {
    int Samples = 30;
    int BinarySearchSamples = 8;
    float MaxDist = 50.0f;
  } ssrData;

  auto [ssrBuff, ssrBuffFut] = create_buffer(frameAllocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&ssrData, 1));
  auto ssrBuffer = *ssrBuff;

  rg->add_pass({
    .name = "SSRPass",
    .resources = {
      "SSR_Image"_image >> vuk::eComputeRW >> "SSR_Output",
      "PBR_Output"_image >> vuk::eFragmentSampled,
      "Depth_Output"_image >> vuk::eFragmentSampled,
      "Normal_Output"_image >> vuk::eFragmentSampled
    },
    .execute = [this, ssrBuffer, vsBuffer](vuk::CommandBuffer& commandBuffer) {
      commandBuffer.bind_compute_pipeline("SSRPipeline")
                   .bind_image(0, 0, "SSR_Image")
                   .bind_sampler(0, 0, {})
                   .bind_image(0, 1, "PBR_Output")
                   .bind_image(0, 2, "Depth_Output")
                   .bind_image(0, 3, m_Resources.CubeMap->AsAttachment())
                   .bind_image(0, 4, "Normal_Output")
                   .bind_buffer(0, 5, vsBuffer)
                   .bind_buffer(0, 6, ssrBuffer)
                   .dispatch((Window::GetWidth() + 8 - 1) / 8, (Window::GetHeight() + 8 - 1) / 8, 1);
    }
  });

  rg->attach_and_clear_image("SSR_Image", {.sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->inference_rule("SSR_Image", vuk::same_shape_as("Final_Image"));
  rg->inference_rule("SSR_Image", vuk::same_format_as("Final_Image"));

  auto [finalBuff, finalBuffFut] = create_buffer(frameAllocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&m_RendererData.m_FinalPassData, 1));
  auto finalBuffer = *finalBuff;

  rg->add_pass({
    .name = "FinalPass",
    .resources = {
      "Final_Image"_image >> vuk::eColorRW >> "Final_Output",
      "PBR_Output"_image >> vuk::eFragmentSampled,
      "SSR_Output"_image >> vuk::eFragmentSampled,
    },
    .execute = [finalBuffer](vuk::CommandBuffer& commandBuffer) {
      commandBuffer.bind_graphics_pipeline("FinalPipeline")
                   .bind_image(0, 0, "PBR_Output")
                   .bind_image(0, 3, "SSR_Output")
                   .bind_buffer(0, 4, finalBuffer)
                   .draw(3, 1, 0, 0);
    }
  });

  return CreateScope<vuk::Future>(vuk::Future{rg, vuk::Name("Final_Output")});
}


void DefaultRenderPipeline::UpdateSkybox(const SceneRenderer::SkyboxLoadEvent& e) {
  m_ForceUpdateMaterials = true;
  m_Resources.CubeMap = e.CubeMap;

  GeneratePrefilter();
}

void DefaultRenderPipeline::UpdateLightingData() {
  OX_SCOPED_ZONE;
  for (auto& e : m_SceneLights) {
    auto& lightComponent = e.GetComponent<LightComponent>();
    auto& transformComponent = e.GetComponent<TransformComponent>();
    switch (lightComponent.Type) {
      case LightComponent::LightType::Directional: break;
      case LightComponent::LightType::Point: {
        m_PointLightsData.emplace_back(VulkanRenderer::LightingData{
          Vec4{transformComponent.Translation, lightComponent.Intensity},
          Vec4{lightComponent.Color, lightComponent.Range}, Vec4{transformComponent.Rotation, 1.0f}
        });
      }
      break;
      case LightComponent::LightType::Spot: break;
    }
  }
}

void DefaultRenderPipeline::GeneratePrefilter() {
  OX_SCOPED_ZONE;
  vuk::Compiler compiler{};
  auto& allocator = *VulkanContext::Get()->superframe_allocator;

  auto [brdfImage, brdfFut] = Prefilter::GenerateBRDFLUT();
  brdfFut.wait(allocator, compiler);
  m_BRDFImage = std::move(brdfImage);

  auto [irradianceImage, irradianceFut] = Prefilter::GenerateIrradianceCube(m_SkyboxCube, m_Resources.CubeMap);
  irradianceFut.wait(allocator, compiler);
  m_IrradianceImage = std::move(irradianceImage);

  auto [prefilterImage, prefilterFut] = Prefilter::GeneratePrefilteredCube(m_SkyboxCube, m_Resources.CubeMap);
  prefilterFut.wait(allocator, compiler);
  m_PrefilteredImage = std::move(prefilterImage);
}

void DefaultRenderPipeline::UpdateParameters(SceneRenderer::ProbeChangeEvent& e) {
  auto& ubo = m_RendererData.m_FinalPassData;
  auto& component = e.Probe;
  ubo.FilmGrain = {component.FilmGrainEnabled, component.FilmGrainIntensity};
  ubo.ChromaticAberration = {component.ChromaticAberrationEnabled, component.ChromaticAberrationIntensity};
  ubo.FilmGrain = {component.FilmGrainEnabled, component.FilmGrainIntensity};
  ubo.VignetteOffset.w = component.VignetteEnabled;
  ubo.VignetteColor.a = component.VignetteIntensity;
}
}
