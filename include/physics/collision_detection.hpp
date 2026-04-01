#pragma once

#include "components/collider_component.hpp"
#include "components/transform_component.hpp"
#include "core/types.hpp"

struct CollisionInfo {
  Vec3 normal;
  float penetration;
};

Vec3 getColliderWorldPosition(const TransformComponent& transform,
                              const Vec3& localCenter);

bool testAABBAABB(const AABB& a, const Vec3& posA,
                  const AABB& b, const Vec3& posB,
                  CollisionInfo& info);

bool testSphereSphere(const Sphere& a, const Vec3& posA,
                      const Sphere& b, const Vec3& posB,
                      CollisionInfo& info);

bool testCollision(const ColliderComponent& colliderA,
                   const TransformComponent& transformA,
                   const ColliderComponent& colliderB,
                   const TransformComponent& transformB,
                   CollisionInfo& info);