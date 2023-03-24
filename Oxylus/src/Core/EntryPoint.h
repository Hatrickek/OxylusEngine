#pragma once
#include "Application.h"
#include "Utils/Log.h"
#include "Utils/Profiler.h"

int main(int argc, char** argv) {
  Oxylus::Log::Init();

  const auto app = Oxylus::CreateApplication({argc, argv});

  app->InitSystems();
  app->Run();

  delete app;
}
