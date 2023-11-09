#include "RendererCommon.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <vuk/CommandBuffer.hpp>
#include <vuk/Partials.hpp>
#include <vuk/Pipeline.hpp>
#include <vuk/RenderGraph.hpp>

#include "Mesh.h"
#include "RendererData.h"

#include "Assets/AssetManager.h"

#include "Core/Resources.h"
#include "Core/Types.h"

#include "Utils/FileUtils.h"
#include "Utils/Log.h"
#include "Utils/Profiler.h"

#include "Vulkan/VukUtils.h"
#include "Vulkan/VulkanContext.h"

namespace Oxylus {
std::pair<vuk::Unique<vuk::Image>, vuk::Future> RendererCommon::generate_cubemap_from_equirectangular(const vuk::Texture& cubemap) {
  auto& allocator = *VulkanContext::get()->superframe_allocator;

  // blur the hdr texture
  Ref<vuk::RenderGraph> rg = create_ref<vuk::RenderGraph>("cubegen");
  rg->attach_image("hdr_image", vuk::ImageAttachment::from_texture(cubemap));
  rg->attach_and_clear_image("blurred_hdr_image", vuk::ImageAttachment{.sample_count = vuk::SampleCountFlagBits::e1, .view_type = vuk::ImageViewType::e2D}, vuk::Black<float>);
  rg->inference_rule("blurred_hdr_image", vuk::same_extent_as("hdr_image"));
  rg->inference_rule("blurred_hdr_image", vuk::same_format_as("hdr_image"));

  apply_blur(rg, "hdr_image", "blurred_hdr_image", "blurred_hdr_image_output");

  vuk::Unique<vuk::Image> output;

  constexpr auto size = 2048;

  const auto mip_count = static_cast<uint32_t>(log2f((float)std::max(size, size))) + 1;

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

  if (!allocator.get_context().is_pipeline_available("equirectangular_to_cubemap")) {
    vuk::PipelineBaseCreateInfo equirectangular_to_cubemap;
    equirectangular_to_cubemap.add_glsl(FileUtils::read_shader_file("Cubemap.vert"), "Cubemap.vert");
    equirectangular_to_cubemap.add_glsl(FileUtils::read_shader_file("EquirectangularToCubemap.frag"), "EquirectangularToCubemap.frag");
    allocator.get_context().create_named_pipeline("equirectangular_to_cubemap", equirectangular_to_cubemap);
  }

  const auto cube = generate_cube();

  const auto capture_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
  const Mat4 capture_views[] = {
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f))
  };

  rg->attach_image("env_cubemap", env_cubemap_ia, vuk::Access::eNone);
  rg->add_pass({
    .resources = {
      "env_cubemap"_image >> vuk::eColorWrite,
      "blurred_hdr_image_output"_image >> vuk::eFragmentSampled
    },
    .execute = [=](vuk::CommandBuffer& cbuf) {
      cbuf.set_viewport(0, vuk::Rect2D::framebuffer())
          .set_scissor(0, vuk::Rect2D::framebuffer())
          .broadcast_color_blend(vuk::BlendPreset::eOff)
          .set_rasterization({})
          .bind_image(0, 2, "blurred_hdr_image_output")
          .bind_sampler(0, 2, vuk::LinearSamplerClamped)
          .bind_graphics_pipeline("equirectangular_to_cubemap");

      auto* projection = cbuf.map_scratch_buffer<glm::mat4>(0, 0);
      *projection = capture_projection;
      const auto view = cbuf.map_scratch_buffer<glm::mat4[6]>(0, 1);
      memcpy(view, capture_views, sizeof(capture_views));

      cube->bind_vertex_buffer(cbuf);
      cube->bind_index_buffer(cbuf);
      cbuf.draw_indexed(cube->indices.size(), 6, 0, 0, 0);
    }
  });

  return {
    std::move(output), transition(vuk::Future{std::move(rg), "env_cubemap+"}, vuk::eFragmentSampled)
  };
}

