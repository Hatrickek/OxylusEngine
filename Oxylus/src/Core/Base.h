#pragma once
#define OX_ARRAYSIZE(_ARR) ((int)(sizeof(_ARR) / sizeof(*(_ARR))))
#define BIT(x) (1 << x)

#define OX_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }
#define DELETE_DEFAULT_CONSTRUCTORS(struct) struct(const struct& other) = delete;\
                                            struct(struct&& other) = delete;\
                                            struct& operator=(const struct& other) = delete;\
                                            struct& operator=(struct&& other) = delete;

#include <memory>

namespace Oxylus {
template <typename T> using Shared = std::shared_ptr<T>;

template <typename T, typename... Args>
constexpr Shared<T> create_shared(Args&&... args) {
  return std::make_shared<T>(std::forward<Args>(args)...);
}

template <typename T> using Unique = std::unique_ptr<T>;

template <typename T, typename... Args>
constexpr Unique<T> create_unique(Args&&... args) {
  return std::make_unique<T>(std::forward<Args>(args)...);
}
}
