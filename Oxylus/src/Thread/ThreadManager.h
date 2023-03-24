#pragma once

#include "Thread.h"

namespace Oxylus {
  class ThreadManager {
  public:
    Thread AssetThread;
    Thread AudioThread;

    Thread RenderCommandsThread;
    Thread RenderThread;

    ThreadManager();
    ~ThreadManager() = default;

    void WaitAllThreads();

    static ThreadManager* Get() { return s_Instance; }

  private:
    static ThreadManager* s_Instance;
  };
}
