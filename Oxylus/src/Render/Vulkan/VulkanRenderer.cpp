#include "VulkanRenderer.h"

#include <future>
#include <vuk/Partials.hpp>

#include "VulkanContext.h"
#include "Assets/AssetManager.h"
#include "Core/Resources.h"
#include "Core/Systems/SystemManager.h"
#include "Render/Mesh.h"
#include "Render/Window.h"
#include "Utils/Profiler.h"
#include "Render/DebugRenderer.h"
#include "Render/DefaultRenderPipeline.h"
#include "Utils/FileUtils.h"

namespace Oxylus {
VulkanRenderer::RendererContext VulkanRenderer::s_RendererContext;
VulkanRenderer::RendererData VulkanRenderer::s_RendererData;
RendererConfig VulkanRenderer::s_RendererConfig;

void VulkanRenderer::Init() {
  s_RendererContext.m_DefaultRenderPipeline = CreateRef<DefaultRenderPipeline>("DefaultRenderPipeline");

  // Save/Load renderer config
  if (!RendererConfig::Get()->LoadConfig("renderer.oxconfig")) {
    RendererConfig::Get()->SaveConfig("renderer.oxconfig");
  }
  RendererConfig::Get()->ConfigChangeDispatcher.trigger(RendererConfig::ConfigChangeEvent{});

  TextureAsset::CreateBlankTexture();
  TextureAsset::CreateWhiteTexture();

  // Debug renderer
  DebugRenderer::Init();

  s_RendererConfig.ConfigChangeDispatcher.trigger(RendererConfig::ConfigChangeEvent{});

#if GPU_PROFILER_ENABLED
    //Initalize tracy profiling
    const VkPhysicalDevice physicalDevice = VulkanContext::Context.PhysicalDevice;
    TracyProfiler::InitTracyForVulkan(physicalDevice,
      LogicalDevice,
      VulkanContext::VulkanQueue.GraphicsQueue,
      s_RendererContext.m_TimelineCommandBuffer.Get());
#endif
}

void VulkanRenderer::Shutdown() {
  RendererConfig::Get()->SaveConfig("renderer.oxconfig");
  DebugRenderer::Release();
#if GPU_PROFILER_ENABLED
    TracyProfiler::DestroyContext();
#endif
}

void VulkanRenderer::SetCamera(Camera& camera) {
  s_RendererContext.m_CurrentCamera = &camera;
}

void VulkanRenderer::Draw(VulkanContext* context, ImGuiLayer* imguiLayer, LayerStack& layerStack, Ref<SystemManager>& systemManager) {
  OX_SCOPED_ZONE;
  imguiLayer->Begin();

  auto frameAllocator = context->Begin();
  const Ref rg = CreateRef<vuk::RenderGraph>("runner");
  rg->attach_swapchain("_swp", context->Swapchain);
  rg->clear_image("_swp", "final_image", vuk::ClearColor{0.0f, 0.0f, 0.0f, 1.0f});

  const auto rp = s_RendererContext.m_RenderPipeline;

  vuk::Future fut = {};

  // Render it directly to swapchain
  if (rp->IsSwapchainAttached()) {
    for (const auto& layer : layerStack)
      layer->OnImGuiRender();
    systemManager->OnImGuiRender();
    imguiLayer->End();

    fut = *rp->OnRender(frameAllocator, vuk::Future{rg, "final_image"});

    fut = imguiLayer->RenderDrawData(frameAllocator, fut, ImGui::GetDrawData());
  }
  // Render it into a separate image with given dimension
  else {
    const auto rgx = CreateRef<vuk::RenderGraph>(rp->GetName().c_str());

    const auto& dim = rp->GetDimension();

    OX_CORE_ASSERT(dim.extent.height > 0)
    OX_CORE_ASSERT(dim.extent.width > 0)

    rgx->attach_and_clear_image("_img",
      vuk::ImageAttachment{
        .extent = dim,
        .format = context->Swapchain->format,
        .sample_count = vuk::Samples::e1,
        .level_count = 1,
        .layer_count = 1
      },
      vuk::ClearColor(0.0f, 0.0f, 0.0f, 1.f)
    );

    auto rpFut = *rp->OnRender(frameAllocator, vuk::Future{rgx, "_img"});
    const auto attachmentNameOut = rpFut.get_bound_name().name;

    auto rpRg = rpFut.get_render_graph();

    vuk::Compiler compiler;
    compiler.compile({&rpRg, 1}, {});

    rg->attach_in(attachmentNameOut, std::move(rpFut));

    auto si = CreateRef<vuk::SampledImage>(make_sampled_image(vuk::NameReference{rg.get(), vuk::QualifiedName({}, attachmentNameOut)}, {}));
    rp->SetFinalImage(si);

    for (const auto& layer : layerStack)
      layer->OnImGuiRender();

    systemManager->OnImGuiRender();

    imguiLayer->End();

    fut = imguiLayer->RenderDrawData(frameAllocator, vuk::Future{rg, "final_image"}, ImGui::GetDrawData());
  }

  context->End(fut, frameAllocator);
}

void VulkanRenderer::RenderNode(const Mesh::Node* node, vuk::CommandBuffer& commandBuffer, const std::function<bool(Mesh::Primitive* prim)>& perMeshFunc) {
  for (const auto& part : node->Primitives) {
    if (!perMeshFunc(part))
      continue;

    commandBuffer.draw_indexed(part->indexCount, 1, part->firstIndex, 0, 0);
  }
  for (const auto& child : node->Children) {
    RenderNode(child, commandBuffer, perMeshFunc);
  }
}

void VulkanRenderer::RenderMesh(const MeshData& mesh, vuk::CommandBuffer& commandBuffer, const std::function<bool(Mesh::Primitive* prim)>& perMeshFunc) {
  mesh.MeshGeometry->BindVertexBuffer(commandBuffer);
  mesh.MeshGeometry->BindIndexBuffer(commandBuffer);
  RenderNode(mesh.MeshGeometry->LinearNodes[mesh.SubmeshIndex], commandBuffer, perMeshFunc);
}

std::pair<vuk::Unique<vuk::Image>, vuk::Future> VulkanRenderer::GenerateCubemapFromEquirectangular(const vuk::Texture& cubemap) {
  auto& allocator = *VulkanContext::Get()->SuperframeAllocator;

  vuk::Unique<vuk::Image> output;

  constexpr auto size = 2048;
  auto mipCount = (uint32_t)log2f((float)std::max(size, size)) + 1;

  auto env_cubemap_ia = vuk::ImageAttachment{
    .image = *output,
    .image_flags = vuk::ImageCreateFlagBits::eCubeCompatible,
    .image_type = vuk::ImageType::e2D,
    .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment | vuk::ImageUsageFlagBits::eTransferSrc | vuk::ImageUsageFlagBits::eTransferDst,
    .extent = vuk::Dimension3D::absolute(size, size, 1),
    .format = vuk::Format::eR32G32B32A32Sfloat,
    .sample_count = vuk::Samples::e1,
    .view_type = vuk::ImageViewType::eCube,
    .base_level = 0,
    .level_count = mipCount,
    .base_layer = 0,
    .layer_count = 6
  };
  output = *vuk::allocate_image(allocator, env_cubemap_ia);
  env_cubemap_ia.image = *output;

  vuk::PipelineBaseCreateInfo equirectangular_to_cubemap;
  equirectangular_to_cubemap.add_glsl(*FileUtils::ReadShaderFile("Cubemap.vert"), "Cubemap.vert");
  equirectangular_to_cubemap.add_glsl(*FileUtils::ReadShaderFile("EquirectangularToCubemap.frag"), "EquirectangularToCubemap.frag");
  allocator.get_context().create_named_pipeline("equirectangular_to_cubemap", equirectangular_to_cubemap);

  auto cube = AssetManager::GetMeshAsset(Resources::GetResourcesPath("Objects/cube.glb"));

  const auto capture_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
  const Mat4 capture_views[] = {
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f))
  };

  vuk::RenderGraph rg("cubegen");
  rg.attach_image("hdr_texture", vuk::ImageAttachment::from_texture(cubemap), vuk::Access::eFragmentSampled);
  rg.attach_image("env_cubemap", env_cubemap_ia, vuk::Access::eNone);
  rg.add_pass({
    .resources = {
      "env_cubemap"_image >> vuk::eColorWrite,
      "hdr_texture"_image >> vuk::eFragmentSampled
    },
    .execute = [=](vuk::CommandBuffer& cbuf) {
      cbuf.set_viewport(0, vuk::Rect2D::framebuffer())
          .set_scissor(0, vuk::Rect2D::framebuffer())
          .broadcast_color_blend(vuk::BlendPreset::eOff)
          .set_rasterization({})
          .bind_image(0, 2, "hdr_texture")
          .bind_sampler(0,
             2,
             vuk::SamplerCreateInfo{
               .magFilter = vuk::Filter::eLinear,
               .minFilter = vuk::Filter::eLinear,
               .mipmapMode = vuk::SamplerMipmapMode::eLinear,
               .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
               .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
               .addressModeW = vuk::SamplerAddressMode::eClampToEdge
             })
          .bind_graphics_pipeline("equirectangular_to_cubemap");

      auto* projection = cbuf.map_scratch_buffer<glm::mat4>(0, 0);
      *projection = capture_projection;
      const auto view = cbuf.map_scratch_buffer<glm::mat4[6]>(0, 1);
      memcpy(view, capture_views, sizeof(capture_views));

      cube->BindVertexBuffer(cbuf);
      cube->BindIndexBuffer(cbuf);
      for (const auto& node : cube->Nodes) {
        for (const auto& primitive : node->Primitives)
          cbuf.draw_indexed(primitive->indexCount, 6, 0, 0, 0);
      }
    }
  });

  return {std::move(output), transition(vuk::Future{std::make_unique<vuk::RenderGraph>(std::move(rg)), "env_cubemap+"}, vuk::eFragmentSampled)};
}
}
