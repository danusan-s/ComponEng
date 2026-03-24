#include "systems/physics_system.hpp"
#include "components/bounding_box_component.hpp"
#include "components/rigidbody_component.hpp"
#include "components/transform_component.hpp"
#include "ecs/world.hpp"

void PhysicsSystem::Update(float deltaTime) {
  world->query<TransformComponent, RigidBodyComponent>().each(
      [&](TransformComponent &transform, RigidBodyComponent &rigidbody) {
        rigidbody.velocity += rigidbody.acceleration * deltaTime;
        transform.position += rigidbody.velocity * deltaTime;
      });
}
