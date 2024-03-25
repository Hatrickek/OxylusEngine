#pragma once

#include <dylib.hpp>
#include <string_view>

#include <ankerl/unordered_dense.h>

#include "Core/Base.hpp"
#include "Core/ESystem.hpp"

namespace ox {
class ModuleInterface;

struct Module {
  Unique<dylib> lib;
  ModuleInterface* interface;
};

class ModuleRegistry : public ESystem {
public:
  void init() override;
  void deinit() override;

  Module* add_lib(const std::string& name, std::string_view path);
  Module* get_lib(const std::string& name);
  void clear();

private:
  ankerl::unordered_dense::map<std::string, Unique<Module>> libs = {};
};
}
