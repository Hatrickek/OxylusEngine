#pragma once
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace FlatEngine{
	class Log {
	public:
		static void Init();
        
		static std::shared_ptr<spdlog::logger>& GetCoreLogger(){ return s_CoreLogger;}
	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
	};
}
//log macros
#define FE_LOG_TRACE(...) ::FlatEngine::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define FE_LOG_INFO(...) ::FlatEngine::Log::GetCoreLogger()->info(__VA_ARGS__)
#define FE_LOG_WARN(...) ::FlatEngine::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define FE_LOG_ERROR(...) ::FlatEngine::Log::GetCoreLogger()->error(__VA_ARGS__)
#define FE_LOG_FATAL(...) ::FlatEngine::Log::GetCoreLogger()->fatal(__VA_ARGS__)
