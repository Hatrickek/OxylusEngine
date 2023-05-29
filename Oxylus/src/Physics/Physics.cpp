#include "src/oxpch.h"
#include "Physics.h"

#include "Utils/Log.h"
#include "Utils/Profiler.h"

namespace Oxylus {
  BPLayerInterfaceImpl Physics::s_LayerInterface;
  JPH::TempAllocatorImpl* Physics::s_TempAllocator = nullptr;
  ObjectVsBroadPhaseLayerFilterImpl Physics::s_ObjectVsBroadPhaseLayerFilterInterface;
  ObjectLayerPairFilterImpl Physics::s_ObjectLayerPairFilterInterface;

  static void TraceImpl(const char* inFMT, ...) {
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);

    OX_CORE_TRACE(buffer);
  }

#ifdef JPH_ENABLE_ASSERTS
  static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine) {
    std::cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr ? inMessage : "") << std::endl;
    return true;
  };
#endif

  void Physics::InitPhysics() {
    // TODO: Override default allocators with Oxylus allocators.
    JPH::RegisterDefaultAllocator();

    // Install callbacks
    JPH::Trace = TraceImpl;
#ifdef JPH_ENABLE_ASSERTS
    JPH::AssertFailed = AssertFailedImpl;
#endif

    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    s_TempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);
  }

  void Physics::ShutdownPhysics() {
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
    delete s_TempAllocator;
  }
}
