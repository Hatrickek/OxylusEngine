#pragma once

#include "Application.h"
#include "Utils/Log.h"
#include "Utils/Profiler.h"

int main(int argc, char** argv) {
  Oxylus::Log::Init();

  const auto app = Oxylus::create_application({argc, argv});

  app->init_systems();
  app->run();

  delete app;
}
