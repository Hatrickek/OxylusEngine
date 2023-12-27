#pragma once
namespace enki {
class TaskScheduler;
}

namespace Oxylus {
class TaskScheduler {
public:
  TaskScheduler() = default;
  ~TaskScheduler() = default;

  static void init();
  static void shutdown();

  static enki::TaskScheduler* get() { return instance->task_scheduler; }

private:
  static TaskScheduler* instance;

  enki::TaskScheduler* task_scheduler;
};
}
