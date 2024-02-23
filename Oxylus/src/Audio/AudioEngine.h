#pragma once
#include "Core/ESystem.h"
struct ma_engine;

namespace Ox {
class AudioEngine : public ESystem {
public:
  void init() override;
  void deinit() override;

  ma_engine* get_engine() const;

private:
  ma_engine* engine = nullptr;
};
}
