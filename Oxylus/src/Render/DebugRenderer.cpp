#include "DebugRenderer.h"

#include "RendererCommon.h"

#include "Utils/OxMath.h"
#include "Utils/Profiler.h"

namespace Oxylus {
DebugRenderer* DebugRenderer::instance = nullptr;

void DebugRenderer::init() {
  if (instance)
    return;

  instance = new DebugRenderer();

  instance->debug_draw_data.cube = RendererCommon::generate_cube();
  instance->debug_draw_data.sphere = RendererCommon::generate_sphere();
}

void DebugRenderer::release() {
  delete instance;
  instance = nullptr;
}

void DebugRenderer::reset(bool clear_ndt) {
  OX_SCOPED_ZONE;
  instance->draw_list.debug_lines.clear();
  instance->draw_list.debug_thick_lines.clear();
  instance->draw_list.debug_points.clear();
  instance->draw_list.debug_shapes.clear();

  if (clear_ndt) {
    instance->draw_list_ndt.debug_lines.clear();
    instance->draw_list_ndt.debug_thick_lines.clear();
    instance->draw_list_ndt.debug_points.clear();
    instance->draw_list_ndt.debug_shapes.clear();
  }
}

void DebugRenderer::draw_point(const Vec3& pos, float point_radius, const Vec4& color, bool ndt) {
  OX_SCOPED_ZONE;
  if (ndt)
    instance->draw_list_ndt.debug_points.emplace_back(pos, point_radius, color);
  else
    instance->draw_list.debug_points.emplace_back(pos, point_radius, color);
}

void DebugRenderer::draw_thick_line(const Vec3& start, const Vec3& end, float line_width, const Vec4& color, bool ndt) {
  OX_SCOPED_ZONE;
  if (ndt)
    instance->draw_list_ndt.debug_thick_lines.emplace_back(start, end, color);
  else
    instance->draw_list.debug_thick_lines.emplace_back(start, end, color);
}

void DebugRenderer::draw_hair_line(const Vec3& start, const Vec3& end, const Vec4& color, bool ndt) {
  OX_SCOPED_ZONE;
  if (ndt)
    instance->draw_list_ndt.debug_lines.emplace_back(start, end, color);
  else
    instance->draw_list.debug_lines.emplace_back(start, end, color);
}

void DebugRenderer::draw_box(const Vec3& pos, const Vec3& scale, const Vec4& color, const Vec3& rotation, const bool ndt) {
  const auto transform = glm::translate(glm::mat4(1.0f), pos) * glm::toMat4(glm::quat(rotation)) * glm::scale(glm::mat4(1.0f), scale);
  if (ndt)
    instance->draw_list_ndt.debug_shapes.emplace_back(ShapeInfo{transform, color, instance->debug_draw_data.cube});
  else
    instance->draw_list.debug_shapes.emplace_back(ShapeInfo{transform, color, instance->debug_draw_data.cube});
}

void DebugRenderer::draw_sphere(const Vec3& pos, const Vec3& scale, const Vec4& color, const Vec3& rotation, bool ndt) {
  const auto transform = glm::translate(glm::mat4(1.0f), pos) * glm::toMat4(glm::quat(rotation)) * glm::scale(glm::mat4(1.0f), scale);
  if (ndt)
    instance->draw_list_ndt.debug_shapes.emplace_back(ShapeInfo{transform, color, instance->debug_draw_data.sphere});
  else
    instance->draw_list.debug_shapes.emplace_back(ShapeInfo{transform, color, instance->debug_draw_data.sphere});
}

void DebugRenderer::draw_capsule(const Vec3& pos, const Vec3& scale, const Vec4& color, const Vec3& rotation, bool ndt) { }

void DebugRenderer::draw_mesh(const Ref<Mesh>& mesh, const Vec3& pos, const Vec3& scale, const Vec4& color, const Vec3& rotation, bool ndt) {
  const auto transform = glm::translate(glm::mat4(1.0f), pos) * glm::toMat4(glm::quat(rotation)) * glm::scale(glm::mat4(1.0f), scale);
  if (ndt)
    instance->draw_list_ndt.debug_shapes.emplace_back(ShapeInfo{transform, color, mesh});
  else
    instance->draw_list.debug_shapes.emplace_back(ShapeInfo{transform, color, mesh});
}

void DebugRenderer::draw_mesh(const Ref<Mesh>& mesh, const Mat4& model_matrix, const Vec4& color, bool ndt) {
  if (ndt)
    instance->draw_list_ndt.debug_shapes.emplace_back(ShapeInfo{model_matrix, color, mesh});
  else
    instance->draw_list.debug_shapes.emplace_back(ShapeInfo{model_matrix, color, mesh});
}

void DebugRenderer::draw_bb(const Mesh::BoundingBox& bb, const Vec4& color, bool corners_only, float width) {
  glm::vec3 uuu = bb.max;
  glm::vec3 lll = bb.min;

  glm::vec3 ull(uuu.x, lll.y, lll.z);
  glm::vec3 uul(uuu.x, uuu.y, lll.z);
  glm::vec3 ulu(uuu.x, lll.y, uuu.z);

  glm::vec3 luu(lll.x, uuu.y, uuu.z);
  glm::vec3 llu(lll.x, lll.y, uuu.z);
  glm::vec3 lul(lll.x, uuu.y, lll.z);

  // Draw edges
  if (!corners_only) {
    draw_thick_line(luu, uuu, width, color);
    draw_thick_line(lul, uul, width, color);
    draw_thick_line(llu, ulu, width, color);
    draw_thick_line(lll, ull, width, color);

    draw_thick_line(lul, lll, width, color);
    draw_thick_line(uul, ull, width, color);
    draw_thick_line(luu, llu, width, color);
    draw_thick_line(uuu, ulu, width, color);

    draw_thick_line(lll, llu, width, color);
    draw_thick_line(ull, ulu, width, color);
    draw_thick_line(lul, luu, width, color);
    draw_thick_line(uul, uuu, width, color);
  }
  else {
    draw_thick_line(luu, luu + (uuu - luu) * 0.25f, width, color);
    draw_thick_line(luu + (uuu - luu) * 0.75f, uuu, width, color);

    draw_thick_line(lul, lul + (uul - lul) * 0.25f, width, color);
    draw_thick_line(lul + (uul - lul) * 0.75f, uul, width, color);

    draw_thick_line(llu, llu + (ulu - llu) * 0.25f, width, color);
    draw_thick_line(llu + (ulu - llu) * 0.75f, ulu, width, color);

    draw_thick_line(lll, lll + (ull - lll) * 0.25f, width, color);
    draw_thick_line(lll + (ull - lll) * 0.75f, ull, width, color);

    draw_thick_line(lul, lul + (lll - lul) * 0.25f, width, color);
    draw_thick_line(lul + (lll - lul) * 0.75f, lll, width, color);

    draw_thick_line(uul, uul + (ull - uul) * 0.25f, width, color);
    draw_thick_line(uul + (ull - uul) * 0.75f, ull, width, color);

    draw_thick_line(luu, luu + (llu - luu) * 0.25f, width, color);
    draw_thick_line(luu + (llu - luu) * 0.75f, llu, width, color);

    draw_thick_line(uuu, uuu + (ulu - uuu) * 0.25f, width, color);
    draw_thick_line(uuu + (ulu - uuu) * 0.75f, ulu, width, color);

    draw_thick_line(lll, lll + (llu - lll) * 0.25f, width, color);
    draw_thick_line(lll + (llu - lll) * 0.75f, llu, width, color);

    draw_thick_line(ull, ull + (ulu - ull) * 0.25f, width, color);
    draw_thick_line(ull + (ulu - ull) * 0.75f, ulu, width, color);

    draw_thick_line(lul, lul + (luu - lul) * 0.25f, width, color);
    draw_thick_line(lul + (luu - lul) * 0.75f, luu, width, color);

    draw_thick_line(uul, uul + (uuu - uul) * 0.25f, width, color);
    draw_thick_line(uul + (uuu - uul) * 0.75f, uuu, width, color);
  }
}
}
