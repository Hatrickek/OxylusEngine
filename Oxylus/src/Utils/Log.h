#pragma once

#include <spdlog/sinks/basic_file_sink.h>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <filesystem>

#include "Core/PlatformDetection.h"

namespace Oxylus {
  class Log {
  public:
    static void Init();

    static std::shared_ptr<spdlog::logger>& GetCoreLogger() {
      return s_CoreLogger;
    }

  private:
    static std::shared_ptr<spdlog::logger> s_CoreLogger;
  };
}

#define OX_ENABLE_ASSERTS

#ifdef OX_DISTRIBUTION
#define OX_DISABLE_DEBUG_BREAKS
#endif

//#define OX_DISABLE_DEBUG_BREAKS

#ifndef OX_DISABLE_DEBUG_BREAKS
#if defined(OX_PLATFORM_WINDOWS)
#define OX_DEBUGBREAK() __debugbreak();
#elif defined(OX_PLATFORM_LINUX)
#include <signal.h>
#define OX_DEBUGBREAK() raise(SIGTRAP)
#elif defined(OX_PLATFORM_MACOS)
#define OX_DEBUGBREAK()
#else
#define OX_DEBUGBREAK()
#endif
#else
#define OX_DEBUGBREAK()
#endif

// log macros
#define OX_CORE_TRACE(...) ::Oxylus::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define OX_CORE_INFO(...) ::Oxylus::Log::GetCoreLogger()->info(__VA_ARGS__)
#define OX_CORE_WARN(...) ::Oxylus::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define OX_CORE_ERROR(...) ::Oxylus::Log::GetCoreLogger()->error(__VA_ARGS__)
#define OX_CORE_BERROR(...) ::Oxylus::Log::GetCoreLogger()->error(__VA_ARGS__); OX_DEBUGBREAK()
#define OX_CORE_FATAL(...) ::Oxylus::Log::GetCoreLogger()->critical(__VA_ARGS__)
#define OX_CORE_BFATAL(...) ::Oxylus::Log::GetCoreLogger()->critical(__VA_ARGS__); OX_DEBUGBREAK()

#define OX_EXPAND_MACRO(x) x
#define OX_STRINGIFY_MACRO(x) #x

#ifdef OX_ENABLE_ASSERTS

// Alteratively we could use the same "default" message for both "WITH_MSG" and "NO_MSG" and
// provide support for custom formatting by concatenating the formatting string instead of having the format inside the default message
#define OX_INTERNAL_ASSERT_IMPL(type, check, msg, ...) { if(!(check)) { OX##type##ERROR(msg, __VA_ARGS__); OX_DEBUGBREAK(); } }
#define OX_INTERNAL_ASSERT_WITH_MSG(type, check, ...) OX_INTERNAL_ASSERT_IMPL(type, check, "Assertion failed: {0}", __VA_ARGS__)
#define OX_INTERNAL_ASSERT_NO_MSG(type, check) OX_INTERNAL_ASSERT_IMPL(type, check, "Assertion '{0}' failed at {1}:{2}", OX_STRINGIFY_MACRO(check), std::filesystem::path(__FILE__).filename().string(), __LINE__)

#define OX_INTERNAL_ASSERT_GET_MACRO_NAME(arg1, arg2, macro, ...) macro
#define OX_INTERNAL_ASSERT_GET_MACRO(...) OX_EXPAND_MACRO( OX_INTERNAL_ASSERT_GET_MACRO_NAME(__VA_ARGS__, OX_INTERNAL_ASSERT_WITH_MSG, OX_INTERNAL_ASSERT_NO_MSG) )

// Currently accepts at least the condition and one additional parameter (the message) being optional
#define OX_ASSERT(...) OX_EXPAND_MACRO( OX_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_, __VA_ARGS__) )
#define OX_CORE_ASSERT(...) OX_EXPAND_MACRO( OX_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_CORE_, __VA_ARGS__) )
#else
#define OX_ASSERT(...)
#define OX_CORE_ASSERT(...)
#endif
