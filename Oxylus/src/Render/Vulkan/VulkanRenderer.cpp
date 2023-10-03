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
VulkanRenderer::RendererContext VulkanRenderer::renderer_context;
RendererConfig VulkanRenderer::renderer_config;

void VulkanRenderer::init() {
  renderer_context.default_render_pipeline = create_ref<DefaultRenderPipeline>("DefaultRenderPipeline");

  // Save/Load renderer config
  if (!RendererConfig::get()->load_config("renderer.oxconfig"))
    RendererConfig::get()->save_config("renderer.oxconfig");
  RendererConfig::get()->config_change_dispatcher.trigger(RendererConfig::ConfigChangeEvent{});

  TextureAsset::create_blank_texture();
  TextureAsset::create_white_texture();

  // Debug renderer
  DebugRenderer::init();

  renderer_config.config_change_dispatcher.trigger(RendererConfig::ConfigChangeEvent{});

#if GPU_PROFILER_ENABLED
    //Initalize tracy profiling
    const VkPhysicalDevice physicalDevice = VulkanContext::Context.PhysicalDevice;
    TracyProfiler::InitTracyForVulkan(physicalDevice,
      LogicalDevice,
      VulkanContext::VulkanQueue.GraphicsQueue,
      s_RendererContext.m_TimelineCommandBuffer.Get());
#endif
}

void VulkanRenderer::shutdown() {
  RendererConfig::get()->save_config("renderer.oxconfig");
  DebugRenderer::release();
#if GPU_PROFILER_ENABLED
    TracyProfiler::DestroyContext();
#endif
}

void VulkanRenderer::set_camera(Camera& camera) {
  renderer_context.current_camera = &camera;
}

void VulkanRenderer::draw(VulkanContext* context, ImGuiLayer* imgui_layer, LayerStack& layer_stack, const Ref<SystemManager>& system_manager) {
  OX_SCOPED_ZONE;
  imgui_layer->Begin();

  auto frameAllocator = context->begin();
  const Ref rg = create_ref<vuk::RenderGraph>("runner");
  rg->attach_swapchain("_swp", context->swapchain);
  rg->clear_image("_swp", "final_image", vuk::ClearColor{0.0f, 0.0f, 0.0f, 1.0f});

  const auto rp = renderer_context.render_pipeline;

  vuk::Future fut = {};

  // Render it directly to swapchain
  if (rp->is_swapchain_attached()) {
    for (const auto& layer : layer_stack)
      layer->on_imgui_render();
    system_manager->OnImGuiRender();
    imgui_layer->End();

    renderer_context.viewport_size.x = Window::get_width();
    renderer_context.viewport_size.y = Window::get_height();

    fut = *rp->on_render(frameAllocator, vuk::Future{rg, "final_image"});

    fut = imgui_layer->RenderDrawData(frameAllocator, fut, ImGui::GetDrawData());
  }
  // Render it into a separate image with given dimension
  else {
    const auto rgx = create_ref<vuk::RenderGraph>(rp->get_name().c_str());

    const auto& dim = rp->get_dimension();

    OX_CORE_ASSERT(dim.extent.width > 0)
    OX_CORE_ASSERT(dim.extent.height > 0)

    renderer_context.viewport_size.x = dim.extent.width;
    renderer_context.viewport_size.y = dim.extent.height;

    rgx->attach_and_clear_image("_img",
      vuk::ImageAttachment{
        .extent = dim,
        .format = context->swapchain->format,
        .sample_count = vuk::Samples::e1,
        .level_count = 1,
        .layer_count = 1
      },
      vuk::ClearColor(0.0f, 0.0f, 0.0f, 1.f)
    );

    auto rpFut = *rp->on_render(frameAllocator, vuk::Future{rgx, "_img"});
    const auto attachmentNameOut = rpFut.get_bound_name().name;

    auto rpRg = rpFut.get_render_graph();

    vuk::Compiler compiler;
    compiler.compile({&rpRg, 1}, {});

    rg->attach_in(attachmentNameOut, std::move(rpFut));

    auto si = create_ref<vuk::SampledImage>(
      make_sampled_image(vuk::NameReference{rg.get(), vuk::QualifiedName({}, attachmentNameOut)}, {}));
    rp->set_final_image(si);

    for (const auto& layer : layer_stack)
      layer->on_imgui_render();

    system_manager->OnImGuiRender();

    imgui_layer->End();

    fut = imgui_layer->RenderDrawData(frameAllocator, vuk::Future{rg, "final_image"}, ImGui::GetDrawData());
  }

  context->end(fut, frameAllocator);
}