std::pair<vuk::Buffer, vuk::Buffer> RendererCommon::merge_render_objects(std::vector<MeshData>& draw_list) {
  OX_SCOPED_ZONE_N("Mesh Merge");
  size_t total_vertices = 0;
  size_t total_indices = 0;

  for (auto& m : draw_list) {
    m.first_index = (uint32_t)total_indices;
    m.first_vertex = (uint32_t)total_vertices;

    OX_CORE_ASSERT(m.vertex_count != 0)
    OX_CORE_ASSERT(m.index_count != 0)
    total_vertices += m.vertex_count;
    total_indices += m.index_count;

    m.is_merged = true;
  }

  const auto vk_context = VulkanContext::get();

  vuk::Buffer merged_vertex_buffer;
  vuk::BufferCreateInfo bci_vertex{vuk::MemoryUsage::eGPUonly, total_vertices * sizeof(Mesh::Vertex), 1};
  vk_context->superframe_allocator->allocate_buffers(std::span{&merged_vertex_buffer, 1}, std::span{&bci_vertex, 1});

  vuk::Buffer merged_index_buffer;
  vuk::BufferCreateInfo bci_index{vuk::MemoryUsage::eGPUonly, total_indices * sizeof(uint32_t), 1};
  vk_context->superframe_allocator->allocate_buffers(std::span{&merged_index_buffer, 1}, std::span{&bci_index, 1});

  std::vector<vuk::Future> futures;

  for (auto& m : draw_list) {
    Ref<vuk::RenderGraph> rgv = create_ref<vuk::RenderGraph>("host_data_to_buffer");
    rgv->attach_buffer("_src_vertex", *m.mesh_geometry->vertex_buffer, vuk::Access::eNone);
    rgv->attach_buffer("_dst_vertex", merged_vertex_buffer, vuk::Access::eNone);

    rgv->add_pass({
      .name = "mesh_merge_vertex_copy",
      .execute_on = vuk::DomainFlagBits::eTransferOnGraphics,
      .resources = {
        "_dst_vertex"_buffer >> vuk::Access::eTransferWrite,
        "_src_vertex"_buffer >> vuk::Access::eTransferRead,
      },
      .execute = [&m](vuk::CommandBuffer& command_buffer) {
        const auto& buffer = *command_buffer.get_resource_buffer("_dst_vertex");

        VkBufferCopy vertexCopy;
        vertexCopy.dstOffset = m.first_vertex * sizeof(Mesh::Vertex);
        vertexCopy.size = m.vertex_count * sizeof(Mesh::Vertex);
        vertexCopy.srcOffset = 0;

        vkCmdCopyBuffer(command_buffer.get_underlying(), m.mesh_geometry->vertex_buffer->buffer, buffer.buffer, 1, &vertexCopy);

        //command_buffer.copy_buffer("_src_vertex", "_dst_vertex", m.vertex_count * sizeof(Mesh::Vertex));
      }
    });
    futures.emplace_back(rgv, "_dst_vertex+");

    const Ref<vuk::RenderGraph> rgi = create_ref<vuk::RenderGraph>("host_data_to_buffer");
    rgi->attach_buffer("_src_index", *m.mesh_geometry->index_buffer, vuk::Access::eNone);
    rgi->attach_buffer("_dst_index", merged_index_buffer, vuk::Access::eNone);

    rgi->add_pass({
      .name = "mesh_merge_index_copy",
      .execute_on = vuk::DomainFlagBits::eTransferOnGraphics,
      .resources = {
        "_dst_index"_buffer >> vuk::Access::eTransferWrite,
        "_src_index"_buffer >> vuk::Access::eTransferRead,
      },
      .execute = [&m](vuk::CommandBuffer& command_buffer) {
        const auto& buffer = *command_buffer.get_resource_buffer("_dst_index");
        VkBufferCopy indexCopy;
        indexCopy.dstOffset = m.first_index * sizeof(uint32_t);
        indexCopy.size = m.index_count * sizeof(uint32_t);
        indexCopy.srcOffset = 0;

        vkCmdCopyBuffer(command_buffer.get_underlying(), m.mesh_geometry->index_buffer->buffer, buffer.buffer, 1, &indexCopy);
        //command_buffer.copy_buffer("_src_index", "_dst_index", m.index_count * sizeof(uint32_t));
      }
    });
    futures.emplace_back(rgi, "_dst_index+");
  }

  vuk::Compiler compiler{};
  for (auto& f : futures) {
    f.wait(*vk_context->superframe_allocator, compiler);
  }

  auto index_buffer = futures.back().get_result<vuk::Buffer>();
  auto vertex_buffer = std::vector<vuk::Future>::iterator(futures.end() - 1)->get_result<vuk::Buffer>();

  return {index_buffer, vertex_buffer};
}

