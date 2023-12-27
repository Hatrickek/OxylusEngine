#include "TaskScheduler.h"

#include <TaskScheduler.h>

#include "Utils/Log.h"
#include "Utils/Profiler.h"

namespace Oxylus {
TaskScheduler* TaskScheduler::instance = nullptr;

void TaskScheduler::init() {
  OX_SCOPED_ZONE;
  instance = new TaskScheduler();
  instance->task_scheduler = new enki::TaskScheduler();
  instance->task_scheduler->Initialize();

  OX_CORE_INFO("TaskScheduler initalized.");
}

void TaskScheduler::shutdown() {
  instance->task_scheduler->ShutdownNow();
  delete instance->task_scheduler;
  delete instance;
  instance = nullptr;
}

void TaskScheduler::add_task_to_pipe(const std::function<void()>& func) {
  auto task = enki::TaskSet(
    [func](enki::TaskSetPartition, uint32_t) {
      func();
    });
  instance->task_scheduler->AddTaskSetToPipe(&task);
}
}
