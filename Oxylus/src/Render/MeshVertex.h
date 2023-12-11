#pragma once
#include <vuk/CommandBuffer.hpp>

#include "Core/Types.h"

namespace Oxylus {
struct Vertex {
  Vec3 position;
  Vec3 normal;
  Vec2 uv;
  Vec4 tangent;
  Vec4 color;
  Vec4 joint0;
  Vec4 weight0;
};

inline auto vertex_pack = vuk::Packed{
  vuk::Format::eR32G32B32Sfloat,    // 12 postition
  vuk::Format::eR32G32B32Sfloat,    // 12 normal
  vuk::Format::eR32G32Sfloat,       // 8  uv
  vuk::Format::eR32G32B32A32Sfloat, // 16 tangent
  vuk::Format::eR32G32B32A32Sfloat, // 16 color
  vuk::Format::eR32G32B32A32Sfloat, // 16 joint
  vuk::Format::eR32G32B32A32Sfloat, // 16 weight
};
}
