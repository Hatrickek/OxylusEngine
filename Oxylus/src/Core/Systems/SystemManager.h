#pragma once
#include "Core/Systems/System.h"
#include "Utils/Log.h"
#include "Core/Base.h"

namespace Oxylus {
  class SystemManager {
  public:
    template <typename T, typename... Args>
    Ref<T> AddSystem(Args&&... args) {
      auto typeName = typeid(T).hash_code();

      OX_CORE_ASSERT(m_Systems.contains(typeName), "Registering system more than once.");

      Ref<T> system = CreateSharedPtr<T>(std::forward<Args>(args)...);
      m_Systems.insert({typeName, std::move(system)});
      return system;
    }

    template <typename T>
    Ref<T> AddSystem(T* t) {
      auto typeName = typeid(T).hash_code();

      OX_CORE_ASSERT(m_Systems.contains(typeName), "Registering system more than once.");

      Ref<T> system = CreateRef<T>(t);
      m_Systems.insert({typeName, std::move(system)});
      return system;
    }

    template <typename T>
    void RemoveSystem() {
      const auto typeName = typeid(T).hash_code();

      if (m_Systems.contains(typeName)) {
        m_Systems.erase(typeName);
      }
    }

    template <typename T>
    T* GetSystem() {
      const auto typeName = typeid(T).hash_code();

      if (m_Systems.contains(typeName)) {
        return dynamic_cast<T*>(m_Systems[typeName].get());
      }

      return nullptr;
    }

    template <typename T>
    T* HasSystem() {
      const auto typeName = typeid(T).hash_code();

      return m_Systems.contains(typeName);
    }

    std::unordered_map<size_t, Ref<System>>& GetSystems() { return m_Systems; }

    void OnUpdate(Scene* scene) const {
      for (auto& system : m_Systems)
        system.second->OnUpdate(scene);
    }

    void OnUpdate() const {
      for (auto& system : m_Systems)
        system.second->OnUpdate();
    }

    void OnImGuiRender() const {
      for (const auto& system : m_Systems)
        system.second->OnImGuiRender();
    }

    void OnDebugDraw() const {
      for (const auto& system : m_Systems)
        system.second->OnDebugRender();
    }

    void Shutdown() const {
      for (const auto& system : m_Systems)
        system.second->OnShutdown();
    }

  private:
    std::unordered_map<size_t, Ref<System>> m_Systems = {};
  };
}
