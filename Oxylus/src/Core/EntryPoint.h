#pragma once

#include "App.h"
#include "Utils/Log.h"

int main(int argc, char** argv) {
  Ox::Log::init();

  const auto app = Ox::create_application({argc, argv});

  app->run();

  delete app;

  return 0;
}
