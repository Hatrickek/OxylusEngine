#pragma once
#include <vuk/CommandBuffer.hpp>

#include "Core/Types.hpp"

namespace ox {
struct Vertex {
  Vec3 position;
  int _pad;
  Vec3 normal;
  int _pad2;
  Vec2 uv;
  Vec2 _pad3;
  Vec4 tangent;
  Vec4 color;
  Vec4 joint0;
  Vec4 weight0;
};

inline auto vertex_pack = vuk::Packed{
  vuk::Format::eR32G32B32A32Sfloat, // 12 postition
  vuk::Format::eR32G32B32A32Sfloat, // 12 normal
  vuk::Format::eR32G32B32A32Sfloat, // 8  uv
  vuk::Format::eR32G32B32A32Sfloat, // 16 tangent
  vuk::Format::eR32G32B32A32Sfloat, // 16 color
  vuk::Format::eR32G32B32A32Sfloat, // 16 joint
  vuk::Format::eR32G32B32A32Sfloat, // 16 weight
};
}
