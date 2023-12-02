#pragma once
#include <vector>

#include "Core/Base.h"
#include "Core/Components.h"
#include "Core/Types.h"
#include "Render/Mesh.h"

namespace Oxylus {
class Material;

struct LightingData {
  Vec4 position_intensity = {};
  Vec4 color_radius = {};
  Vec4 rotation_type = {};
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
