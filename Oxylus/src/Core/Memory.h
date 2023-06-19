#pragma once
#include <cstddef>
#include <cstdint>

namespace Oxylus {
  struct Memory {
    static uint64_t TotalAllocated;
    static uint64_t TotalFreed;

    static uint64_t CurrentUsage() { return TotalAllocated - TotalFreed; }
  };

  struct GPUMemory {
    static uint64_t TotalAllocated;
    static uint64_t TotalFreed;

    static uint64_t CurrentUsage() { return TotalAllocated - TotalFreed; }
  };
}

extern void Delete(void* block, std::size_t size);
extern void* New(std::size_t size);

void* operator new(std::size_t size);

void operator delete(void* block, std::size_t size);
