#include "ModuleRegistry.hpp"

#include "ModuleInterface.hpp"

#include "Utils/Log.hpp"

namespace ox {
void ModuleRegistry::init() {}

void ModuleRegistry::deinit() {
  clear();
}

Module* ModuleRegistry::add_lib(const std::string& name, std::string_view path) {
  try {
    auto lib = create_unique<dylib>(path);

    const auto create_func = lib->get_function<ModuleInterface*()>("create_module");
    ModuleInterface* interface = create_func();

    auto module = create_unique<Module>(std::move(lib), interface);
    libs.emplace(name, std::move(module));

    OX_LOG_INFO("Successfully loaded module: {}", name);

    return libs[name].get();
  }
  catch (const std::exception& exc) {
    OX_LOG_ERROR("{}", exc.what());
    return nullptr;
  }
}

Module* ModuleRegistry::get_lib(const std::string& name) {
  try {
    return libs.at(name).get();
  }
  catch ([[maybe_unused]] const std::exception& exc) {
    OX_LOG_ERROR("Module {} doesn't exists or has not been registered.", name);
    return nullptr;
  }
}

void ModuleRegistry::clear() {
  for (auto&& [name, module] : libs) {
    delete module->interface;
    module->lib.reset();
  }
  libs.clear();
}
}
