#pragma once
#include <vuk/Future.hpp>
#include <vuk/Image.hpp>

#include "Assets/TextureAsset.h"

namespace Oxylus {
class Mesh;

class Prefilter {
public:
  static std::pair<vuk::Unique<vuk::Image>, vuk::Future> GenerateBRDFLUT();
  static std::pair<vuk::Unique<vuk::Image>, vuk::Future> GenerateIrradianceCube(const Ref<Mesh>& skybox, const Ref<TextureAsset>& cubemap);
  static std::pair<vuk::Unique<vuk::Image>, vuk::Future> GeneratePrefilteredCube(const Ref<Mesh>& skybox, const Ref<TextureAsset>& cubemap);
};
}
