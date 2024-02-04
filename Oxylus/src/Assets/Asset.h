#pragma once
#include <cstdint>

namespace Ox {
class AssetManager;

static constexpr uint32_t INVALID_ASSET_ID = UINT32_MAX;

class Asset {
public:
  Asset() = default;
  explicit Asset(const uint32_t id) : asset_id(id) {}

  uint32_t get_id() const { return asset_id; }
  bool is_valid_id() const { return asset_id != INVALID_ASSET_ID; }

private:
  uint32_t asset_id = UINT32_MAX;

  friend AssetManager;
};
}
