#pragma once

#include "Application.h"
#include "Utils/Log.h"

int main(int argc, char** argv) {
  Oxylus::Log::init();

  const auto app = Oxylus::create_application({argc, argv});

  app->init_systems();
  app->run();

  delete app;
}
