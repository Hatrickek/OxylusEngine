#pragma once
#include <vuk/Future.hpp>
#include <vuk/Image.hpp>

#include "Assets/TextureAsset.h"

namespace Oxylus {
class Mesh;

class Prefilter {
public:
  static std::pair<vuk::Texture, vuk::Future> generate_brdflut();
  static std::pair<vuk::Texture, vuk::Future> generate_irradiance_cube(const Ref<Mesh>& skybox, const Ref<TextureAsset>& cubemap);
  static std::pair<vuk::Texture, vuk::Future> generate_prefiltered_cube(const Ref<Mesh>& skybox, const Ref<TextureAsset>& cubemap);
};
}
