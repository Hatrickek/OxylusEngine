#pragma once
#include <utility>

#include <vuk/Future.hpp>

namespace Oxylus {
class RendererCommon {
public:
  static void apply_blur(const std::shared_ptr<vuk::RenderGraph>& render_graph, vuk::Name src_attachment, vuk::Name attachment_name, vuk::Name attachment_name_output);
  static std::pair<vuk::Unique<vuk::Image>, vuk::Future> generate_cubemap_from_equirectangular(const vuk::Texture& cubemap);
};
}
