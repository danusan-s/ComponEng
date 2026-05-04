#include "componeng/physics/collision_detection.hpp"
#include "componeng/components/transform_component.hpp"
#include <algorithm>
#include <cmath>

TransformComponent
getWorldTransform(const TransformComponent &localTransform,
                  const TransformComponent &parentTransform) {
  TransformComponent worldTransform;
  worldTransform.position = parentTransform.position + localTransform.position;
  worldTransform.rotation = parentTransform.rotation + localTransform.rotation;
  worldTransform.scale = parentTransform.scale * localTransform.scale;
  return worldTransform;
}

bool testBoxBox(const TransformComponent &a, const TransformComponent &b,
                CollisionInfo &info) {
  Vec3 diff = b.position - a.position;
  Vec3 halfExtentsA = a.scale;
  Vec3 halfExtentsB = b.scale;

  float overlapX = (halfExtentsA.x + halfExtentsB.x) - std::abs(diff.x);
  if (overlapX <= 0)
    return false;

  float overlapY = (halfExtentsA.y + halfExtentsB.y) - std::abs(diff.y);
  if (overlapY <= 0)
    return false;

  float overlapZ = (halfExtentsA.z + halfExtentsB.z) - std::abs(diff.z);
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

bool testSphereSphere(const TransformComponent &a, const TransformComponent &b,
                      CollisionInfo &info) {
  Vec3 delta = b.position - a.position;
  float distance = length(delta);

  float radiusSum = a.scale.x + b.scale.x; // Assuming scale.x is the radius

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

bool testAABBSphere(const TransformComponent &a, const TransformComponent &b,
                    CollisionInfo &info) {
  Vec3 centerA = a.position;
  Vec3 sphereCenter = b.position;

  Vec3 minA = centerA - a.scale;
  Vec3 maxA = centerA + a.scale;

  float radius = b.scale.x; // Assuming scale.x is the radius

  Vec3 closestPoint;
  closestPoint.x = std::max(minA.x, std::min(sphereCenter.x, maxA.x));
  closestPoint.y = std::max(minA.y, std::min(sphereCenter.y, maxA.y));
  closestPoint.z = std::max(minA.z, std::min(sphereCenter.z, maxA.z));

  Vec3 delta = sphereCenter - closestPoint;
  float distanceSq = dot(delta, delta);

  if (distanceSq > (radius * radius)) {
    return false;
  }

  float distance = std::sqrt(distanceSq);

  if (distance > 1e-4f) {
    info.normal = delta / distance;
    info.penetration = radius - distance;
  } else {
    Vec3 dA = sphereCenter - minA;
    Vec3 dB = maxA - sphereCenter;

    float minOverlap = std::min({dA.x, dB.x, dA.y, dB.y, dA.z, dB.z});

    Vec3 floorToSphere = sphereCenter - centerA;
    if (length(floorToSphere) > 1e-6f)
      info.normal = normalize(floorToSphere);
    else
      info.normal = Vec3(0, -1, 0);
    info.penetration = radius + minOverlap;
  }

  return true;
}

bool testSphereBox(const TransformComponent &a, const TransformComponent &b,
                   CollisionInfo &info) {
  bool res = testAABBSphere(b, a, info);
  info.normal = -info.normal;
  return res;
}

bool testCollision(const ColliderComponent &colliderA,
                   const TransformComponent &transformA,
                   const ColliderComponent &colliderB,
                   const TransformComponent &transformB, CollisionInfo &info) {
  const TransformComponent worldTransformA =
      getWorldTransform(colliderA.transform, transformA);
  const TransformComponent worldTransformB =
      getWorldTransform(colliderB.transform, transformB);

  if (colliderA.type == ColliderType::Box &&
      colliderB.type == ColliderType::Box) {
    return testBoxBox(worldTransformA, worldTransformB, info);
  }

  if (colliderA.type == ColliderType::Sphere &&
      colliderB.type == ColliderType::Sphere) {
    return testSphereSphere(worldTransformA, worldTransformB, info);
  }

  if (colliderA.type == ColliderType::Box &&
      colliderB.type == ColliderType::Sphere) {
    return testAABBSphere(worldTransformA, worldTransformB, info);
  }
  if (colliderA.type == ColliderType::Sphere &&
      colliderB.type == ColliderType::Box) {
    return testSphereBox(worldTransformA, worldTransformB, info);
  }

  return false;
}
