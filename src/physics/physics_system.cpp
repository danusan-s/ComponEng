#include "physics/physics_system.hpp"
#include "components/collider_component.hpp"
#include "components/rigidbody_component.hpp"
#include "components/transform_component.hpp"
#include "ecs/entity.hpp"
#include "ecs/world.hpp"
#include "physics/collision_detection.hpp"
#include <algorithm>
#include <vector>

constexpr Vec3 g_gravity = Vec3(0.0f, -9.81f, 0.0f);

struct EntityPhysicsData {
  EntityID entity;
  TransformComponent *transform;
  RigidBodyComponent *rigidbody;
  ColliderComponent *collider;
};

struct CollisionPair {
  size_t indexA;
  size_t indexB;
  CollisionInfo info;
};

static double g_accumulatedTime = 0.0f;

static void resolveCollision(EntityPhysicsData &a, EntityPhysicsData &b,
                             const CollisionInfo &info) {
  float inverseMassA = 0.0f;
  float inverseMassB = 0.0f;
  float restitutionA = 0.0f;
  float restitutionB = 0.0f;

  if (a.rigidbody && a.rigidbody->type != RigidBodyComponent::Static) {
    if (a.rigidbody->type == RigidBodyComponent::Dynamic) {
      inverseMassA =
          a.rigidbody->mass != 0.0f ? 1.0f / a.rigidbody->mass : 0.0f;
    }
    restitutionA = a.rigidbody->restitution;
  }

  if (b.rigidbody && b.rigidbody->type != RigidBodyComponent::Static) {
    if (b.rigidbody->type == RigidBodyComponent::Dynamic) {
      inverseMassB =
          b.rigidbody->mass != 0.0f ? 1.0f / b.rigidbody->mass : 0.0f;
    }
    restitutionB = b.rigidbody->restitution;
  }

  if (inverseMassA == 0.0f && inverseMassB == 0.0f)
    return;

  Vec3 velocityA = a.rigidbody ? a.rigidbody->velocity : Vec3(0.0f);
  Vec3 velocityB = b.rigidbody ? b.rigidbody->velocity : Vec3(0.0f);
  Vec3 relativeVelocity = velocityB - velocityA;
  float velocityAlongNormal = dot(relativeVelocity, info.normal);

  if (velocityAlongNormal > 0.0f)
    return;

  float e = std::max(restitutionA, restitutionB);

  float j = -(1.0f + e) * velocityAlongNormal / (inverseMassA + inverseMassB);

  Vec3 impulse = j * info.normal;

  if (a.rigidbody && a.rigidbody->type == RigidBodyComponent::Dynamic) {
    a.rigidbody->velocity -= impulse * inverseMassA;
  }
  if (b.rigidbody && b.rigidbody->type == RigidBodyComponent::Dynamic) {
    b.rigidbody->velocity += impulse * inverseMassB;
  }

  constexpr float percent = 0.2f;
  constexpr float slop = 0.01f;
  float correctionMagnitude = std::max(info.penetration - slop, 0.0f) *
                              percent / (inverseMassA + inverseMassB);
  Vec3 correction = correctionMagnitude * info.normal;

  if (a.rigidbody && a.rigidbody->type == RigidBodyComponent::Dynamic) {
    a.transform->position -= correction * inverseMassA;
  }
  if (b.rigidbody && b.rigidbody->type == RigidBodyComponent::Dynamic) {
    b.transform->position += correction * inverseMassB;
  }
}

void PhysicsSystem::onUpdate(const SystemState &state) {
  g_accumulatedTime += state.deltaTime;
  constexpr float fixedTimeStep = 1 / 60.0f;
  if (g_accumulatedTime < fixedTimeStep)
    return;

  while (g_accumulatedTime >= fixedTimeStep) {
    g_accumulatedTime -= fixedTimeStep;

    state.world->query<TransformComponent, RigidBodyComponent>().each(
        [&](TransformComponent &transform, RigidBodyComponent &rigidbody) {
          if (rigidbody.type == RigidBodyComponent::Static)
            return;
          if (rigidbody.type == RigidBodyComponent::Dynamic) {
            rigidbody.velocity += g_gravity * fixedTimeStep;
          }
        });

    state.world->query<TransformComponent, RigidBodyComponent>().each(
        [&](TransformComponent &transform, RigidBodyComponent &rigidbody) {
          if (rigidbody.type == RigidBodyComponent::Static)
            return;
          transform.position += rigidbody.velocity * fixedTimeStep;
        });

    std::vector<EntityPhysicsData> colliders;

    state.world
        ->query<TransformComponent, RigidBodyComponent, ColliderComponent>()
        .eachWithEntity([&](EntityID entity, TransformComponent &transform,
                            RigidBodyComponent &rigidbody,
                            ColliderComponent &collider) {
          colliders.push_back({entity, &transform, &rigidbody, &collider});
        });

    state.world->query<TransformComponent, ColliderComponent>()
        .exclude<RigidBodyComponent>()
        .eachWithEntity([&](EntityID entity, TransformComponent &transform,
                            ColliderComponent &collider) {
          colliders.push_back({entity, &transform, nullptr, &collider});
        });

    size_t n = colliders.size();
    if (n < 2)
      continue;

    size_t numThreads = state.world->threadPool().threadCount();
    size_t chunkSize = std::max(size_t(1), n / numThreads);

    std::vector<std::vector<CollisionPair>> threadCollisions(numThreads);

    std::vector<std::future<void>> futures;
    for (size_t t = 0; t < numThreads; ++t) {
      size_t iStart = t * chunkSize;
      size_t iEnd = (t == numThreads - 1) ? n : iStart + chunkSize;

      if (iStart >= n - 1)
        break;

      futures.push_back(state.world->threadPool().submit([&, t, iStart,
                                                          iEnd]() {
        for (size_t i = iStart; i < iEnd; ++i) {
          for (size_t k = i + 1; k < n; ++k) {
            CollisionInfo info;
            if (!testCollision(*colliders[i].collider, *colliders[i].transform,
                               *colliders[k].collider, *colliders[k].transform,
                               info))
              continue;

            threadCollisions[t].push_back({i, k, info});
          }
        }
      }));
    }

    for (auto &f : futures)
      f.wait();

    for (size_t t = 0; t < threadCollisions.size(); ++t) {
      for (auto &pair : threadCollisions[t]) {
        resolveCollision(colliders[pair.indexA], colliders[pair.indexB],
                         pair.info);
      }
    }
  }
}
