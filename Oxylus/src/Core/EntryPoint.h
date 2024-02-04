#pragma once

#include "Application.h"
#include "Utils/Log.h"

int main(int argc, char** argv) {
  Ox::Log::init();

  const auto app = Ox::create_application({argc, argv});

  app->init_systems();
  app->run();

  delete app;
}
