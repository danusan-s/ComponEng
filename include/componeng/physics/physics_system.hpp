#pragma once
#include "componeng/ecs/system.hpp"

/**
 * @brief Simulation-phase system that handles gravity, integration, and
 * collision response.
 *
 * Runs on a fixed 60Hz timestep with an accumulator. Uses the engine's
 * ThreadPool to parallelize gravity integration and broad-phase collision
 * detection.
 */
class PhysicsSystem : public ISystem {
public:
  void onUpdate(const SystemState &state) override;
};
