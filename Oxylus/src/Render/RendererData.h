#pragma once
#include <vector>

#include "Core/Base.h"
#include "Core/Types.h"

namespace Oxylus {
class Material;
class Mesh;

struct LightingData {
  Vec4 position_and_intensity;
  Vec4 color_and_radius;
  Vec4 rotation;
};

struct MeshData {
  Mesh* mesh_geometry;
  std::vector<Ref<Material>> materials;
  Mat4 transform;
  uint32_t submesh_index = 0; // todo: get rid of this

  MeshData(Mesh* mesh,
           const Mat4& transform,
           const std::vector<Ref<Material>>& materials,
           const uint32_t submeshIndex) : mesh_geometry(mesh), materials(materials), transform(transform),
                                          submesh_index(submeshIndex) { }
};
}
