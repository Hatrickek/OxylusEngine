#pragma once

#include "App.h"
#include "Utils/Log.h"

int main(int argc, char** argv) {
  ox::Log::init();

  const auto app = ox::create_application({argc, argv});

  app->run();

  delete app;

  return 0;
}
