#pragma once
#include <TaskScheduler.h>

#define COMBINE(X,Y) X##Y
#define VAR_NAME(X,Y) COMBINE(X,Y)

#define ADD_TASK_TO_PIPE(closure, func) \
  auto VAR_NAME(task, __LINE__) = enki::TaskSet( \
    [closure](enki::TaskSetPartition, uint32_t) mutable { \
      func\
    }); \
  TaskScheduler::get()->AddTaskSetToPipe(&VAR_NAME(task, __LINE__))

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