void VulkanRenderer::render_node(const Mesh::Node* node, vuk::CommandBuffer& command_buffer, const std::function<bool(Mesh::Primitive* prim)>& per_mesh_func) {
  for (const auto& part : node->primitives) {
    if (!per_mesh_func(part))
      continue;

    command_buffer.draw_indexed(part->index_count, 1, part->first_index, 0, 0);
  }
  for (const auto& child : node->children)
    render_node(child, command_buffer, per_mesh_func);
}

void VulkanRenderer::render_mesh(const MeshData& mesh,
                                 vuk::CommandBuffer& command_buffer,
                                 const std::function<bool(Mesh::Primitive* prim)>& per_mesh_func) {
  mesh.mesh_geometry->bind_vertex_buffer(command_buffer);
  mesh.mesh_geometry->bind_index_buffer(command_buffer);
  render_node(mesh.mesh_geometry->linear_nodes[mesh.submesh_index], command_buffer, per_mesh_func);
}

std::pair<vuk::Unique<vuk::Image>, vuk::Future> VulkanRenderer::generate_cubemap_from_equirectangular(const vuk::Texture& cubemap) {
  auto& allocator = *VulkanContext::get()->superframe_allocator;

  vuk::Unique<vuk::Image> output;

  constexpr auto size = 2048;
  auto mip_count = static_cast<uint32_t>(log2f((float)std::max(size, size))) + 1;

  auto env_cubemap_ia = vuk::ImageAttachment{
    .image = *output,
    .image_flags = vuk::ImageCreateFlagBits::eCubeCompatible,
    .image_type = vuk::ImageType::e2D,
    .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment |
             vuk::ImageUsageFlagBits::eTransferSrc | vuk::ImageUsageFlagBits::eTransferDst,
    .extent = vuk::Dimension3D::absolute(size, size, 1),
    .format = vuk::Format::eR32G32B32A32Sfloat,
    .sample_count = vuk::Samples::e1,
    .view_type = vuk::ImageViewType::eCube,
    .base_level = 0,
    .level_count = mip_count,
    .base_layer = 0,
    .layer_count = 6
  };
  output = *allocate_image(allocator, env_cubemap_ia);
  env_cubemap_ia.image = *output;

  vuk::PipelineBaseCreateInfo equirectangular_to_cubemap;
  equirectangular_to_cubemap.add_glsl(*FileUtils::read_shader_file("Cubemap.vert"), "Cubemap.vert");
  equirectangular_to_cubemap.add_glsl(*FileUtils::read_shader_file("EquirectangularToCubemap.frag"), "EquirectangularToCubemap.frag");
  allocator.get_context().create_named_pipeline("equirectangular_to_cubemap", equirectangular_to_cubemap);

  auto cube = AssetManager::get_mesh_asset(Resources::get_resources_path("Objects/cube.glb"));

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

      cube->bind_vertex_buffer(cbuf);
      cube->bind_index_buffer(cbuf);
      for (const auto& node : cube->nodes)
        for (const auto& primitive : node->primitives)
          cbuf.draw_indexed(primitive->index_count, 6, 0, 0, 0);
    }
  });

  return {
    std::move(output), transition(vuk::Future{std::make_unique<vuk::RenderGraph>(std::move(rg)), "env_cubemap+"}, vuk::eFragmentSampled)
  };
}
}
