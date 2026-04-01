#include "physics/collision_detection.hpp"
#include <algorithm>
#include <cmath>

Vec3 getColliderWorldPosition(const TransformComponent &transform,
                              const Vec3 &localCenter) {
  return transform.position + localCenter;
}

bool testAABBAABB(const AABB &a, const Vec3 &posA, const AABB &b,
                  const Vec3 &posB, CollisionInfo &info) {
  Vec3 centerA = posA + a.localCenter;
  Vec3 centerB = posB + b.localCenter;

  Vec3 diff = centerB - centerA;

  float overlapX = (a.halfExtents.x + b.halfExtents.x) - std::abs(diff.x);
  if (overlapX <= 0)
    return false;

  float overlapY = (a.halfExtents.y + b.halfExtents.y) - std::abs(diff.y);
  if (overlapY <= 0)
    return false;

  float overlapZ = (a.halfExtents.z + b.halfExtents.z) - std::abs(diff.z);
  if (overlapZ <= 0)
    return false;

  if (overlapX < overlapY && overlapX < overlapZ) {
    info.penetration = overlapX;
    info.normal = Vec3((diff.x > 0) ? 1.0f : -1.0f, 0, 0);
  } else if (overlapY < overlapZ) {
    info.penetration = overlapY;
    info.normal = Vec3(0, (diff.y > 0) ? 1.0f : -1.0f, 0);
  } else {
    info.penetration = overlapZ;
    info.normal = Vec3(0, 0, (diff.z > 0) ? 1.0f : -1.0f);
  }

  return true;
}

bool testSphereSphere(const Sphere &a, const Vec3 &posA, const Sphere &b,
                      const Vec3 &posB, CollisionInfo &info) {
  Vec3 centerA = posA + a.localCenter;
  Vec3 centerB = posB + b.localCenter;

  Vec3 delta = centerB - centerA;
  float distance = length(delta);
  float radiusSum = a.radius + b.radius;

  if (distance >= radiusSum)
    return false;

  if (distance < 1e-6f) {
    info.normal = Vec3(1, 0, 0);
    info.penetration = radiusSum;
  } else {
    info.normal = delta / distance;
    info.penetration = radiusSum - distance;
  }

  return true;
}

bool testAABBSphere(const AABB &a, const Vec3 &posA, const Sphere &b,
                    const Vec3 &posB, CollisionInfo &info) {
  Vec3 centerA = posA + a.localCenter;
  Vec3 sphereCenter = posB + b.localCenter;

  Vec3 minA = centerA - a.halfExtents;
  Vec3 maxA = centerA + a.halfExtents;

  Vec3 closestPoint;
  closestPoint.x = std::max(minA.x, std::min(sphereCenter.x, maxA.x));
  closestPoint.y = std::max(minA.y, std::min(sphereCenter.y, maxA.y));
  closestPoint.z = std::max(minA.z, std::min(sphereCenter.z, maxA.z));

  Vec3 delta = sphereCenter - closestPoint;
  float distanceSq = dot(delta, delta);

  if (distanceSq > (b.radius * b.radius)) {
    return false;
  }

  float distance = std::sqrt(distanceSq);

  if (distance > 1e-4f) {
    info.normal = delta / distance;
    info.penetration = b.radius - distance;
  } else {
    Vec3 dA = sphereCenter - minA;
    Vec3 dB = maxA - sphereCenter;

    float minOverlap = std::min({dA.x, dB.x, dA.y, dB.y, dA.z, dB.z});

    Vec3 floorToSphere = sphereCenter - centerA;
    if (length(floorToSphere) > 1e-6f)
      info.normal = normalize(floorToSphere);
    else
      info.normal = Vec3(0, -1, 0);
    info.penetration = b.radius + minOverlap;
  }

  return true;
}

bool testSphereAABB(const Sphere &a, const Vec3 &posA, const AABB &b,
                    const Vec3 &posB, CollisionInfo &info) {
  bool res = testAABBSphere(b, posB, a, posA, info);
  info.normal = -info.normal;
  return res;
}

bool testCollision(const ColliderComponent &colliderA,
                   const TransformComponent &transformA,
                   const ColliderComponent &colliderB,
                   const TransformComponent &transformB, CollisionInfo &info) {
  if (colliderA.type == ColliderType::AABB &&
      colliderB.type == ColliderType::AABB) {
    const AABB &a = std::get<AABB>(colliderA.shape);
    const AABB &b = std::get<AABB>(colliderB.shape);
    return testAABBAABB(a, transformA.position, b, transformB.position, info);
  }

  if (colliderA.type == ColliderType::Sphere &&
      colliderB.type == ColliderType::Sphere) {
    const Sphere &a = std::get<Sphere>(colliderA.shape);
    const Sphere &b = std::get<Sphere>(colliderB.shape);
    return testSphereSphere(a, transformA.position, b, transformB.position,
                            info);
  }

  if (colliderA.type == ColliderType::AABB &&
      colliderB.type == ColliderType::Sphere) {
    const AABB &a = std::get<AABB>(colliderA.shape);
    const Sphere &b = std::get<Sphere>(colliderB.shape);
    return testAABBSphere(a, transformA.position, b, transformB.position, info);
  }
  if (colliderA.type == ColliderType::Sphere &&
      colliderB.type == ColliderType::AABB) {
    const Sphere &a = std::get<Sphere>(colliderA.shape);
    const AABB &b = std::get<AABB>(colliderB.shape);
    return testSphereAABB(a, transformA.position, b, transformB.position, info);
  }

  return false;
}
