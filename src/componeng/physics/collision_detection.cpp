#include "componeng/physics/collision_detection.hpp"
#include "componeng/components/transform_component.hpp"
#include <algorithm>
#include <cmath>

namespace componeng::physics {

components::TransformComponent
getWorldTransform(const components::TransformComponent &localTransform,
                  const components::TransformComponent &parentTransform) {
  components::TransformComponent worldTransform;
  worldTransform.position = parentTransform.position + localTransform.position;
  worldTransform.rotation = parentTransform.rotation + localTransform.rotation;
  worldTransform.scale = parentTransform.scale * localTransform.scale;
  return worldTransform;
}

bool testBoxBox(const components::TransformComponent &a,
                const components::TransformComponent &b, CollisionInfo &info) {
  core::Vec3 diff = b.position - a.position;
  core::Vec3 halfExtentsA = a.scale;
  core::Vec3 halfExtentsB = b.scale;

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
    info.normal = core::Vec3((diff.x > 0) ? 1.0f : -1.0f, 0, 0);
  } else if (overlapY < overlapZ) {
    info.penetration = overlapY;
    info.normal = core::Vec3(0, (diff.y > 0) ? 1.0f : -1.0f, 0);
  } else {
    info.penetration = overlapZ;
    info.normal = core::Vec3(0, 0, (diff.z > 0) ? 1.0f : -1.0f);
  }

  return true;
}

bool testSphereSphere(const components::TransformComponent &a,
                      const components::TransformComponent &b,
                      CollisionInfo &info) {
  core::Vec3 delta = b.position - a.position;
  float distance = length(delta);

  float radiusSum = a.scale.x + b.scale.x; // Assuming scale.x is the radius

  if (distance >= radiusSum)
    return false;

  if (distance < 1e-6f) {
    info.normal = core::Vec3(1, 0, 0);
    info.penetration = radiusSum;
  } else {
    info.normal = delta / distance;
    info.penetration = radiusSum - distance;
  }

  return true;
}

bool testAABBSphere(const components::TransformComponent &a,
                    const components::TransformComponent &b,
                    CollisionInfo &info) {
  core::Vec3 centerA = a.position;
  core::Vec3 sphereCenter = b.position;

  core::Vec3 minA = centerA - a.scale;
  core::Vec3 maxA = centerA + a.scale;

  float radius = b.scale.x; // Assuming scale.x is the radius

  core::Vec3 closestPoint;
  closestPoint.x = std::max(minA.x, std::min(sphereCenter.x, maxA.x));
  closestPoint.y = std::max(minA.y, std::min(sphereCenter.y, maxA.y));
  closestPoint.z = std::max(minA.z, std::min(sphereCenter.z, maxA.z));

  core::Vec3 delta = sphereCenter - closestPoint;
  float distanceSq = dot(delta, delta);

  if (distanceSq > (radius * radius)) {
    return false;
  }

  float distance = std::sqrt(distanceSq);

  if (distance > 1e-4f) {
    info.normal = delta / distance;
    info.penetration = radius - distance;
  } else {
    core::Vec3 dA = sphereCenter - minA;
    core::Vec3 dB = maxA - sphereCenter;

    float minOverlap = std::min({dA.x, dB.x, dA.y, dB.y, dA.z, dB.z});

    core::Vec3 floorToSphere = sphereCenter - centerA;
    if (length(floorToSphere) > 1e-6f)
      info.normal = normalize(floorToSphere);
    else
      info.normal = core::Vec3(0, -1, 0);
    info.penetration = radius + minOverlap;
  }

  return true;
}

bool testSphereBox(const components::TransformComponent &a,
                   const components::TransformComponent &b,
                   CollisionInfo &info) {
  bool res = testAABBSphere(b, a, info);
  info.normal = -info.normal;
  return res;
}

bool testCollision(const components::ColliderComponent &colliderA,
                   const components::TransformComponent &transformA,
                   const components::ColliderComponent &colliderB,
                   const components::TransformComponent &transformB,
                   CollisionInfo &info) {
  const components::TransformComponent worldTransformA =
      getWorldTransform(colliderA.transform, transformA);
  const components::TransformComponent worldTransformB =
      getWorldTransform(colliderB.transform, transformB);

  if (colliderA.type == components::ColliderType::Box &&
      colliderB.type == components::ColliderType::Box) {
    return testBoxBox(worldTransformA, worldTransformB, info);
  }

  if (colliderA.type == components::ColliderType::Sphere &&
      colliderB.type == components::ColliderType::Sphere) {
    return testSphereSphere(worldTransformA, worldTransformB, info);
  }

  if (colliderA.type == components::ColliderType::Box &&
      colliderB.type == components::ColliderType::Sphere) {
    return testBoxSphere(worldTransformA, worldTransformB, info);
  }
  if (colliderA.type == components::ColliderType::Sphere &&
      colliderB.type == components::ColliderType::Box) {
    return testSphereBox(worldTransformA, worldTransformB, info);
  }

  return false;
}

} // namespace componeng::physics
