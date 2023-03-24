#include "oxpch.h"
#include "PhysicsRigidbody.h"

#include "Physics/Physics.h"
#include <PxRigidDynamic.h>

namespace Oxylus {
  PhysicsRigidbody::PhysicsRigidbody() = default;
  PhysicsRigidbody::~PhysicsRigidbody() = default;

  void PhysicsRigidbody::Init(physx::PxShape* shape) {
    m_Shape = shape;
    if (m_Type == RigidBodyType::Static) {
      m_StaticBody = Physics::s_Physics->createRigidStatic(m_LocalTransform);
      m_StaticBody->attachShape(*m_Shape);
      Physics::s_Scene->addActor(*m_StaticBody);
    }
    if (m_Type == RigidBodyType::Dynamic) {
      m_DynamicBody = Physics::s_Physics->createRigidDynamic(m_LocalTransform);
      m_DynamicBody->attachShape(*m_Shape);
      Physics::s_Scene->addActor(*m_DynamicBody);
    }
  }

  static physx::PxTransform TransformComponentToPxTransform(const glm::vec3& position, const glm::vec3& rotation) {
    physx::PxTransform pxTransform;
    pxTransform.p.x = position.x;
    pxTransform.p.y = position.y;
    pxTransform.p.z = position.z;

    pxTransform.q.x = rotation.x;
    pxTransform.q.y = rotation.y;
    pxTransform.q.z = rotation.z;

    return pxTransform;
  }

  static glm::vec3 PhysXVec3ToGlmVec3(const physx::PxVec3& vec3) {
    return {vec3.x, vec3.y, vec3.z};
  }

  static glm::vec3 PhysXVec3ToGlmVec3(const physx::PxQuat& quat) {
    return {quat.x, quat.y, quat.z};
  }

  void PhysicsRigidbody::OnUpdate(glm::vec3& position, glm::vec3& rotation) const {
    if (m_Type == RigidBodyType::Static) {
      //TODO: (This is not recommended by NVIDIA)
      m_StaticBody->setGlobalPose(TransformComponentToPxTransform(position, rotation));
    }
    if (m_Type == RigidBodyType::Dynamic) {
      if (IsKinematic()) {
        m_DynamicBody->setKinematicTarget(TransformComponentToPxTransform(position, rotation));
      }
      else {
        position = PhysXVec3ToGlmVec3(m_DynamicBody->getGlobalPose().p);

        rotation = PhysXVec3ToGlmVec3(m_DynamicBody->getGlobalPose().q);
      }
    }
  }

  bool PhysicsRigidbody::IsKinematic() const {
    const physx::PxRigidBodyFlags flags = m_DynamicBody->getRigidBodyFlags();
    return flags.isSet(physx::PxRigidBodyFlag::eKINEMATIC);
  }

  void PhysicsRigidbody::SetKinematic(bool value) const {
    m_DynamicBody->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, value);
  }

  void PhysicsRigidbody::Release() const {
    if (m_Type == RigidBodyType::Dynamic) {
      m_DynamicBody->detachShape(*m_Shape);
      Physics::s_Scene->removeActor(*m_DynamicBody);
    }
    if (m_Type == RigidBodyType::Static) {
      m_StaticBody->detachShape(*m_Shape);
      Physics::s_Scene->removeActor(*m_StaticBody);
    }
  }

  void PhysicsRigidbody::ChangeType(RigidBodyType type) {
    Release();
    m_Type = type;

    Init(m_Shape);
  }

  void PhysicsRigidbody::SetGeometry(const physx::PxGeometry& geometry) const {
    m_Shape->setGeometry(geometry);
  }

  void PhysicsRigidbody::SetMass(const float mass) const {
    m_DynamicBody->setMass(mass);
  }

}
