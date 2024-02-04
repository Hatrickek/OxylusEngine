#include "TaskScheduler.h"

#include <TaskScheduler.h>

#include "Utils/Log.h"
#include "Utils/Profiler.h"

namespace Ox {
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

void TaskScheduler::wait_for_all() {
  instance->task_scheduler->WaitforAll();
}
}
