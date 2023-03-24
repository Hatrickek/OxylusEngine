#include "src/oxpch.h"
#include "Thread.h"

namespace Oxylus {
  Thread::Thread() {
    m_Worker = std::thread(&Thread::QueueLoop, this);
  }

  Thread::~Thread() {
    if (m_Worker.joinable()) {
      Wait();
      m_QueueMutex.lock();
      m_Destroying = true;
      m_Condition.notify_one();
      m_QueueMutex.unlock();
      m_Worker.join();
    }
  }

  void Thread::AddJob(std::function<void()> function) {
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    m_JobQueue.push(std::move(function));
    m_Condition.notify_one();
  }

  void Thread::Wait() {
    std::unique_lock<std::mutex> lock(m_QueueMutex);
    m_Condition.wait(lock,
      [this]() {
        return m_JobQueue.empty();
      });
  }

  void Thread::QueueLoop() {
    while (true) {
      std::function<void()> job;
      {
        std::unique_lock<std::mutex> lock(m_QueueMutex);
        m_Condition.wait(lock,
          [this] {
            return !m_JobQueue.empty() || m_Destroying;
          });
        if (m_Destroying) {
          break;
        }
        job = m_JobQueue.front();
      }

      job();

      {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        m_JobQueue.pop();
        m_Condition.notify_one();
      }
    }
  }
}
