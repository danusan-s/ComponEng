#pragma once

#include "componeng/components/collider_component.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/core/types.hpp"

/**
 * @brief Output of a collision test: contact normal and penetration depth.
 *
 * The normal points from collider A toward collider B.
 */
struct CollisionInfo {
  Vec3 normal;
  float penetration;
};

/**
 * @brief Computes the world-space transform of a collider given its local
 * transform and entity transform.
 */
TransformComponent getWorldTransform(const TransformComponent &localTransform,
                                       const TransformComponent &parentTransform);

/**
 * @brief Tests two axis-aligned bounding boxes for overlap.
 * @return true if overlapping, with contact info filled into @p info.
 */
bool testBoxBox(const TransformComponent &a, const TransformComponent &b,
              CollisionInfo &info);

/**
 * @brief Tests two spheres for overlap.
 * @return true if overlapping, with contact info filled into @p info.
 */
bool testSphereSphere(const TransformComponent &a, const TransformComponent &b,
                     CollisionInfo &info);

/**
 * @brief Tests a Box and a sphere for overlap.
 * @return true if overlapping, with contact info filled into @p info.
 */
bool testBoxSphere(const TransformComponent &box, const TransformComponent &sphere,
                  CollisionInfo &info);

/**
 * @brief Tests a sphere and a Box for overlap (reverse of testBoxSphere).
 *
 * Swaps param and calls testBoxSphere. Reverses normal direction in @p info to
 * maintain A->B convention.
 *
 * @return true if overlapping, with contact info filled into @p info.
 */
bool testSphereBox(const TransformComponent &sphere, const TransformComponent &box,
                  CollisionInfo &info);

/**
 * @brief Dispatches to the appropriate shape-pair test based on collider types.
 *
 * Supports Box-Box, Sphere-Sphere, and Box-Sphere pairs.
 * Returns false for unsupported combinations.
 */
bool testCollision(const ColliderComponent &colliderA,
                   const TransformComponent &transformA,
                   const ColliderComponent &colliderB,
                   const TransformComponent &transformB, CollisionInfo &info);