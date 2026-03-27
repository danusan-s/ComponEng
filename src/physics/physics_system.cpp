#include "physics/physics_system.hpp"
#include "components/collider_component.hpp"
#include "components/rigidbody_component.hpp"
#include "components/transform_component.hpp"
#include "ecs/entity.hpp"
#include "ecs/world.hpp"
#include "physics/collision_detection.hpp"
#include <vector>

Vec3 GRAVITY = Vec3(0.0f, -9.81f, 0.0f);

struct EntityPhysicsData {
  EntityID entity;
  TransformComponent *transform;
  RigidBodyComponent *rigidbody;
  ColliderComponent *collider;
};

static float accumulatedTime = 0.0f;

void PhysicsSystem::onUpdate(const SystemState &state) {
  accumulatedTime += state.deltaTime;
  const float fixedTimeStep = 1 / 120.0f; // 120 FPS physics update
  if (accumulatedTime < fixedTimeStep)
    return;
  accumulatedTime = 0.0f;

  // Phase 1: Update velocities (apply gravity)
  state.world->query<TransformComponent, RigidBodyComponent>().each(
      [&](TransformComponent &transform, RigidBodyComponent &rigidbody) {
        if (rigidbody.type == RigidBodyComponent::Static)
          return;
        if (rigidbody.type == RigidBodyComponent::Dynamic) {
          rigidbody.velocity += GRAVITY * fixedTimeStep;
        }
      });

  // Phase 2: Collision detection and response
  std::vector<EntityPhysicsData> colliders;

  // Collect all entities with collider component
  state.world
      ->query<TransformComponent, RigidBodyComponent, ColliderComponent>()
      .eachWithEntity([&](EntityID entity, TransformComponent &transform,
                          RigidBodyComponent &rigidbody,
                          ColliderComponent &collider) {
        colliders.push_back({entity, &transform, &rigidbody, &collider});
      });

  state.world->query<TransformComponent, ColliderComponent>()
      .Exclude<RigidBodyComponent>()
      .eachWithEntity([&](EntityID entity, TransformComponent &transform,
                          ColliderComponent &collider) {
        colliders.push_back({entity, &transform, nullptr, &collider});
      });

  // Check collisions between all pairs
  for (size_t i = 0; i < colliders.size(); ++i) {
    for (size_t k = i + 1; k < colliders.size(); ++k) {
      EntityPhysicsData &a = colliders[i];
      EntityPhysicsData &b = colliders[k];

      CollisionInfo info;
      if (!TestCollision(*a.collider, *a.transform, *b.collider, *b.transform,
                         info))
        continue;

      // Get inverse masses
      float inverseMassA = 0.0f;
      float inverseMassB = 0.0f;
      float restitutionA = 0.0f;
      float restitutionB = 0.0f;

      if (a.rigidbody && a.rigidbody->type != RigidBodyComponent::Static) {
        if (a.rigidbody->type == RigidBodyComponent::Dynamic) {
          inverseMassA = 1.0f / a.rigidbody->mass;
        }
        restitutionA = a.rigidbody->restitution;
      }

      if (b.rigidbody && b.rigidbody->type != RigidBodyComponent::Static) {
        if (b.rigidbody->type == RigidBodyComponent::Dynamic) {
          inverseMassB = 1.0f / b.rigidbody->mass;
        }
        // Kinematic: inverseMassB stays 0
        restitutionB = b.rigidbody->restitution;
      }

      // Skip if both are static/kinematic with no mass
      if (inverseMassA == 0.0f && inverseMassB == 0.0f)
        continue;

      // Compute relative velocity
      Vec3 velocityA = a.rigidbody ? a.rigidbody->velocity : Vec3(0.0f);
      Vec3 velocityB = b.rigidbody ? b.rigidbody->velocity : Vec3(0.0f);
      Vec3 relativeVelocity = velocityB - velocityA;
      float velocityAlongNormal = dot(relativeVelocity, info.normal);

      // Skip if separating
      if (velocityAlongNormal > 0.0f)
        continue;

      float e = std::max(restitutionA, restitutionB);

      // Compute impulse scalar
      float j =
          -(1.0f + e) * velocityAlongNormal / (inverseMassA + inverseMassB);

      // Apply impulse
      Vec3 impulse = j * info.normal;

      if (a.rigidbody && a.rigidbody->type == RigidBodyComponent::Dynamic) {
        a.rigidbody->velocity -= impulse * inverseMassA;
      }
      if (b.rigidbody && b.rigidbody->type == RigidBodyComponent::Dynamic) {
        b.rigidbody->velocity += impulse * inverseMassB;
      }

      // Position correction (resolve overlap)
      const float percent =
          0.2f; // Percentage of correction (0.2 = 20% per frame)
      const float slop = 0.01f; // Allowable penetration (0 = no tolerance)
      float correctionMagnitude = std::max(info.penetration - slop, 0.0f) *
                                  percent / (inverseMassA + inverseMassB);
      Vec3 correction = correctionMagnitude * info.normal;

      // Move A opposite to normal, B along normal to separate
      if (a.rigidbody && a.rigidbody->type == RigidBodyComponent::Dynamic) {
        a.transform->position -= correction * inverseMassA;
      }
      if (b.rigidbody && b.rigidbody->type == RigidBodyComponent::Dynamic) {
        b.transform->position += correction * inverseMassB;
      }
    }
  }

  // Phase 3: Update positions
  state.world->query<TransformComponent, RigidBodyComponent>().each(
      [&](TransformComponent &transform, RigidBodyComponent &rigidbody) {
        if (rigidbody.type == RigidBodyComponent::Static)
          return;
        transform.position += rigidbody.velocity * fixedTimeStep;
      });
}
