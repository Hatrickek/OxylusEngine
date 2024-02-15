#pragma once
#include "Core/Base.h"

#define OX_BUILD_DLL
#include "Core/Linker.h"

namespace sol {
class state;
}

namespace Ox {
/// Class for sharing meta components across applications with dynamic linking.
class OX_API LuaComponentInterface {
public:
  virtual ~LuaComponentInterface() = default;
  DELETE_DEFAULT_CONSTRUCTORS(LuaComponentInterface)

  virtual void bind_components(sol::state* state);
};
}
