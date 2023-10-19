#pragma once
#include "Core/Base.h"
#include "Core/Components.h"

namespace Oxylus {
struct SkyboxLoadEvent {
  Ref<TextureAsset> cube_map = nullptr;
};

struct ProbeChangeEvent {
  PostProcessProbe probe;
};
}
