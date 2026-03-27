#pragma once
#include "core/types.hpp"
#include <variant>

// Take object transform as center
struct AABB {
  Vec3 localCenter; // offset from object transform to OBB center
  Vec3 halfExtents;
};

struct OBB {
  Vec3 localCenter;   // offset from object transform to OBB center
  Vec3 localRotation; // rotation from local space to world space
  Vec3 halfExtents;
};

struct Sphere {
  Vec3 localCenter; // offset from object transform to sphere center
  float radius;
};

struct Capsule {
  Vec3 localCenter; // offset from object transform to capsule center
  Vec3 axis; // normalized direction from one hemisphere center to the other
  float radius;
  float halfHeight; // height excluding hemispheres
};

struct Plane {
  glm::vec3 normal; // normalized
  float distance; // distance from origin along normal, no need for local center
                  // since plane is infinite and can only be moved along normal
                  // which is defined by distance
};

// --- Collider Type Enum ---
enum class ColliderType { AABB, OBB, Sphere, Capsule, Plane };

// --- Collider Component ---
struct ColliderComponent {
  ColliderType type;
  std::variant<AABB, OBB, Sphere, Capsule, Plane> shape;
};
