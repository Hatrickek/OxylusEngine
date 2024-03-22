#pragma once
#include <TaskScheduler.h>

#include "Core/ESystem.h"

namespace ox {
class TaskScheduler : public ESystem {
public:
  TaskScheduler() = default;

  void init() override;
  void deinit() override;

  Unique<enki::TaskScheduler>& get() { return task_scheduler; }

  template <typename func> void add_task(func function) {
    const enki::TaskSetFunction f = [function](enki::TaskSetPartition, uint32_t) mutable { function(); };
    task_scheduler->AddTaskSetToPipe(task_sets.emplace_back(create_unique<enki::TaskSet>(f)).get());
  }

  void wait_for_all();

private:
  Unique<enki::TaskScheduler> task_scheduler;
  std::vector<Unique<enki::TaskSet>> task_sets = {};
};
} // namespace ox
