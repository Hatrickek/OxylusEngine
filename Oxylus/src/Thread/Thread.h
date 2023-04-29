#pragma once

#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace Oxylus {
  class Thread {
  public:
    Thread();
    ~Thread();

    void QueueJob(std::function<void()> function);
    void Wait();
    uint32_t GetQueueSize() const { return (uint32_t)m_JobQueue.size(); }

  private:
    void QueueLoop();

    bool m_Destroying = false;
    std::thread m_Worker;
    std::queue<std::function<void()>> m_JobQueue;
    std::mutex m_QueueMutex;
    std::condition_variable m_Condition;
  };
}
