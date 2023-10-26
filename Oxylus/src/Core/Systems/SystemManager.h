#pragma once
#include "Core/Systems/System.h"
#include "Utils/Log.h"
#include "Core/Base.h"

namespace Oxylus {
class SystemManager {
public:
  template <typename T, typename... Args>
  Ref<T> add_system(Args&&... args) {
    auto typeName = typeid(T).hash_code();

    OX_CORE_ASSERT(!m_Systems.contains(typeName), "Registering system more than once.");

    Ref<T> system = create_ref<T>(std::forward<Args>(args)...);
    m_Systems.insert({typeName, std::move(system)});
    return system;
  }

  template <typename T>
  Ref<T> add_system(T* t) {
    auto typeName = typeid(T).hash_code();

    OX_CORE_ASSERT(!m_Systems.contains(typeName), "Registering system more than once.");

    Ref<T> system = create_ref<T>(t);
    m_Systems.insert({typeName, std::move(system)});
    return system;
  }

  template <typename T>
  void remove_system() {
    const auto typeName = typeid(T).hash_code();

    if (m_Systems.contains(typeName)) {
      m_Systems.erase(typeName);
    }
  }

  template <typename T>
  T* get_system() {
    const auto typeName = typeid(T).hash_code();

    if (m_Systems.contains(typeName)) {
      return dynamic_cast<T*>(m_Systems[typeName].get());
    }

    return nullptr;
  }

  template <typename T>
  T* has_system() {
    const auto typeName = typeid(T).hash_code();

    return m_Systems.contains(typeName);
  }

  std::unordered_map<size_t, Ref<System>>& get_systems() { return m_Systems; }

  void on_update() const {
    for (auto& system : m_Systems)
      system.second->on_update();
  }

  void on_imgui_render() const {
    for (const auto& system : m_Systems)
      system.second->on_imgui_render();
  }

  void shutdown() const {
    for (const auto& system : m_Systems)
      system.second->on_shutdown();
  }

private:
  std::unordered_map<size_t, Ref<System>> m_Systems = {};
};
}
