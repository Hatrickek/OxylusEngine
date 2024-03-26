#pragma once
#include <vuk/Future.hpp>
#include <vuk/Image.hpp>

#include "Assets/TextureAsset.hpp"

namespace ox {
class Mesh;

class Prefilter {
public:
  static std::pair<vuk::Texture, vuk::Future> generate_brdflut();
  static std::pair<vuk::Texture, vuk::Future> generate_irradiance_cube(const Shared<Mesh>& skybox, const Shared<TextureAsset>& cubemap);
  static std::pair<vuk::Texture, vuk::Future> generate_prefiltered_cube(const Shared<Mesh>& skybox, const Shared<TextureAsset>& cubemap);
};
}
