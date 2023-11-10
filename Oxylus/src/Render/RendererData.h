#pragma once
#include <vector>

#include "Core/Base.h"
#include "Core/Types.h"
#include "Render/Mesh.h"

namespace Oxylus {
class Material;

struct LightingData {
  Vec4 position_and_intensity = {};
  Vec4 color_and_radius = {};
  Vec4 rotation = {};
  Vec4 _pad = {};
};

struct MeshData {
  uint32_t first_vertex = 0;
  uint32_t first_index = 0;
  uint32_t index_count = 0;
  uint32_t vertex_count = 0;
  bool is_merged = false;

  Mesh* mesh_geometry;
  std::vector<Ref<Material>> materials;
  Mat4 transform;
  uint32_t submesh_index = 0;
};
}
