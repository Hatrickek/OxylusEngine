#include <oxpch.h>
#include "Memory.h"
#include "Utils/Profiler.h"

namespace Oxylus {
  uint64_t Memory::TotalAllocated = 0;
  uint64_t Memory::TotalFreed = 0;

  uint64_t GPUMemory::TotalAllocated = 0;
  uint64_t GPUMemory::TotalFreed = 0;
}

void Delete(void* _Block, size_t _Size) {
  Oxylus::Memory::TotalFreed += _Size;
  TracyFree(_Block);
  free(_Block);
}

void* New(size_t _Size) {
  Oxylus::Memory::TotalAllocated += _Size;
  const auto ptr = malloc(_Size);
  TracyAlloc(ptr, _Size);
  return ptr;
}

void* operator new(const size_t _Size) {
  return New(_Size);
}

void operator delete(void* _Block, const size_t _Size) {
  Delete(_Block, _Size);
}
