#include "DebugRenderer.h"

#include "Mesh.h"
#include "Core/Resources.h"
#include "Utils/OxMath.h"
#include "Utils/Profiler.h"

namespace Oxylus {
DebugRenderer* DebugRenderer::s_Instance = nullptr;

void DebugRenderer::Init() {
  if (s_Instance)
    return;

  s_Instance = new DebugRenderer();

  s_Instance->m_DebugDrawData.Cube = CreateRef<Mesh>(Resources::GetResourcesPath("Objects/cube.glb"),
    Mesh::FileLoadingFlags::DontLoadImages | Mesh::FileLoadingFlags::DontCreateMaterials,
    1.0f);

  s_Instance->m_DebugDrawData.Sphere = CreateRef<Mesh>(Resources::GetResourcesPath("Objects/sphere.gltf"),
    Mesh::FileLoadingFlags::DontLoadImages | Mesh::FileLoadingFlags::DontCreateMaterials,
    1.0f);
}

void DebugRenderer::Release() {
  delete s_Instance;
  s_Instance = nullptr;
}

void DebugRenderer::Reset(bool clearNDT) {
  OX_SCOPED_ZONE;
  s_Instance->m_DrawList.m_DebugLines.clear();
  s_Instance->m_DrawList.m_DebugThickLines.clear();
  s_Instance->m_DrawList.m_DebugPoints.clear();
  s_Instance->m_DrawList.m_DebugShapes.clear();

  if (clearNDT) {
    s_Instance->m_DrawListNDT.m_DebugLines.clear();
    s_Instance->m_DrawListNDT.m_DebugThickLines.clear();
    s_Instance->m_DrawListNDT.m_DebugPoints.clear();
    s_Instance->m_DrawListNDT.m_DebugShapes.clear();
  }
}

void DebugRenderer::DrawPoint(const Vec3& pos, float pointRadius, const Vec4& color, bool ndt) {
  OX_SCOPED_ZONE;
  if (ndt)
    s_Instance->m_DrawListNDT.m_DebugPoints.emplace_back(pos, pointRadius, color);
  else
    s_Instance->m_DrawList.m_DebugPoints.emplace_back(pos, pointRadius, color);
}

void DebugRenderer::DrawThickLine(const Vec3& start, const Vec3& end, float lineWidth, const Vec4& color, bool ndt) {
  OX_SCOPED_ZONE;
  if (ndt)
    s_Instance->m_DrawListNDT.m_DebugThickLines.emplace_back(start, end, color);
  else
    s_Instance->m_DrawList.m_DebugThickLines.emplace_back(start, end, color);
}

void DebugRenderer::DrawHairLine(const Vec3& start, const Vec3& end, const Vec4& color, bool ndt) {
  OX_SCOPED_ZONE;
  if (ndt)
    s_Instance->m_DrawListNDT.m_DebugLines.emplace_back(start, end, color);
  else
    s_Instance->m_DrawList.m_DebugLines.emplace_back(start, end, color);
}

void DebugRenderer::DrawBox(const Vec3& pos, const Vec3& scale, const Vec4& color, const Vec3& rotation, const bool ndt) {
  const auto transform = glm::translate(glm::mat4(1.0f), pos) * glm::toMat4(glm::quat(rotation)) * glm::scale(glm::mat4(1.0f), scale);
  if (ndt)
    s_Instance->m_DrawListNDT.m_DebugShapes.emplace_back(ShapeInfo{transform, color, s_Instance->m_DebugDrawData.Cube});
  else
    s_Instance->m_DrawList.m_DebugShapes.emplace_back(ShapeInfo{transform, color, s_Instance->m_DebugDrawData.Cube});
}

void DebugRenderer::DrawSphere(const Vec3& pos, const Vec3& scale, const Vec4& color, const Vec3& rotation, bool ndt) {
  const auto transform = glm::translate(glm::mat4(1.0f), pos) * glm::toMat4(glm::quat(rotation)) * glm::scale(glm::mat4(1.0f), scale);
  if (ndt)
    s_Instance->m_DrawListNDT.m_DebugShapes.emplace_back(ShapeInfo{transform, color, s_Instance->m_DebugDrawData.Sphere});
  else
    s_Instance->m_DrawList.m_DebugShapes.emplace_back(ShapeInfo{transform, color, s_Instance->m_DebugDrawData.Sphere});
}

void DebugRenderer::DrawCapsule(const Vec3& pos, const Vec3& scale, const Vec4& color, const Vec3& rotation, bool ndt) { }
}
