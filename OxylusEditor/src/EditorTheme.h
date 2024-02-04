﻿#pragma once
#include <unordered_map>

namespace Ox {
class EditorTheme {
public:
  static std::unordered_map<size_t, const char8_t*> component_icon_map;

  static void init();
};
}
