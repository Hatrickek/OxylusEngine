#pragma once
#include <string_view>
#include <vector>

#include "Core/Base.h"

namespace sol {
class state;
}

namespace Oxylus {
class LuaManager {
public:
  ~LuaManager() = default;

  void init();

  static LuaManager* get();
  sol::state* get_state() const { return m_state.get(); }

private:
  Ref<sol::state> m_state = nullptr;

  void bind_log() const;
  void bind_ecs();
  void bind_math();
};
}
