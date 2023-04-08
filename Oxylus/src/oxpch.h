#pragma once

#include "Core/PlatformDetection.h"

#ifdef OX_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
// See github.com/skypjack/entt/wiki/Frequently-Asked-Questions#warning-c4003-the-min-the-max-and-the-macro
#define NOMINMAX
#endif
#endif

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>

#include <string>
#include <sstream>
#include <array>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

#include "Core/Memory.h"
#include "Core/Base.h"
#include "Utils/Log.h"
#include "Core/Types.h"

#ifdef OX_PLATFORM_WINDOWS

#include <windows.h>

#endif