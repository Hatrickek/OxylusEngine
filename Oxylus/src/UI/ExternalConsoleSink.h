#pragma once

#include <mutex>

#include <functional>
#include <string>
#include <spdlog/sinks/base_sink.h>

#include "Core/Base.h"

namespace Oxylus {
  class ExternalConsoleSink : public spdlog::sinks::base_sink<std::mutex> {
  public:
    struct Message {
      std::string Buffer;
      const char* CallerPath;
      const char* CallerFunction;
      int32_t CallerLine;
      spdlog::level::level_enum Level;

      Message(std::string_view message,
              const char* callerPath,
              const char* callerFunction,
              int32_t callerLine,
              spdlog::level::level_enum level) : Buffer(message.data(), message.size()), CallerPath(callerPath),
                                                 CallerFunction(callerFunction), CallerLine(callerLine),
                                                 Level(level) { }
    };

    explicit ExternalConsoleSink(bool forceFlush = false, uint8_t bufferCapacity = 10) :
            m_MessageBufferCapacity(forceFlush ? 1 : bufferCapacity),
            m_MessageBuffer(std::vector<Ref<Message>>(forceFlush ? 1 : bufferCapacity)) { }

    ExternalConsoleSink(const ExternalConsoleSink&) = delete;

    ExternalConsoleSink& operator=(const ExternalConsoleSink&) = delete;

    ~ExternalConsoleSink() override = default;

    static void SetConsoleSink_HandleFlush(const std::function<void(std::string_view,
                                                                    const char*,
                                                                    const char*,
                                                                    int32_t,
                                                                    spdlog::level::level_enum)>& func) {
      OnFlush = func;
    }

  protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
      if (OnFlush == nullptr) {
        flush_();
        return;
      }

      spdlog::memory_buf_t formatted;
      base_sink<std::mutex>::formatter_->format(msg, formatted);
      *(m_MessageBuffer.begin() + m_MessagesBuffered) = CreateRef<Message>(
              std::string_view(formatted.data(), formatted.size()),
              msg.source.filename,
              msg.source.funcname,
              msg.source.line,
              msg.level);

      if (++m_MessagesBuffered == m_MessageBufferCapacity)
        flush_();
    }

    void flush_() override {
      if (OnFlush == nullptr)
        return;

      for (const auto& msg : m_MessageBuffer)
        OnFlush(msg->Buffer, msg->CallerPath, msg->CallerFunction, msg->CallerLine, msg->Level);

      m_MessagesBuffered = 0;
    }

  private:
    uint8_t m_MessagesBuffered = 0;
    uint8_t m_MessageBufferCapacity;
    std::vector<Ref<Message>> m_MessageBuffer;

    static std::function<void(std::string_view, const char*, const char*, int32_t, spdlog::level::level_enum)> OnFlush;
  };
}
