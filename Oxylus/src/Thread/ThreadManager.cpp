#include "src/oxpch.h"
#include "ThreadManager.h"

namespace Oxylus {
  ThreadManager* ThreadManager::s_Instance = nullptr;

  ThreadManager::ThreadManager() {
    s_Instance = this;
  }

  void ThreadManager::WaitAllThreads() {
    AssetThread.Wait();
    AudioThread.Wait();
    RenderThread.Wait();
  }
}
