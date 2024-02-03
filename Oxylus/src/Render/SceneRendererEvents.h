#pragma once
#include "Core/Base.h"
#include "Scene/Components.h"

namespace Oxylus {
struct SkyboxLoadEvent {
  Shared<TextureAsset> cube_map = nullptr;
};

struct ProbeChangeEvent {
  PostProcessProbe probe;
};
}
