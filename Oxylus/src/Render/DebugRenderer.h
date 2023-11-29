#pragma once

#include "Mesh.h"
#include "Core/Types.h"

namespace Oxylus {
struct LineVertexData {
  Vec3 position = {};
  Vec4 color = {};
};

struct LineInfo {
  Vec3 p1 = {};
  Vec3 p2 = {};
  Vec4 col = {};

  LineInfo(const Vec3& pos1, const Vec3& pos2, const Vec4& color)
    : p1(pos1), p2(pos2), col(color) { }
};

struct PointInfo {
  Vec3 p1 = {};
  Vec4 col = {};
  float size = 0;

  PointInfo(const Vec3& pos1, float s, const Vec4& color)
    : p1(pos1), col(color), size(s) { }
};

struct ShapeInfo {
  Mat4 model_matrix = {};
  Vec4 color = {};
  Ref<Mesh> shape_mesh = nullptr;
};

class DebugRenderer {
public:
  DebugRenderer() = default;
  ~DebugRenderer() = default;

  static void init();
  static void release();
  static void reset(bool clear_ndt = true);

  /// Note: 'NDT' parameter: No Depth Testing.

  /// Draw Point (circle)
  static void draw_point(const Vec3& pos, float point_radius, const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f), bool ndt = false);

  /// Draw Line with a given thickness
  static void draw_thick_line(const Vec3& start, const Vec3& end, float line_width, const Vec4& color = Vec4(1), bool ndt = false);

  /// Draw line with thickness of 1 screen pixel regardless of distance from camera
  static void draw_hair_line(const Vec3& start, const Vec3& end, const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f), bool ndt = false);

  /// Draw Shapes
  static void draw_box(const Vec3& pos, const Vec3& scale, const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f), const Vec3& rotation = Vec3(0), bool ndt = false);
  static void draw_sphere(const Vec3& pos, const Vec3& scale, const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f), const Vec3& rotation = Vec3(0), bool ndt = false);
  static void draw_capsule(const Vec3& pos, const Vec3& scale, const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f), const Vec3& rotation = Vec3(0), bool ndt = false);
  static void draw_mesh(const Ref<Mesh>& mesh, const Vec3& pos, const Vec3& scale, const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f), const Vec3& rotation = Vec3(0), bool ndt = false);
  static void draw_mesh(const Ref<Mesh>& mesh, const Mat4& model_matrix, const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f), bool ndt = false);
  static void draw_bb(const Mesh::BoundingBox& bb, const Vec4& color = Vec4(1.0f), bool corners_only = false, float width = 1.0f);

  static DebugRenderer* get_instance() { return instance; }
  const std::vector<LineInfo>& get_lines(bool depth_tested = true) const { return depth_tested ? draw_list.debug_lines : draw_list_ndt.debug_lines; }
  const std::vector<LineInfo>& get_thick_lines(bool depth_tested = true) const { return depth_tested ? draw_list.debug_thick_lines : draw_list_ndt.debug_thick_lines; }
  const std::vector<PointInfo>& get_points(bool depth_tested = true) const { return depth_tested ? draw_list.debug_points : draw_list_ndt.debug_points; }
  const std::vector<ShapeInfo>& get_shapes(bool depth_tested = true) const { return depth_tested ? draw_list.debug_shapes : draw_list_ndt.debug_shapes; }

private:
  static DebugRenderer* instance;

  struct DebugDrawList {
    std::vector<LineInfo> debug_lines = {};
    std::vector<PointInfo> debug_points = {};
    std::vector<LineInfo> debug_thick_lines = {};
    std::vector<ShapeInfo> debug_shapes = {};
  };

  struct DebugDrawData {
    Ref<Mesh> cube = nullptr;
    Ref<Mesh> sphere = nullptr;
  } debug_draw_data;

  DebugDrawList draw_list;
  DebugDrawList draw_list_ndt;
};
}
