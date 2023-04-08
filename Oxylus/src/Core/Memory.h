#pragma once

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

void* operator new(size_t _Size);

void operator delete(void* _Block, size_t _Size);
