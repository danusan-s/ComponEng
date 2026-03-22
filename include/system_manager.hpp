#pragma once
#include "system.hpp"
#include <assert.h>
#include <memory>
#include <vector>

class SystemManager {
private:
  std::vector<std::pair<const char *, std::shared_ptr<System>>> systems;

  size_t find(const char *typeName) {
    for (size_t i = 0; i < systems.size(); ++i) {
      if (systems[i].first == typeName) {
        return i;
      }
    }
    return systems.size();
  }

public:
  template <typename T> std::shared_ptr<T> RegisterSystem(World &world) {
    const char *typeName = typeid(T).name();

    assert(find(typeName) == systems.size() &&
           "Registering system more than once.");

    auto system = std::make_shared<T>();
    system->Init(world);
    systems.emplace_back(typeName, system);
    return system;
  }

  void UpdateAll(float deltaTime) {
    for (auto const &pair : systems) {
      auto const &system = pair.second;
      system->Update(deltaTime);
    }
  }

  void ShutdownAll() {
    for (auto const &pair : systems) {
      auto const &system = pair.second;
      system->Shutdown();
    }
  }
};
