#pragma once
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
  Shared<sol::state> m_state = nullptr;

  void bind_log() const;
};
}
