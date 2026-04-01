#include "physics/collision_detection.hpp"
#include <algorithm>
#include <cmath>

Vec3 GetColliderWorldPosition(const TransformComponent &transform,
                              const Vec3 &localCenter) {
  // For simplicity, assume no rotation applied to local center
  // In a more complete system, we'd apply rotation
  return transform.position + localCenter;
}

bool TestAABBAABB(const AABB &a, const Vec3 &posA, const AABB &b,
                  const Vec3 &posB, CollisionInfo &info) {
  // 1. Get world centers
  Vec3 centerA = posA + a.localCenter;
  Vec3 centerB = posB + b.localCenter;

  // 2. Calculate the distance between centers and the combined half-extents
  Vec3 diff = centerB - centerA;

  // Overlap on X
  float overlapX = (a.halfExtents.x + b.halfExtents.x) - std::abs(diff.x);
  if (overlapX <= 0)
    return false;

  // Overlap on Y
  float overlapY = (a.halfExtents.y + b.halfExtents.y) - std::abs(diff.y);
  if (overlapY <= 0)
    return false;

  // Overlap on Z
  float overlapZ = (a.halfExtents.z + b.halfExtents.z) - std::abs(diff.z);
  if (overlapZ <= 0)
    return false;

  // 3. Find the Separating Axis with the LEAST penetration
  // This ensures we resolve the collision along the shortest path
  if (overlapX < overlapY && overlapX < overlapZ) {
    info.penetration = overlapX;
    // If B is to the right of A, normal is (1,0,0). Otherwise (-1,0,0).
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

bool TestSphereSphere(const Sphere &a, const Vec3 &posA, const Sphere &b,
                      const Vec3 &posB, CollisionInfo &info) {
  Vec3 centerA = posA + a.localCenter;
  Vec3 centerB = posB + b.localCenter;

  Vec3 delta = centerB - centerA;
  float distance = length(delta);
  float radiusSum = a.radius + b.radius;

  if (distance >= radiusSum)
    return false;

  if (distance < 1e-6f) {
    // Spheres at same position, pick arbitrary normal
    info.normal = Vec3(1, 0, 0);
    info.penetration = radiusSum;
  } else {
    info.normal = delta / distance;
    info.penetration = radiusSum - distance;
  }

  return true;
}

bool TestAABBSphere(const AABB &a, const Vec3 &posA, const Sphere &b,
                    const Vec3 &posB, CollisionInfo &info) {
  // 1. Get world centers
  Vec3 centerA = posA + a.localCenter;
  Vec3 sphereCenter = posB + b.localCenter;

  // 2. Find the closest point on the AABB to the sphere center
  // We "clamp" the sphere center to the AABB's bounds
  Vec3 minA = centerA - a.halfExtents;
  Vec3 maxA = centerA + a.halfExtents;

  Vec3 closestPoint;
  closestPoint.x = std::max(minA.x, std::min(sphereCenter.x, maxA.x));
  closestPoint.y = std::max(minA.y, std::min(sphereCenter.y, maxA.y));
  closestPoint.z = std::max(minA.z, std::min(sphereCenter.z, maxA.z));

  // 3. Calculate distance from closest point to sphere center
  Vec3 delta = sphereCenter - closestPoint;
  float distanceSq = dot(delta, delta); // Use squared distance for performance

  // 4. Check if distance is less than radius
  if (distanceSq > (b.radius * b.radius)) {
    return false;
  }

  float distance = std::sqrt(distanceSq);

  // 5. Fill Collision Info
  if (distance > 1e-4f) {
    // Sphere center is outside the AABB
    info.normal = delta / distance; // Normalize
    info.penetration = b.radius - distance;
  } else {
    // SPECIAL CASE: Sphere center is INSIDE the AABB
    // We need to find the shallowest face to push the sphere out of
    Vec3 dA = sphereCenter - minA;
    Vec3 dB = maxA - sphereCenter;

    float minOverlap = std::min({dA.x, dB.x, dA.y, dB.y, dA.z, dB.z});

    Vec3 floorToSphere = sphereCenter - centerA;
    if (length(floorToSphere) > 1e-6f)
      info.normal = normalize(floorToSphere);
    else
      info.normal = Vec3(0, -1, 0); // fallback: push up
    info.penetration = b.radius + minOverlap;
  }

  return true;
}

bool TestSphereAABB(const Sphere &a, const Vec3 &posA, const AABB &b,
                    const Vec3 &posB, CollisionInfo &info) {
  bool res = TestAABBSphere(b, posB, a, posA, info);
  info.normal = -info.normal; // Flip normal for correct direction
  return res;
}

bool TestCollision(const ColliderComponent &colliderA,
                   const TransformComponent &transformA,
                   const ColliderComponent &colliderB,
                   const TransformComponent &transformB, CollisionInfo &info) {
  // Only handle AABB vs AABB and Sphere vs Sphere for now
  if (colliderA.type == ColliderType::AABB &&
      colliderB.type == ColliderType::AABB) {
    const AABB &a = std::get<AABB>(colliderA.shape);
    const AABB &b = std::get<AABB>(colliderB.shape);
    return TestAABBAABB(a, transformA.position, b, transformB.position, info);
  }

  if (colliderA.type == ColliderType::Sphere &&
      colliderB.type == ColliderType::Sphere) {
    const Sphere &a = std::get<Sphere>(colliderA.shape);
    const Sphere &b = std::get<Sphere>(colliderB.shape);
    return TestSphereSphere(a, transformA.position, b, transformB.position,
                            info);
  }

  if (colliderA.type == ColliderType::AABB &&
      colliderB.type == ColliderType::Sphere) {
    const AABB &a = std::get<AABB>(colliderA.shape);
    const Sphere &b = std::get<Sphere>(colliderB.shape);
    return TestAABBSphere(a, transformA.position, b, transformB.position, info);
  }
  if (colliderA.type == ColliderType::Sphere &&
      colliderB.type == ColliderType::AABB) {
    const Sphere &a = std::get<Sphere>(colliderA.shape);
    const AABB &b = std::get<AABB>(colliderB.shape);
    return TestSphereAABB(a, transformA.position, b, transformB.position, info);
  }

  // Unsupported collision pair
  return false;
}
