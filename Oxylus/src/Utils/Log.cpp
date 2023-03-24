#include "oxpch.h"
#include "Log.h"
#include <UI/ExternalConsoleSink.h>
#include "Utils/Profiler.h"

namespace Oxylus {
  std::shared_ptr<spdlog::logger> Log::s_CoreLogger;

  void Log::Init() {
    std::vector<spdlog::sink_ptr> logSinks;
    logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    logSinks.emplace_back(std::make_shared<ExternalConsoleSink>(true));
    logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/oxylus_log.txt", true));

    logSinks[0]->set_pattern("%^[%T] %n: %v%$");
    logSinks[1]->set_pattern("%^[%T] %n: %v%$");
    logSinks[2]->set_pattern("[%T] [%l] %n: %v");

    s_CoreLogger = std::make_shared<spdlog::logger>("Oxylus", logSinks.begin(), logSinks.end());
    s_CoreLogger->set_level(spdlog::level::trace);
    spdlog::register_logger(s_CoreLogger);

    spdlog::set_error_handler([](const std::string& msg) {
      OX_CORE_ERROR(msg);
    });
  }
}
