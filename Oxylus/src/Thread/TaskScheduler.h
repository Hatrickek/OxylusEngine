#pragma once
#include <TaskScheduler.h>

#include "Core/ESystem.h"

#define COMBINE(X, Y) X##Y
#define VAR_NAME(X, Y) COMBINE(X, Y)

#define ADD_TASK_TO_PIPE(closure, ...)                                                                                \
  auto VAR_NAME(task, __LINE__) = enki::TaskSet([closure](enki::TaskSetPartition, uint32_t) mutable { __VA_ARGS__ }); \
  App::get_system<TaskScheduler>()->get()->AddTaskSetToPipe(&VAR_NAME(task, __LINE__))

namespace ox {
class TaskScheduler : public ESystem {
public:
  TaskScheduler() = default;
  ~TaskScheduler() = default;

  void init() override;
  void deinit() override;

  Unique<enki::TaskScheduler>& get() { return task_scheduler; }

  void wait_for_all();

private:
  Unique<enki::TaskScheduler> task_scheduler;
};
} // namespace ox
