#pragma once

#include "Mesh.h"
#include "Core/Types.h"

namespace ox {
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
  Shared<Mesh> shape_mesh = nullptr;
};

class DebugRenderer {
public:
  DebugRenderer() = default;
  ~DebugRenderer() = default;

  static void init();
  static void release();
  static void reset(bool clear_depth_tested = true);

  /// Note: 'NDT' parameter: No Depth Testing.

  /// Draw Point (circle)
  static void draw_point(const Vec3& pos, float point_radius, const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f), bool depth_tested = false);

  /// Draw Line with a given thickness
  static void draw_line(const Vec3& start, const Vec3& end, float line_width, const Vec4& color = Vec4(1), bool depth_tested = false);

  /// Draw line with thickness of 1 screen pixel regardless of distance from camera
  static void draw_hair_line(const Vec3& start, const Vec3& end, const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f), bool depth_tested = false);

  /// Draw Shapes
  static void draw_box(const Vec3& pos, const Vec3& scale, const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f), const Vec3& rotation = Vec3(0), bool depth_tested = false);
  static void draw_sphere(const Vec3& pos, const Vec3& scale, const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f), const Vec3& rotation = Vec3(0), bool depth_tested = false);
  static void draw_capsule(const Vec3& pos, const Vec3& scale, const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f), const Vec3& rotation = Vec3(0), bool ndt = false);
  static void draw_mesh(const Shared<Mesh>& mesh, const Vec3& pos, const Vec3& scale, const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f), const Vec3& rotation = Vec3(0), bool depth_tested = false);
  static void draw_mesh(const Shared<Mesh>& mesh, const Mat4& model_matrix, const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f), bool depth_tested = false);
  static void draw_aabb(const AABB& aabb, const Vec4& color = Vec4(1.0f), bool corners_only = false, float width = 1.0f, bool depth_tested = false);

  static DebugRenderer* get_instance() { return instance; }
  const std::vector<LineInfo>& get_lines(bool depth_tested = true) const { return !depth_tested ? draw_list.debug_lines : draw_list_depth_tested.debug_lines; }
  const std::vector<PointInfo>& get_points(bool depth_tested = true) const { return !depth_tested ? draw_list.debug_points : draw_list_depth_tested.debug_points; }
  const std::vector<ShapeInfo>& get_shapes(bool depth_tested = true) const { return !depth_tested ? draw_list.debug_shapes : draw_list_depth_tested.debug_shapes; }

  const vuk::Unique<vuk::Buffer>& get_global_index_buffer() const { return debug_renderer_context.index_buffer; }

  static std::pair<std::vector<Vertex>, uint32_t> get_vertices_from_lines(const std::vector<LineInfo>& lines);

private:
  static DebugRenderer* instance;

  static constexpr uint32_t MAX_LINES = 10'000;
  static constexpr uint32_t MAX_LINE_VERTICES = MAX_LINES * 2;
  static constexpr uint32_t MAX_LINE_INDICES = MAX_LINES * 6;

  struct DebugDrawList {
    std::vector<LineInfo> debug_lines = {};
    std::vector<PointInfo> debug_points = {};
    std::vector<ShapeInfo> debug_shapes = {};
  };

  struct DebugRendererContext {
    vuk::Unique<vuk::Buffer> index_buffer;
    Shared<Mesh> cube;
    Shared<Mesh> sphere;
  } debug_renderer_context;

  DebugDrawList draw_list;
  DebugDrawList draw_list_depth_tested;
};
}