Ref<Mesh> RendererCommon::generate_quad() {
  std::vector<Mesh::Vertex> vertices(4);
  vertices[0].position = Vec3{-1.0f, -1.0f, 0.0f};
  vertices[0].uv = {};

  vertices[1].position = Vec3{1.0f, -1.0f, 0.0f};
  vertices[1].uv = Vec2{1.0f, 0.0f};

  vertices[2].position = Vec3{1.0f, 1.0f, 0.0f};
  vertices[2].uv = Vec2{1.0f, 1.0f};

  vertices[3].position = Vec3{-1.0f, 1.0f, 0.0f};
  vertices[3].uv = {0.0f, 1.0f};

  const auto indices = std::vector<uint32_t>{0, 1, 2, 2, 3, 0};

  return create_ref<Mesh>(vertices, indices);
}

Ref<Mesh> RendererCommon::generate_cube() {
  std::vector<Mesh::Vertex> vertices(24);

  vertices[0].position = Vec3(0.5f, 0.5f, 0.5f);
  vertices[0].normal = Vec3(0.0f, 0.0f, 1.0f);

  vertices[1].position = Vec3(-0.5f, 0.5f, 0.5f);
  vertices[1].normal = Vec3(0.0f, 0.0f, 1.0f);

  vertices[2].position = Vec3(-0.5f, -0.5f, 0.5f);
  vertices[2].normal = Vec3(0.0f, 0.0f, 1.0f);

  vertices[3].position = Vec3(0.5f, -0.5f, 0.5f);
  vertices[3].normal = Vec3(0.0f, 0.0f, 1.0f);

  vertices[4].position = Vec3(0.5f, 0.5f, 0.5f);
  vertices[4].normal = Vec3(1.0f, 0.0f, 0.0f);

  vertices[5].position = Vec3(0.5f, -0.5f, 0.5f);
  vertices[5].normal = Vec3(1.0f, 0.0f, 0.0f);

  vertices[6].position = Vec3(0.5f, -0.5f, -0.5f);
  vertices[6].normal = Vec3(1.0f, 0.0f, 0.0f);

  vertices[7].position = Vec3(0.5f, 0.5f, -0.5f);
  vertices[7].normal = Vec3(1.0f, 0.0f, 0.0f);

  vertices[8].position = Vec3(0.5f, 0.5f, 0.5f);
  vertices[8].normal = Vec3(0.0f, 1.0f, 0.0f);

  vertices[9].position = Vec3(0.5f, 0.5f, -0.5f);
  vertices[9].normal = Vec3(0.0f, 1.0f, 0.0f);

  vertices[10].position = Vec3(-0.5f, 0.5f, -0.5f);
  vertices[10].normal = Vec3(0.0f, 1.0f, 0.0f);

  vertices[11].position = Vec3(-0.5f, 0.5f, 0.5f);
  vertices[11].normal = Vec3(0.0f, 1.0f, 0.0f);

  vertices[12].position = Vec3(-0.5f, 0.5f, 0.5f);
  vertices[12].normal = Vec3(-1.0f, 0.0f, 0.0f);

  vertices[13].position = Vec3(-0.5f, 0.5f, -0.5f);
  vertices[13].normal = Vec3(-1.0f, 0.0f, 0.0f);

  vertices[14].position = Vec3(-0.5f, -0.5f, -0.5f);
  vertices[14].normal = Vec3(-1.0f, 0.0f, 0.0f);

  vertices[15].position = Vec3(-0.5f, -0.5f, 0.5f);
  vertices[15].normal = Vec3(-1.0f, 0.0f, 0.0f);

  vertices[16].position = Vec3(-0.5f, -0.5f, -0.5f);
  vertices[16].normal = Vec3(0.0f, -1.0f, 0.0f);

  vertices[17].position = Vec3(0.5f, -0.5f, -0.5f);
  vertices[17].normal = Vec3(0.0f, -1.0f, 0.0f);

  vertices[18].position = Vec3(0.5f, -0.5f, 0.5f);
  vertices[18].normal = Vec3(0.0f, -1.0f, 0.0f);

  vertices[19].position = Vec3(-0.5f, -0.5f, 0.5f);
  vertices[19].normal = Vec3(0.0f, -1.0f, 0.0f);

  vertices[20].position = Vec3(0.5f, -0.5f, -0.5f);
  vertices[20].normal = Vec3(0.0f, 0.0f, -1.0f);

  vertices[21].position = Vec3(-0.5f, -0.5f, -0.5f);
  vertices[21].normal = Vec3(0.0f, 0.0f, -1.0f);

  vertices[22].position = Vec3(-0.5f, 0.5f, -0.5f);
  vertices[22].normal = Vec3(0.0f, 0.0f, -1.0f);

  vertices[23].position = Vec3(0.5f, 0.5f, -0.5f);
  vertices[23].normal = Vec3(0.0f, 0.0f, -1.0f);

  for (int i = 0; i < 6; i++) {
    vertices[i * 4 + 0].uv = Vec2(0.0f, 0.0f);
    vertices[i * 4 + 1].uv = Vec2(1.0f, 0.0f);
    vertices[i * 4 + 2].uv = Vec2(1.0f, 1.0f);
    vertices[i * 4 + 3].uv = Vec2(0.0f, 1.0f);
  }

  std::vector<uint32_t> indices = {
    0, 1, 2,
    0, 2, 3,
    4, 5, 6,
    4, 6, 7,
    8, 9, 10,
    8, 10, 11,
    12, 13, 14,
    12, 14, 15,
    16, 17, 18,
    16, 18, 19,
    20, 21, 22,
    20, 22, 23
  };

  return create_ref<Mesh>(vertices, indices);
}

