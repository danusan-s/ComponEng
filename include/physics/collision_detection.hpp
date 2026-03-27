#pragma once
#include "components/collider_component.hpp"
#include "components/transform_component.hpp"
#include "core/types.hpp"

struct CollisionInfo {
  Vec3 normal;      // From A to B
  float penetration;
};

// Get world position of collider shape center
Vec3 GetColliderWorldPosition(const TransformComponent& transform,
                              const Vec3& localCenter);

// AABB vs AABB collision detection
bool TestAABBAABB(const AABB& a, const Vec3& posA,
                  const AABB& b, const Vec3& posB,
                  CollisionInfo& info);

// Sphere vs Sphere collision detection
bool TestSphereSphere(const Sphere& a, const Vec3& posA,
                      const Sphere& b, const Vec3& posB,
                      CollisionInfo& info);

// Generic collision test between two colliders
bool TestCollision(const ColliderComponent& colliderA,
                   const TransformComponent& transformA,
                   const ColliderComponent& colliderB,
                   const TransformComponent& transformB,
                   CollisionInfo& info);