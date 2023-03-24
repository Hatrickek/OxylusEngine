#include "src/oxpch.h"
#include "AudioEngine.h"

#define MINIAUDIO_IMPLEMENTATION

#include <miniaudio.h>

#include "Utils/Log.h"

namespace Oxylus {
  ma_engine* AudioEngine::s_Engine;

  void AudioEngine::Init() {
    ma_engine_config config = ma_engine_config_init();
    config.listenerCount = 1;

    s_Engine = new ma_engine();
    const ma_result result = ma_engine_init(&config, s_Engine);
    OX_CORE_ASSERT(result == MA_SUCCESS, "Failed to initialize audio engine!")

    OX_CORE_TRACE("Initalized audio engine.");
  }

  void AudioEngine::Shutdown() {
    ma_engine_uninit(s_Engine);
    delete s_Engine;
  }

  AudioEngineInternal AudioEngine::GetEngine() {
    return s_Engine;
  }
}
