#pragma once

#include "Core/PlatformDetection.h"

#ifdef OX_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
// See github.com/skypjack/entt/wiki/Frequently-Asked-Questions#warning-c4003-the-min-the-max-and-the-macro
#define NOMINMAX
#endif
#endif

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <utility>

#include <array>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Core/Base.h"
#include "Core/Memory.h"
#include "Core/Types.h"
#include "Utils/Log.h"

#include <glm/glm.hpp>
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <Jolt/Jolt.h>

#ifdef OX_PLATFORM_WINDOWS
#include <windows.h>
#endif