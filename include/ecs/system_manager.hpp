#pragma once
#include "ecs/system.hpp"
#include <array>
#include <assert.h>
#include <memory>
#include <vector>

enum SystemGroup { Initialization, Simulation, Presentation };

constexpr SystemGroup GROUP_ORDER[] = {Initialization, Simulation,
                                       Presentation};
constexpr size_t NUM_GROUPS = sizeof(GROUP_ORDER) / sizeof(SystemGroup);

struct SystemRecord {
  const char *typeName;
  std::shared_ptr<ISystem> system;
};

class SystemManager {
private:
  std::array<std::vector<SystemRecord>, NUM_GROUPS> systems;

  bool hasSystem(const char *typeName) {
    for (SystemGroup group : GROUP_ORDER) {
      auto &groupSystems = systems[group];
      for (size_t i = 0; i < groupSystems.size(); ++i) {
        if (groupSystems[i].typeName == typeName) {
          return true;
        }
      }
    }
    return false;
  }

public:
  template <typename T> std::shared_ptr<T> RegisterSystem(SystemGroup group) {
    const char *typeName = typeid(T).name();

    assert(!hasSystem(typeName) && "Registering system more than once.");

    auto system = std::make_shared<T>();
    systems[group].push_back({typeName, system});
    return system;
  }

  void CreateAll(World *world) {
    SystemState state{world, 0.0f};
    for (SystemGroup group : GROUP_ORDER) {
      auto &groupSystems = systems[group];
      for (auto const &sysRec : groupSystems) {
        auto const &system = sysRec.system;
        system->onCreate(state);
      }
    }
  }

  void UpdateAll(World *world, float deltaTime) {
    SystemState state{world, deltaTime};
    for (SystemGroup group : GROUP_ORDER) {
      auto &groupSystems = systems[group];
      for (auto const &sysRec : groupSystems) {
        auto const &system = sysRec.system;
        system->onUpdate(state);
      }
    }
  }

  void DestroyAll(World *world) {
    SystemState state{world, 0.0f};
    for (SystemGroup group : GROUP_ORDER) {
      auto &groupSystems = systems[group];
      for (auto const &sysRec : groupSystems) {
        auto const &system = sysRec.system;
        system->onDestroy(state);
      }
    }
  }
};
