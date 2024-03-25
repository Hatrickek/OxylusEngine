#include "DebugRenderer.hpp"

#include <vuk/Partials.hpp>

#include "RendererCommon.h"

#include "Utils/OxMath.hpp"
#include "Utils/Profiler.hpp"

#include "Vulkan/VkContext.hpp"

namespace ox {
DebugRenderer* DebugRenderer::instance = nullptr;

void DebugRenderer::init() {
  OX_SCOPED_ZONE;
  if (instance)
    return;

  instance = new DebugRenderer();

  std::vector<uint32_t> indices = {};
  indices.resize(MAX_LINE_INDICES);

  for (uint32_t i = 0; i < MAX_LINE_INDICES; i++) {
    indices[i] = i;
  }

  instance->debug_renderer_context.cube = RendererCommon::generate_cube();
  instance->debug_renderer_context.sphere = RendererCommon::generate_sphere();

  auto [i_buff, i_buff_fut] = create_buffer(*VkContext::get()->superframe_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(indices));

  auto compiler = vuk::Compiler{};
  i_buff_fut.wait(*VkContext::get()->superframe_allocator, compiler);

  instance->debug_renderer_context.index_buffer = std::move(i_buff);
}

void DebugRenderer::release() {
  delete instance;
  instance = nullptr;
}

void DebugRenderer::reset(bool clear_depth_tested) {
  OX_SCOPED_ZONE;
  instance->draw_list.debug_lines.clear();
  instance->draw_list.debug_points.clear();
  instance->draw_list.debug_shapes.clear();

  if (clear_depth_tested) {
    instance->draw_list_depth_tested.debug_lines.clear();
    instance->draw_list_depth_tested.debug_points.clear();
    instance->draw_list_depth_tested.debug_shapes.clear();
  }
}

void DebugRenderer::draw_point(const Vec3& pos, float point_radius, const Vec4& color, bool depth_tested) {
  OX_SCOPED_ZONE;
  if (depth_tested)
    instance->draw_list_depth_tested.debug_points.emplace_back(pos, point_radius, color);
  else
    instance->draw_list.debug_points.emplace_back(pos, point_radius, color);
}

void DebugRenderer::draw_line(const Vec3& start, const Vec3& end, float line_width, const Vec4& color, bool depth_tested) {
  OX_SCOPED_ZONE;
  if (depth_tested)
    instance->draw_list_depth_tested.debug_lines.emplace_back(start, end, color);
  else
    instance->draw_list.debug_lines.emplace_back(start, end, color);
}

void DebugRenderer::draw_hair_line(const Vec3& start, const Vec3& end, const Vec4& color, bool depth_tested) {
  OX_SCOPED_ZONE;
  if (depth_tested)
    instance->draw_list_depth_tested.debug_lines.emplace_back(start, end, color);
  else
    instance->draw_list.debug_lines.emplace_back(start, end, color);
}

void DebugRenderer::draw_box(const Vec3& pos, const Vec3& scale, const Vec4& color, const Vec3& rotation, const bool depth_tested) {
  const auto transform = translate(Mat4(1.0f), pos) * toMat4(glm::quat(rotation)) * glm::scale(Mat4(1.0f), scale);
  if (depth_tested)
    instance->draw_list_depth_tested.debug_shapes.emplace_back(ShapeInfo{transform, color, instance->debug_renderer_context.cube});
  else
    instance->draw_list.debug_shapes.emplace_back(ShapeInfo{transform, color, instance->debug_renderer_context.cube});
}

void DebugRenderer::draw_sphere(const Vec3& pos, const Vec3& scale, const Vec4& color, const Vec3& rotation, bool depth_tested) {
  const auto transform = translate(Mat4(1.0f), pos) * toMat4(glm::quat(rotation)) * glm::scale(Mat4(1.0f), scale);
  if (depth_tested)
    instance->draw_list_depth_tested.debug_shapes.emplace_back(ShapeInfo{transform, color, instance->debug_renderer_context.sphere});
  else
    instance->draw_list.debug_shapes.emplace_back(ShapeInfo{transform, color, instance->debug_renderer_context.sphere});
}

void DebugRenderer::draw_capsule(const Vec3& pos, const Vec3& scale, const Vec4& color, const Vec3& rotation, bool ndt) { }

void DebugRenderer::draw_mesh(const Shared<Mesh>& mesh, const Vec3& pos, const Vec3& scale, const Vec4& color, const Vec3& rotation, bool depth_tested) {
  const auto transform = translate(Mat4(1.0f), pos) * toMat4(glm::quat(rotation)) * glm::scale(Mat4(1.0f), scale);
  if (depth_tested)
    instance->draw_list_depth_tested.debug_shapes.emplace_back(ShapeInfo{transform, color, mesh});
  else
    instance->draw_list.debug_shapes.emplace_back(ShapeInfo{transform, color, mesh});
}

void DebugRenderer::draw_mesh(const Shared<Mesh>& mesh, const Mat4& model_matrix, const Vec4& color, bool depth_tested) {
  if (depth_tested)
    instance->draw_list_depth_tested.debug_shapes.emplace_back(ShapeInfo{model_matrix, color, mesh});
  else
    instance->draw_list.debug_shapes.emplace_back(ShapeInfo{model_matrix, color, mesh});
}

void DebugRenderer::draw_aabb(const AABB& aabb, const Vec4& color, bool corners_only, float width, bool depth_tested) {
  Vec3 uuu = aabb.max;
  Vec3 lll = aabb.min;

  Vec3 ull(uuu.x, lll.y, lll.z);
  Vec3 uul(uuu.x, uuu.y, lll.z);
  Vec3 ulu(uuu.x, lll.y, uuu.z);

  Vec3 luu(lll.x, uuu.y, uuu.z);
  Vec3 llu(lll.x, lll.y, uuu.z);
  Vec3 lul(lll.x, uuu.y, lll.z);

  // Draw edges
  if (!corners_only) {
    draw_line(luu, uuu, width, color, depth_tested);
    draw_line(lul, uul, width, color, depth_tested);
    draw_line(llu, ulu, width, color, depth_tested);
    draw_line(lll, ull, width, color, depth_tested);

    draw_line(lul, lll, width, color, depth_tested);
    draw_line(uul, ull, width, color, depth_tested);
    draw_line(luu, llu, width, color, depth_tested);
    draw_line(uuu, ulu, width, color, depth_tested);

    draw_line(lll, llu, width, color, depth_tested);
    draw_line(ull, ulu, width, color, depth_tested);
    draw_line(lul, luu, width, color, depth_tested);
    draw_line(uul, uuu, width, color, depth_tested);
  }
  else {
    draw_line(luu, luu + (uuu - luu) * 0.25f, width, color, depth_tested);
    draw_line(luu + (uuu - luu) * 0.75f, uuu, width, color, depth_tested);

    draw_line(lul, lul + (uul - lul) * 0.25f, width, color, depth_tested);
    draw_line(lul + (uul - lul) * 0.75f, uul, width, color, depth_tested);

    draw_line(llu, llu + (ulu - llu) * 0.25f, width, color, depth_tested);
    draw_line(llu + (ulu - llu) * 0.75f, ulu, width, color, depth_tested);

    draw_line(lll, lll + (ull - lll) * 0.25f, width, color, depth_tested);
    draw_line(lll + (ull - lll) * 0.75f, ull, width, color, depth_tested);

    draw_line(lul, lul + (lll - lul) * 0.25f, width, color, depth_tested);
    draw_line(lul + (lll - lul) * 0.75f, lll, width, color, depth_tested);

    draw_line(uul, uul + (ull - uul) * 0.25f, width, color, depth_tested);
    draw_line(uul + (ull - uul) * 0.75f, ull, width, color, depth_tested);

    draw_line(luu, luu + (llu - luu) * 0.25f, width, color, depth_tested);
    draw_line(luu + (llu - luu) * 0.75f, llu, width, color, depth_tested);

    draw_line(uuu, uuu + (ulu - uuu) * 0.25f, width, color, depth_tested);
    draw_line(uuu + (ulu - uuu) * 0.75f, ulu, width, color, depth_tested);

    draw_line(lll, lll + (llu - lll) * 0.25f, width, color, depth_tested);
    draw_line(lll + (llu - lll) * 0.75f, llu, width, color, depth_tested);

    draw_line(ull, ull + (ulu - ull) * 0.25f, width, color, depth_tested);
    draw_line(ull + (ulu - ull) * 0.75f, ulu, width, color, depth_tested);

    draw_line(lul, lul + (luu - lul) * 0.25f, width, color, depth_tested);
    draw_line(lul + (luu - lul) * 0.75f, luu, width, color, depth_tested);

    draw_line(uul, uul + (uuu - uul) * 0.25f, width, color, depth_tested);
    draw_line(uul + (uuu - uul) * 0.75f, uuu, width, color, depth_tested);
  }
}

std::pair<std::vector<Vertex>, uint32_t> DebugRenderer::get_vertices_from_lines(const std::vector<LineInfo>& lines) {
  std::vector<Vertex> vertices = {};
  uint32_t indices = 0;

  for (const auto& line : lines) {
    // store color in normals for simplicity
    vertices.emplace_back(Vertex{.position = line.p1, .normal = line.col});
    vertices.emplace_back(Vertex{.position = line.p2, .normal = line.col});

    indices += 2;
  }

  return {vertices, indices};
}
}
