#pragma once

#include <fmtlog.h>
#include <memory>

#include "Core/PlatformDetection.h"

#include "Utils/FileSystem.h"

namespace Oxylus {
class ExternalSink {
public:
  void* user_data = nullptr;

  ExternalSink(void* user_data) { this->user_data = user_data; }
  virtual ~ExternalSink() = default;
  ExternalSink(const ExternalSink&) = delete;

  ExternalSink& operator=(const ExternalSink&) = delete;

  virtual void log(int64_t ns,
                   fmtlog::LogLevel level,
                   fmt::string_view location,
                   size_t base_pos,
                   fmt::string_view thread_name,
                   fmt::string_view msg,
                   size_t body_pos,
                   size_t log_file_pos) = 0;

};

class Log {
public:
  static void init();
  static void force_poll();

  template<typename T>
  static void register_sink(void* user_data) {
    external_sinks.emplace_back(std::make_shared<T>(user_data));
  }

private:
  static void logcb(int64_t ns,
                    fmtlog::LogLevel level,
                    fmt::string_view location,
                    size_t base_pos,
                    fmt::string_view thread_name,
                    fmt::string_view msg,
                    size_t body_pos,
                    size_t log_file_pos);

  static std::vector<std::shared_ptr<ExternalSink>> external_sinks;
};
}

// log macros
#define OX_CORE_TRACE(...) FMTLOG(fmtlog::DBG, __VA_ARGS__)
#define OX_CORE_INFO(...) FMTLOG(fmtlog::INF, __VA_ARGS__)
#define OX_CORE_WARN(...) FMTLOG(fmtlog::WRN, __VA_ARGS__)
#define OX_CORE_ERROR(...) FMTLOG(fmtlog::ERR, __VA_ARGS__)

#ifndef OX_DEBUG
#define OX_DISABLE_DEBUG_BREAKS
#endif

#ifdef OX_DISTRIBUTION
#define OX_DISABLE_DEBUG_BREAKS
#endif

#ifndef OX_DISABLE_DEBUG_BREAKS
#if defined(OX_PLATFORM_WINDOWS)
#define OX_DEBUGBREAK() __debugbreak()
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

#define OX_ENABLE_ASSERTS

#define OX_EXPAND_MACRO(x) x
#define OX_STRINGIFY_MACRO(x) #x

#ifdef OX_ENABLE_ASSERTS

// Alteratively we could use the same "default" message for both "WITH_MSG" and "NO_MSG" and
// provide support for custom formatting by concatenating the formatting string instead of having the format inside the default message
#define OX_INTERNAL_ASSERT_IMPL(type, check, msg, ...) { if(!(check)) { OX##type##ERROR(msg, __VA_ARGS__); OX_DEBUGBREAK(); } }
#define OX_INTERNAL_ASSERT_WITH_MSG(type, check, ...) OX_INTERNAL_ASSERT_IMPL(type, check, "Assertion failed: {0}", __VA_ARGS__)
#define OX_INTERNAL_ASSERT_NO_MSG(type, check) OX_INTERNAL_ASSERT_IMPL(type, check, "Assertion '{0}' failed at {1}:{2}", OX_STRINGIFY_MACRO(check), ::Oxylus::FileSystem::get_file_name(__FILE__), __LINE__)

#define OX_INTERNAL_ASSERT_GET_MACRO_NAME(arg1, arg2, macro, ...) macro
#define OX_INTERNAL_ASSERT_GET_MACRO(...) OX_EXPAND_MACRO( OX_INTERNAL_ASSERT_GET_MACRO_NAME(__VA_ARGS__, OX_INTERNAL_ASSERT_WITH_MSG, OX_INTERNAL_ASSERT_NO_MSG) )

// Currently accepts at least the condition and one additional parameter (the message) being optional
#define OX_ASSERT(...) OX_EXPAND_MACRO( OX_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_, __VA_ARGS__) )
#define OX_CORE_ASSERT(...) OX_EXPAND_MACRO( OX_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_CORE_, __VA_ARGS__) )
#else
#define OX_ASSERT(...)
#define OX_CORE_ASSERT(...)
#endif
