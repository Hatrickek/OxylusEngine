#include "src/oxpch.h"
#include "ExternalConsoleSink.h"

namespace Oxylus {
  std::function<void(std::string_view, const char*, const char*, int32_t, spdlog::level::level_enum)>
          ExternalConsoleSink::OnFlush;
}
