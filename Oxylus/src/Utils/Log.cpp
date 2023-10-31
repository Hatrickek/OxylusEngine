#include "Log.h"
#include <UI/ExternalConsoleSink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

namespace Oxylus {
std::shared_ptr<spdlog::logger> Log::s_core_logger;

void Log::init() {
  std::vector<spdlog::sink_ptr> log_sinks;
  log_sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>())
         ->set_pattern("%^[%T] [%l]%$ %n: %v");
  log_sinks.emplace_back(std::make_shared<ExternalConsoleSink>(true))
         ->set_pattern("%^[%T] [%l]%$ %n: %v");
  log_sinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_st>("logs/oxylus_log.txt", true))
         ->set_pattern("[%T] [%l] %n: %v");

  s_core_logger = std::make_shared<spdlog::logger>("Oxylus", log_sinks.begin(), log_sinks.end());
  s_core_logger->set_level(spdlog::level::trace);
  spdlog::register_logger(s_core_logger);

  spdlog::set_error_handler([](const std::string& msg) {
    OX_CORE_ERROR(msg);
  });
}
}
