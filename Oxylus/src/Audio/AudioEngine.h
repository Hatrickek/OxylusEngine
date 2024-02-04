#pragma once
struct ma_engine;

namespace Ox {
using AudioEngineInternal = void*;

class AudioEngine {
public:
  static void init();
  static void shutdown();

  static AudioEngineInternal get_engine();

private:
  static ma_engine* s_engine;
};
}
