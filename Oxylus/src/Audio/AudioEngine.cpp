#include "AudioEngine.h"

#define MINIAUDIO_IMPLEMENTATION

#include <miniaudio.h>

#include "Utils/Log.h"
#include "Utils/Profiler.h"

namespace Oxylus {
ma_engine* AudioEngine::s_engine;

void AudioEngine::init() {
  OX_SCOPED_ZONE;
  ma_engine_config config = ma_engine_config_init();
  config.listenerCount = 1;

  s_engine = new ma_engine();
  const ma_result result = ma_engine_init(&config, s_engine);
  OX_CORE_ASSERT(result == MA_SUCCESS, "Failed to initialize audio engine!")

  OX_CORE_INFO("Initalized audio engine.");
}

void AudioEngine::shutdown() {
  ma_engine_uninit(s_engine);
  delete s_engine;
}

AudioEngineInternal AudioEngine::get_engine() {
  return s_engine;
}
}
