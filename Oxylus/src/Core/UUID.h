#pragma once

#include <cstdint>
#include <unordered_map>

namespace Ox {
class UUID {
public:
  UUID();

  UUID(uint64_t uuid);

  UUID(const UUID&) = default;

  operator uint64_t() const {
    return m_UUID;
  }

private:
  uint64_t m_UUID;
};
}

template <>
struct std::hash<Ox::UUID> {
  size_t operator()(const Ox::UUID& uuid) const noexcept {
    return uuid;
  }
};
