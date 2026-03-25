#include "systems/physics_system.hpp"
#include "components/rigidbody_component.hpp"
#include "components/transform_component.hpp"
#include "ecs/world.hpp"

Vec3 GRAVITY = Vec3(0.0f, -9.81f, 0.0f);

void PhysicsSystem::Update(float deltaTime) {
  world->query<TransformComponent, RigidBodyComponent>().eachOptional(
      [&](TransformComponent &transform, RigidBodyComponent &rigidbody) {
        if (rigidbody.type == RigidBodyComponent::Static)
          return;
        if (rigidbody.type == RigidBodyComponent::Dynamic) {
          rigidbody.velocity += GRAVITY * deltaTime;
        }
        transform.position += rigidbody.velocity * deltaTime;
      });
}
