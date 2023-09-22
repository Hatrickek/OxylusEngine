#include "Memory.h"
#include "Utils/Profiler.h"

namespace Oxylus {
uint64_t Memory::TotalAllocated = 0;
uint64_t Memory::TotalFreed = 0;

uint64_t GPUMemory::TotalAllocated = 0;
uint64_t GPUMemory::TotalFreed = 0;
}

void Delete(void* block, std::size_t size) {
  Oxylus::Memory::TotalFreed += size;
  OX_FREE(block);
  free(block);
}

void* New(std::size_t size) {
  Oxylus::Memory::TotalAllocated += size;
  const auto ptr = malloc(size);
  OX_ALLOC(ptr, size);
  return ptr;
}

void* operator new(const std::size_t size) {
  return New(size);
}

void operator delete(void* block, const std::size_t size) {
  Delete(block, size);
}
