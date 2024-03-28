#pragma once
#include <vuk/Image.hpp>

#include "Core/Base.hpp"
#include "Core/Types.hpp"

namespace vuk {
struct PipelineBaseCreateInfo;
class Context;
} // namespace vuk
namespace ox {
class Camera;
class FSR {
public:
  FSR() = default;
  ~FSR() = default;

  std::unique_ptr<char[]> scratch_memory = nullptr;

  Vec2 get_jitter(uint32_t frameIndex, uint32_t renderWidth, uint32_t renderHeight, uint32_t windowWidth) const;

  void create_fs2_resources(UVec2 render_resolution, UVec2 presentation_resolution);
  void load_pipelines(vuk::Allocator& allocator, vuk::PipelineBaseCreateInfo& pipeline_ci);
  void dispatch(Camera& camera,
                float dt,
                float sharpness);
};
} // namespace ox
