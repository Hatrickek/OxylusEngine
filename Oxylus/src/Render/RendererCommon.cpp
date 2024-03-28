#include "RendererCommon.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <vuk/CommandBuffer.hpp>
#include <vuk/Partials.hpp>
#include <vuk/Pipeline.hpp>
#include <vuk/RenderGraph.hpp>

#include "Mesh.h"

#include "Assets/AssetManager.hpp"
#include "Core/FileSystem.hpp"

#include "Core/Types.hpp"

#include "Utils/Log.hpp"

#include "Utils/VukCommon.hpp"
#include "Vulkan/VkContext.hpp"

namespace ox {
RendererCommon::MeshLib RendererCommon::mesh_lib = {};

vuk::Value<vuk::ImageAttachment> RendererCommon::generate_cubemap_from_equirectangular(const vuk::ImageAttachment& cubemap) {
  const auto hdr_image = vuk::clear_image(vuk::declare_ia("hdr_image", cubemap), vuk::Black<float>);
  const auto blurred_hdr_image = vuk::clear_image(vuk::declare_ia("blurred_hdr_image", cubemap), vuk::Black<float>);
  /// TODO: generate mips instead
  apply_blur(hdr_image, blurred_hdr_image);

  auto& allocator = VkContext::get()->superframe_allocator;
  if (!allocator->get_context().is_pipeline_available("equirectangular_to_cubemap")) {
    vuk::PipelineBaseCreateInfo equirectangular_to_cubemap;
    equirectangular_to_cubemap.add_glsl(FileSystem::read_shader_file("Cubemap.vert"), "Cubemap.vert");
    equirectangular_to_cubemap.add_glsl(FileSystem::read_shader_file("EquirectangularToCubemap.frag"), "EquirectangularToCubemap.frag");
    allocator->get_context().create_named_pipeline("equirectangular_to_cubemap", equirectangular_to_cubemap);
  }

  constexpr auto size = 2048;
  auto attch = vuk::ImageAttachment::from_preset(vuk::ImageAttachment::Preset::eRTTCube,
                                                 vuk::Format::eR32G32B32A32Sfloat,
                                                 {size, size, 1},
                                                 vuk::Samples::e1);
  auto target = vuk::clear_image(vuk::declare_ia("cubemap", attch), vuk::Black<float>);

  const auto capture_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
  const Mat4 capture_views[] = {lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
                                lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
                                lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)),
                                lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)),
                                lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f)),
                                lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f))};

  auto cubemap_pass = vuk::make_pass("env_cubemap",
                                     [capture_projection, capture_views](vuk::CommandBuffer& command_buffer,
                                                                         VUK_IA(vuk::eColorWrite) cube_map,
                                                                         VUK_IA(vuk::eFragmentSampled) hdr) {
    command_buffer.set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend(vuk::BlendPreset::eOff)
      .set_rasterization({})
      .bind_image(0, 2, hdr)
      .bind_sampler(0, 2, vuk::LinearSamplerClamped)
      .bind_graphics_pipeline("equirectangular_to_cubemap");

    auto* projection = command_buffer.scratch_buffer<glm::mat4>(0, 0);
    *projection = capture_projection;
    const auto view = command_buffer.scratch_buffer<glm::mat4[6]>(0, 1);
    memcpy(view, capture_views, sizeof(capture_views));

    const auto cube = generate_cube();
    cube->bind_vertex_buffer(command_buffer);
    cube->bind_index_buffer(command_buffer);
    command_buffer.draw_indexed(cube->index_count, 6, 0, 0, 0);

    return cube_map;
  });

  return cubemap_pass(blurred_hdr_image, target);
}

Shared<Mesh> RendererCommon::generate_quad() {
  if (mesh_lib.quad)
    return mesh_lib.quad;

  std::vector<Vertex> vertices(4);
  vertices[0].position = Vec3{-1.0f, -1.0f, 0.0f};
  vertices[0].uv = {};

  vertices[1].position = Vec3{1.0f, -1.0f, 0.0f};
  vertices[1].uv = Vec2{1.0f, 0.0f};

  vertices[2].position = Vec3{1.0f, 1.0f, 0.0f};
  vertices[2].uv = Vec2{1.0f, 1.0f};

  vertices[3].position = Vec3{-1.0f, 1.0f, 0.0f};
  vertices[3].uv = {0.0f, 1.0f};

  const auto indices = std::vector<uint32_t>{0, 1, 2, 2, 3, 0};

  mesh_lib.quad = create_shared<Mesh>(vertices, indices);

  return mesh_lib.quad;
}

Shared<Mesh> RendererCommon::generate_cube() {
  if (mesh_lib.cube)
    return mesh_lib.cube;

  std::vector<Vertex> vertices(24);

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

  std::vector<uint32_t> indices = {0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,  8,  9,  10, 8,  10, 11,
                                   12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23};

  mesh_lib.cube = create_shared<Mesh>(vertices, indices);

  return mesh_lib.cube;
}

Shared<Mesh> RendererCommon::generate_sphere() {
  if (mesh_lib.sphere)
    return mesh_lib.sphere;

  std::vector<Vertex> vertices;
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

      Vertex vs;
      vs.normal[0] = cosPhi * sinTheta;                             // x
      vs.normal[1] = cosTheta;                                      // y
      vs.normal[2] = sinPhi * sinTheta;                             // z
      vs.uv[0] = 1.0f - (float)longNumber / (float)longitude_bands; // u
      vs.uv[1] = 1.0f - (float)i / (float)latitude_bands;           // v
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

  mesh_lib.sphere = create_shared<Mesh>(vertices, indices);

  return mesh_lib.sphere;
}

vuk::Value<vuk::ImageAttachment> RendererCommon::apply_blur(const vuk::Value<vuk::ImageAttachment>& src_attachment,
                                                            const vuk::Value<vuk::ImageAttachment>& dst_attachment) {
  auto& allocator = *VkContext::get()->superframe_allocator;
  if (!allocator.get_context().is_pipeline_available("gaussian_blur_pipeline")) {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(FileSystem::read_shader_file("FullscreenTriangle.hlsl"),
                 FileSystem::get_shader_path("FullscreenTriangle.hlsl"),
                 vuk::HlslShaderStage::eVertex);
    pci.add_glsl(FileSystem::read_shader_file("PostProcess/SinglePassGaussianBlur.frag"), "PostProcess/SinglePassGaussianBlur.frag");
    allocator.get_context().create_named_pipeline("gaussian_blur_pipeline", pci);
  }

  auto pass = vuk::make_pass("blur", [](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eFragmentSampled) src, VUK_IA(vuk::eColorRW) target) {
    command_buffer.bind_graphics_pipeline("gaussian_blur_pipeline")
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend(vuk::BlendPreset::eOff)
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
      .bind_image(0, 0, src)
      .bind_sampler(0, 0, vuk::LinearSamplerClamped)
      .draw(3, 1, 0, 0);

    return target;
  });

  return pass(src_attachment, dst_attachment);
}
} // namespace ox
