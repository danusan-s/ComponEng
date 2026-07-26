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

  Type type;
  core::Vec3 velocity;
  float mass;
  float restitution;
};

#define RIGIDBODY_COMPONENT_FIELDS(F, ctx) \
  F(enum, type, 0, ctx) \
  F(Vec3, velocity, ctx) \
  F(float, mass, 1.0f, ctx) \
  F(float, restitution, 0.5f, ctx)

SERIALIZABLE_COMPONENT(RigidBodyComponent, RIGIDBODY_COMPONENT_FIELDS)

#undef RIGIDBODY_COMPONENT_FIELDS

} // namespace componeng::components
