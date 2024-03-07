#pragma once
#include "Core/Base.h"
#include "Scene/Components.h"

namespace ox {
struct SkyboxLoadEvent {
  Shared<TextureAsset> cube_map = nullptr;
};
}
