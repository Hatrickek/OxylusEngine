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

void DebugRenderer::reset(bool clearNDT) {
  OX_SCOPED_ZONE;
  instance->draw_list.debug_lines.clear();
  instance->draw_list.debug_thick_lines.clear();
  instance->draw_list.debug_points.clear();
  instance->draw_list.debug_shapes.clear();

  if (clearNDT) {
    instance->draw_list_ndt.debug_lines.clear();
    instance->draw_list_ndt.debug_thick_lines.clear();
    instance->draw_list_ndt.debug_points.clear();
    instance->draw_list_ndt.debug_shapes.clear();
  }
}

void DebugRenderer::draw_point(const Vec3& pos, float pointRadius, const Vec4& color, bool ndt) {
  OX_SCOPED_ZONE;
  if (ndt)
    instance->draw_list_ndt.debug_points.emplace_back(pos, pointRadius, color);
  else
    instance->draw_list.debug_points.emplace_back(pos, pointRadius, color);
}

void DebugRenderer::draw_thick_line(const Vec3& start, const Vec3& end, float lineWidth, const Vec4& color, bool ndt) {
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
}
