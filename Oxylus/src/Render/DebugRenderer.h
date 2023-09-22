#pragma once

#include "Mesh.h"
#include "Core/Types.h"

namespace Oxylus {
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
  Mat4 ModelMatrix = {};
  Vec4 Color = {};
  Ref<Mesh> ShapeMesh = nullptr;
};

class DebugRenderer {
public:
  DebugRenderer() = default;
  ~DebugRenderer() = default;

  static void Init();
  static void Release();
  static void Reset(bool clearNDT = true);

  /// Note: 'NDT' parameter: No Depth Testing.

  /// Draw Point (circle)
  static void DrawPoint(const Vec3& pos, float pointRadius, const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f), bool ndt = false);

  /// Draw Line with a given thickness
  static void DrawThickLine(const Vec3& start, const Vec3& end, float lineWidth, const Vec4& color = Vec4(1), bool ndt = false);

  /// Draw line with thickness of 1 screen pixel regardless of distance from camera
  static void DrawHairLine(const Vec3& start, const Vec3& end, const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f), bool ndt = false);

  /// Draw Shapes
  static void DrawBox(const Vec3& pos, const Vec3& scale, const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f), const Vec3& rotation = Vec3(0), bool ndt = false);
  static void DrawSphere(const Vec3& pos, const Vec3& scale, const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f), const Vec3& rotation = Vec3(0), bool ndt = false);
  static void DrawCapsule(const Vec3& pos, const Vec3& scale, const Vec4& color = Vec4(1.0f, 1.0f, 1.0f, 1.0f), const Vec3& rotation = Vec3(0), bool ndt = false);

  static DebugRenderer* GetInstance() { return s_Instance; }
  const std::vector<LineInfo>& GetLines(bool depthTested = true) const { return depthTested ? m_DrawList.m_DebugLines : m_DrawListNDT.m_DebugLines; }
  const std::vector<LineInfo>& GetThickLines(bool depthTested = true) const { return depthTested ? m_DrawList.m_DebugThickLines : m_DrawListNDT.m_DebugThickLines; }
  const std::vector<PointInfo>& GetPoints(bool depthTested = true) const { return depthTested ? m_DrawList.m_DebugPoints : m_DrawListNDT.m_DebugPoints; }
  const std::vector<ShapeInfo>& GetShapes(bool depthTested = true) const { return depthTested ? m_DrawList.m_DebugShapes : m_DrawListNDT.m_DebugShapes; }

private:
  static DebugRenderer* s_Instance;

  struct DebugDrawList {
    std::vector<LineInfo> m_DebugLines;
    std::vector<PointInfo> m_DebugPoints;
    std::vector<LineInfo> m_DebugThickLines;
    std::vector<ShapeInfo> m_DebugShapes;
  };

  struct DebugDrawData {
    Ref<Mesh> Cube = nullptr;
    Ref<Mesh> Sphere = nullptr;
  } m_DebugDrawData;

  DebugDrawList m_DrawList;
  DebugDrawList m_DrawListNDT;
};
}
