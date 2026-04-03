#pragma once

// Forward declaration to avoid circular dependencies
class World;

/**
 * @brief Per-frame state passed to every system during its lifecycle callbacks.
 *
 * Contains a pointer to the World for entity queries and the elapsed
 * frame time (deltaTime) for time-based updates.
 */
struct SystemState {
  World *world;
  float deltaTime;
};

/**
 * @brief Abstract base class for all ECS systems.
 *
 * Systems contain the game logic and operate on entities via queries.
 * Each system is assigned to a SystemGroup which determines its
 * coarse execution order within the frame.
 */
class ISystem {
public:
  virtual ~ISystem() = default;
  virtual void onUpdate(const SystemState &state) = 0;
  virtual void onCreate(const SystemState &state) {
  }
  virtual void onDestroy(const SystemState &state) {
  }
};
