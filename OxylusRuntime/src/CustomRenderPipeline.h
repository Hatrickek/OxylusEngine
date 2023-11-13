#pragma once
#include "Render/RenderPipeline.h"

namespace Oxylus {
class CustomRenderPipeline : public RenderPipeline {
public:
  explicit CustomRenderPipeline(const std::string& name = "CustomRenderPipeline")
    : RenderPipeline(name) { }

  std::vector<MeshData> draw_list = {};
  Camera* current_camera = nullptr;

  void init(vuk::Allocator& allocator) override;
  void shutdown() override;
  Scope<vuk::Future> on_render(vuk::Allocator& frame_allocator, const vuk::Future& target, vuk::Dimension3D dim) override;
  void on_register_render_object(const MeshData& render_object) override;
  void on_register_camera(Camera* camera) override;
};
}
