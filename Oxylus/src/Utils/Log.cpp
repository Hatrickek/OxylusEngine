#include "Log.h"

#include <filesystem>

#include <fmt/color.h>

#include "Profiler.h"

namespace Ox {
std::vector<std::shared_ptr<ExternalSink>> Log::external_sinks = {};

void Log::init() {
  OX_SCOPED_ZONE;
  if (!std::filesystem::exists("logs"))
    std::filesystem::create_directory("logs");
  fmtlog::setLogLevel(fmtlog::DBG);
  fmtlog::setLogCB(logcb, fmtlog::DBG);
  fmtlog::setLogFile("logs/oxylus_log.txt", true);
  fmtlog::setHeaderPattern("{HMS} | {l} | {s:<16} | ");
  fmtlog::flushOn(fmtlog::DBG);
  fmtlog::setThreadName("MAIN");
  fmtlog::startPollingThread(1);
}

void Log::force_poll() {
  fmtlog::poll(true);
}

void Log::logcb(int64_t ns,
                fmtlog::LogLevel level,
                fmt::string_view location,
                size_t base_pos,
                fmt::string_view thread_name,
                fmt::string_view msg,
                size_t body_pos,
                size_t log_file_pos) {
  auto color = fg(fmt::color::white);

  switch (level) {
    case fmtlog::LogLevel::DBG: {
      color = fg(fmt::color::white);
      break;
    }
    case fmtlog::LogLevel::INF: {
      color = fg(fmt::color::lime_green);
      break;
    }
    case fmtlog::LogLevel::WRN: {
      color = fg(fmt::color::orange);
      break;
    }
    case fmtlog::LogLevel::ERR: {
      color = fg(fmt::color::red);
      break;
    }
    case fmtlog::LogLevel::OFF: break;
  }

  fmt::print(color, "{}\n", msg);

  for (uint32_t i = 0; i < (uint32_t)external_sinks.size(); i++) {
    if (external_sinks[i]->user_data)
      external_sinks[i]->log(ns, level, location, base_pos, thread_name, msg, body_pos, log_file_pos);
    else
      external_sinks.erase(external_sinks.begin() + i);
  }
}
}
