#pragma once
#include <PxRigidStatic.h>

#include "glm/mat4x4.hpp"

namespace Oxylus {
  class PhysicsRigidbody {
  public:
    enum class RigidBodyType {
      Static,
      Dynamic
    };

    PhysicsRigidbody();
    ~PhysicsRigidbody();

    void Init(physx::PxShape* shape);
    void OnUpdate(glm::vec3& position, glm::vec3& rotation) const;

    void Release() const;
    void ChangeType(RigidBodyType type);

    void SetKinematic(bool value) const;
    void SetGeometry(const physx::PxGeometry& geometry) const;
    void SetMass(float mass) const;

    bool IsKinematic() const;
    physx::PxShape* GetShape() const { return m_Shape; }
    const RigidBodyType& GetType() const { return m_Type; }

  private:
    RigidBodyType m_Type = RigidBodyType::Dynamic;
    physx::PxRigidStatic* m_StaticBody = nullptr;
    physx::PxRigidDynamic* m_DynamicBody = nullptr;
    physx::PxTransform m_LocalTransform;
    physx::PxShape* m_Shape = nullptr;
  };
}
