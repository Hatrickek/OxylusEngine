#pragma once
#include <utility>

#include <vuk/Future.hpp>

#include "Core/Base.hpp"

namespace ox {
class Mesh;

class RendererCommon {
public:
  static void apply_blur(const std::shared_ptr<vuk::RenderGraph>& render_graph, vuk::Name src_attachment, vuk::Name attachment_name, vuk::Name attachment_name_output);
  static std::pair<vuk::Unique<vuk::Image>, vuk::Future> generate_cubemap_from_equirectangular(const vuk::Texture& cubemap);

  static Shared<Mesh> generate_quad();
  static Shared<Mesh> generate_cube();
  static Shared<Mesh> generate_sphere();

private:
  static struct MeshLib {
    Shared<Mesh> quad = {};
    Shared<Mesh> cube = {};
    Shared<Mesh> sphere = {};
  } mesh_lib;
};
}
