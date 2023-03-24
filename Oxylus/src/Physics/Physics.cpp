#include "oxpch.h"
#include "Physics.h"

#include "Utils/Log.h"
#include "Utils/Profiler.h"

#undef PX_RELEASE
#define PX_RELEASE(x) if(x) { (x)->release(); (x) = NULL; }

namespace Oxylus {
  class PhysxErrorCallback : public physx::PxErrorCallback {
  public:
    PhysxErrorCallback() = default;
    ~PhysxErrorCallback() override = default;

    void reportError(const physx::PxErrorCode::Enum code, const char* message, const char* file, int line) override {
      switch (code) {
        case physx::PxErrorCode::eNO_ERROR: break;
        case physx::PxErrorCode::eDEBUG_INFO: OX_CORE_INFO("PhysX Error: {}", message);
          break;
        case physx::PxErrorCode::eDEBUG_WARNING: OX_CORE_WARN("PhysX Error: {}", message);
          break;
        case physx::PxErrorCode::eINVALID_PARAMETER: OX_CORE_ERROR("PhysX Error: {}", message);
          break;
        case physx::PxErrorCode::eINVALID_OPERATION: OX_CORE_ERROR("PhysX Error: {}", message);
          break;
        case physx::PxErrorCode::eOUT_OF_MEMORY: OX_CORE_ERROR("PhysX Error: {}", message);
          break;
        case physx::PxErrorCode::eINTERNAL_ERROR: OX_CORE_ERROR("PhysX Error: {}", message);
          break;
        case physx::PxErrorCode::eABORT: OX_CORE_ERROR("PhysX Error: {}", message);
          OX_CORE_WARN("PhysX Error: {}", message);
          break;
        case physx::PxErrorCode::ePERF_WARNING: break;
        case physx::PxErrorCode::eMASK_ALL: break;
      }
    }
  };

  static physx::PxDefaultAllocator s_Allocator;
  static PhysxErrorCallback s_ErrorCallback;
  static physx::PxFoundation* s_Foundation = NULL;
  physx::PxPhysics* Physics::s_Physics = NULL;
  static physx::PxPvd* s_Pvd = NULL;
  physx::PxScene* Physics::s_Scene = NULL;
  static physx::PxDefaultCpuDispatcher* s_Dispatcher = NULL;

  float Physics::yGravity = -9.81f;

  void Physics::InitPhysics() {
    s_Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, s_Allocator, s_ErrorCallback);
    s_Pvd = PxCreatePvd(*s_Foundation);
    physx::PxPvdTransport* transport = physx::PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
    s_Pvd->connect(*transport, physx::PxPvdInstrumentationFlag::eALL);
    s_Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *s_Foundation, physx::PxTolerancesScale(), true, s_Pvd);
    //physx::PxCooking* m_Cooking = PxCreateCooking(PX_PHYSICS_VERSION, *m_Foundation, physx::PxCookingParams(scale));
    if (!PxInitExtensions(*s_Physics, s_Pvd))
      OX_CORE_ERROR("PxInitExtensions failed!");
    physx::PxSceneDesc sceneDesc(s_Physics->getTolerancesScale());
    sceneDesc.gravity = physx::PxVec3(0.0f, yGravity, 0.0f);
    s_Dispatcher = physx::PxDefaultCpuDispatcherCreate(2);
    sceneDesc.cpuDispatcher = s_Dispatcher;
    sceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;
    s_Scene = s_Physics->createScene(sceneDesc);
    if (physx::PxPvdSceneClient* pvdClient = s_Scene->getScenePvdClient()) {
      pvdClient->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
      pvdClient->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
      pvdClient->setScenePvdFlag(physx::PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
    }
    OX_CORE_TRACE("PhysX initialized.");
  }

  void Physics::Update(float deltaTime) {
    ZoneScoped;
    if (!s_Scene)
      return;
    s_Scene->simulate(1.0f / 60.0f);
    s_Scene->fetchResults(true);
  }

  void Physics::CleanupPhysics() {
    PX_RELEASE(s_Physics)
    PX_RELEASE(s_Foundation)
  }
}
