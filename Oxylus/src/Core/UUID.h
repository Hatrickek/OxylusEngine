#pragma once

#include <cstdint>
#include <ankerl/unordered_dense.h>

namespace Ox {
class UUID {
public:
  UUID();

  UUID(uint64_t uuid);

  UUID(const UUID&) = default;

  operator uint64_t() const { return _uuid; }

private:
  uint64_t _uuid;
};
}

template <>
struct ankerl::unordered_dense::hash<Ox::UUID> {
  size_t operator()(const Ox::UUID& uuid) const noexcept {
    return uuid;
  }
};
