#pragma once
#include "entity.hpp"
#include "system.hpp"
#include <assert.h>
#include <memory>
#include <unordered_map>
#include <vector>

class SystemManager {
private:
  std::vector<std::pair<const char *, std::shared_ptr<System>>> systems;
  std::unordered_map<const char *, Signature> systemSignatures;

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

  template <typename T> void SetSignature(Signature signature) {
    const char *typeName = typeid(T).name();

    assert(find(typeName) != systems.size() &&
           "System used before registered.");

    systemSignatures.insert({typeName, signature});
  }

  void EntityDestroyed(EntityID entity) {
    for (auto const &pair : systems) {
      auto const &system = pair.second;

      system->entities.erase(entity);
    }
  }

  void EntitySignatureChanged(EntityID entity, Signature entitySignature) {
    for (auto const &pair : systems) {
      auto const &type = pair.first;
      auto const &system = pair.second;
      auto const &systemSignature = systemSignatures[type];

      if ((entitySignature & systemSignature) == systemSignature) {
        system->entities.insert(entity);
      } else {
        system->entities.erase(entity);
      }
    }
  }

  void UpdateAll(float deltaTime) {
    for (auto const &pair : systems) {
      auto const &system = pair.second;
      system->Update(deltaTime);
    }
  }
};
