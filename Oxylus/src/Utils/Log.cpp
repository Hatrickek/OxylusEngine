#include "Log.h"

namespace Oxylus {
std::vector<std::shared_ptr<ExternalSink>> Log::external_sinks = {};

void Log::init() {
  fmtlog::setLogCB(logcb, fmtlog::DBG);
  fmtlog::setLogFile("logs/oxylus_log.txt", true);
  fmtlog::setHeaderPattern("{HMS} | {l} | {s:<16} | ");
  fmtlog::flushOn(fmtlog::DBG);
  fmtlog::setThreadName("MAIN");
  fmtlog::startPollingThread(1);
}

void Log::logcb(int64_t ns,
                fmtlog::LogLevel level,
                fmt::string_view location,
                size_t base_pos,
                fmt::string_view thread_name,
                fmt::string_view msg,
                size_t body_pos,
                size_t log_file_pos) {
  fmt::print("{}\n", msg);

  for (const auto& interface : external_sinks)
    interface->log(ns, level, location, base_pos, thread_name, msg, body_pos, log_file_pos);
}
}
