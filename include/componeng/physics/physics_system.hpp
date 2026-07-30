#pragma once
#include "componeng/core/types.hpp"
#include "componeng/ecs/system.hpp"

namespace componeng::physics {

/**
 * @brief Simulation-phase system that handles gravity, integration, and
 * collision response.
 *
 * Runs on a fixed 60Hz timestep with an accumulator. Uses the engine's
 * ThreadPool to parallelize gravity integration and broad-phase collision
 * detection.
 */
class PhysicsSystem : public ecs::ISystem {
public:
  core::Vec3 g_gravity = core::Vec3(0.0f, -9.81f, 0.0f);
  void onUpdate(const ecs::SystemState &state) override;
};

} // namespace componeng::physics