Ref<Mesh> RendererCommon::generate_sphere() {
  std::vector<Mesh::Vertex> vertices;
  std::vector<uint32_t> indices;

  int latitude_bands = 30;
  int longitude_bands = 30;
  float radius = 2;

  for (int i = 0; i <= latitude_bands; i++) {
    float theta = (float)i * glm::pi<float>() / (float)latitude_bands;
    float sinTheta = sin(theta);
    float cosTheta = cos(theta);

    for (int longNumber = 0; longNumber <= longitude_bands; longNumber++) {
      float phi = (float)longNumber * 2.0f * glm::pi<float>() / (float)longitude_bands;
      float sinPhi = sin(phi);
      float cosPhi = cos(phi);

      Mesh::Vertex vs;
      vs.normal[0] = cosPhi * sinTheta;   // x
      vs.normal[1] = cosTheta;            // y
      vs.normal[2] = sinPhi * sinTheta;   // z
      vs.uv[0] = 1.0f - (float)longNumber / (float)longitude_bands; // u
      vs.uv[1] = 1.0f - (float)i / (float)latitude_bands;   // v
      vs.position[0] = radius * vs.normal[0];
      vs.position[1] = radius * vs.normal[1];
      vs.position[2] = radius * vs.normal[2];

      vertices.push_back(vs);
    }

    for (int j = 0; j < latitude_bands; j++) {
      for (int longNumber = 0; longNumber < longitude_bands; longNumber++) {
        int first = j * (longitude_bands + 1) + longNumber;
        int second = first + longitude_bands + 1;

        indices.push_back(first);
        indices.push_back(second);
        indices.push_back(first + 1);

        indices.push_back(second);
        indices.push_back(second + 1);
        indices.push_back(first + 1);
      }
    }
  }

  return create_ref<Mesh>(vertices, indices);
}

void RendererCommon::apply_blur(const std::shared_ptr<vuk::RenderGraph>& render_graph, vuk::Name src_attachment, const vuk::Name attachment_name, const vuk::Name attachment_name_output) {
  auto& allocator = *VulkanContext::get()->superframe_allocator;
  if (!allocator.get_context().is_pipeline_available("gaussian_blur_pipeline")) {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("FullscreenPass.vert"), FileUtils::get_shader_path("FullscreenPass.vert"));
    pci.add_glsl(FileUtils::read_shader_file("SinglePassGaussianBlur.frag"), "SinglePassGaussianBlur.frag");
    allocator.get_context().create_named_pipeline("gaussian_blur_pipeline", pci);
  }

  render_graph->add_pass({
    .name = "blur",
    .resources = {
      vuk::Resource(attachment_name, vuk::Resource::Type::eImage, vuk::eColorRW, attachment_name_output),
      vuk::Resource(src_attachment, vuk::Resource::Type::eImage, vuk::eFragmentSampled),
    },
    .execute = [src_attachment](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_graphics_pipeline("gaussian_blur_pipeline")
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend(vuk::BlendPreset::eOff)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                    .bind_image(0, 0, src_attachment)
                    .bind_sampler(0, 0, vuk::LinearSamplerClamped)
                    .draw(3, 1, 0, 0);
    }
  });
}
}
