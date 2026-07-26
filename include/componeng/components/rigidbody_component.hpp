#pragma once
#include "componeng/components/component_serializer.hpp"
#include "componeng/core/types.hpp"
#include "json.hpp"

namespace componeng::components {

struct RigidBodyComponent {
  static constexpr const char* component_name = "RigidBodyComponent";

  enum Type {
    Static,   // Immovable object, not affected by forces
    Dynamic,  // Affected by forces, can move and rotate
    Kinematic // Moves according to user velocities, not affected by forces
  };

  Type type = Static;
  core::Vec3 velocity;
  float mass = 1.0f;
  float restitution = 0.5f;
};

#define RIGIDBODY_COMPONENT_FIELDS(F, ctx) \
  F(type, ctx) \
  F(velocity, ctx) \
  F(mass, ctx) \
  F(restitution, ctx)

SERIALIZABLE_COMPONENT(RigidBodyComponent, RIGIDBODY_COMPONENT_FIELDS)

#undef RIGIDBODY_COMPONENT_FIELDS

} // namespace componeng::components
