#include <oxpch.h>
#include "Memory.h"

namespace Oxylus {
  uint64_t Memory::TotalAllocated = 0;
  uint64_t Memory::TotalFreed = 0;

  uint64_t GPUMemory::TotalAllocated = 0;
  uint64_t GPUMemory::TotalFreed = 0;
}

void* operator new(const size_t _Size) {
  Oxylus::Memory::TotalAllocated += _Size;
  return malloc(_Size);
}

void operator delete(void* _Block, const size_t _Size) noexcept {
  Oxylus::Memory::TotalFreed += _Size;
  free(_Block);
}
