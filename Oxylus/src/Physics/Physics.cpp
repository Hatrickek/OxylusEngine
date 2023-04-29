#include "src/oxpch.h"
#include "Physics.h"

#include "Utils/Log.h"
#include "Utils/Profiler.h"

namespace Oxylus {
  JPH::TempAllocatorImpl* Physics::s_TempAllocator = nullptr;
  Ref<JPH::PhysicsSystem> Physics::s_PhysicsSystem;
  Ref<JPH::JobSystemThreadPool> Physics::s_JobSystem;
  BPLayerInterfaceImpl Physics::s_LayerInterface;
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
    //TODO: Override default allocators with Oxylus allocators.
    JPH::RegisterDefaultAllocator();

    // Install callbacks
    JPH::Trace = TraceImpl;
#ifdef JPH_ENABLE_ASSERTS
    JPH::AssertFailed = AssertFailedImpl;
#endif

    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    s_TempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);

    s_JobSystem = CreateRef<JPH::JobSystemThreadPool>();
    s_JobSystem->Init(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

    s_PhysicsSystem = CreateRef<JPH::PhysicsSystem>();
    s_PhysicsSystem->Init(MAX_BODIES,
      0,
      MAX_BODY_PAIRS,
      MAX_CONTACT_CONSTRAINS,
      s_LayerInterface,
      s_ObjectVsBroadPhaseLayerFilterInterface,
      s_ObjectLayerPairFilterInterface);

    //TODO: Call this after a body has been added.
    s_PhysicsSystem->OptimizeBroadPhase();
  }

  void Physics::Update(float) {
    const float cDeltaTime = 1.0f / 60.0f; //Update rate

    // If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep the simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
    const int cCollisionSteps = 1;

    // If you want more accurate step results you can do multiple sub steps within a collision step. Usually you would set this to 1.
    const int cIntegrationSubSteps = 1;

    // Step the world
    s_PhysicsSystem->Update(cDeltaTime, cCollisionSteps, cIntegrationSubSteps, s_TempAllocator, s_JobSystem.get());
  }

  void Physics::Shutdown() {
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
    delete s_TempAllocator;
  }
}
