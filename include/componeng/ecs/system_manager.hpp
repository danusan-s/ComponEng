#pragma once

#include "componeng/ecs/system.hpp"
#include <array>
#include <assert.h>
#include <memory>
#include <vector>

enum SystemGroup { Initialization, Simulation, Presentation };

constexpr SystemGroup GROUP_ORDER[] = {Initialization, Simulation,
                                       Presentation};
constexpr size_t NUM_GROUPS = sizeof(GROUP_ORDER) / sizeof(SystemGroup);

/**
 * @brief Lightweight record pairing a system's type name with its shared
 * pointer.
 */
struct SystemRecord {
  const char *typeName;
  std::shared_ptr<ISystem> system;
};

/**
 * @brief Scheduler that registers, groups, and executes ECS systems.
 *
 * Systems are organized into three fixed groups (Initialization, Simulation,
 * Presentation) and run sequentially each frame in that order.
 * Duplicate registration of the same system type is prevented at runtime.
 */
class SystemManager {
private:
  std::array<std::vector<SystemRecord>, NUM_GROUPS> m_systems;

  bool isRegistered(const char *typeName) {
    for (SystemGroup group : GROUP_ORDER) {
      auto &groupSystems = m_systems[group];
      for (size_t i = 0; i < groupSystems.size(); ++i) {
        if (groupSystems[i].typeName == typeName) {
          return true;
        }
      }
    }
    return false;
  }

public:
  template <typename T> std::shared_ptr<T> registerSystem(SystemGroup group) {
    const char *typeName = typeid(T).name();

    assert(!isRegistered(typeName) && "Registering system more than once.");

    auto system = std::make_shared<T>();
    m_systems[group].push_back({typeName, system});
    return system;
  }

  void createAll(World *world) {
    SystemState state{world, 0.0f};
    for (SystemGroup group : GROUP_ORDER) {
      auto &groupSystems = m_systems[group];
      for (auto const &sysRec : groupSystems) {
        auto const &system = sysRec.system;
        system->onCreate(state);
      }
    }
  }

  void updateAll(World *world, float deltaTime) {
    SystemState state{world, deltaTime};
    for (SystemGroup group : GROUP_ORDER) {
      auto &groupSystems = m_systems[group];
      for (auto const &sysRec : groupSystems) {
        auto const &system = sysRec.system;
        system->onUpdate(state);
      }
    }
  }

  void destroyAll(World *world) {
    SystemState state{world, 0.0f};
    for (SystemGroup group : GROUP_ORDER) {
      auto &groupSystems = m_systems[group];
      for (auto const &sysRec : groupSystems) {
        auto const &system = sysRec.system;
        system->onDestroy(state);
      }
    }
  }
};
