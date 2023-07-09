#pragma once

#include "Core/UUID.h"
#include "Core/Base.h"

#include <string>

namespace Oxylus {
  enum class AssetType {
    None = 0,
    Mesh,
    Sound,
    Image,
    Material
  };

  using AssetHandle = UUID;

  template<typename T>
  struct Asset {
    Ref<T> Data = nullptr;
    AssetHandle Handle;
    std::string Path;
    AssetType Type{};

    explicit operator bool() const {
      return !Path.empty();
    }
  };
}
