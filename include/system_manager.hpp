#pragma once
#include "entity.hpp"
#include <assert.h>
#include <memory>
#include <set>
#include <unordered_map>

class System {
public:
  std::set<EntityID> entities;
};

class SystemManager {
private:
  std::unordered_map<const char *, std::shared_ptr<System>> systems;
  std::unordered_map<const char *, Signature> systemSignatures;

public:
  template <typename T> std::shared_ptr<T> RegisterSystem() {
    const char *typeName = typeid(T).name();

    assert(systems.find(typeName) == systems.end() &&
           "Registering system more than once.");

    auto system = std::make_shared<T>();
    systems.insert({typeName, system});
    return system;
  }

  template <typename T> void SetSignature(Signature signature) {
    const char *typeName = typeid(T).name();

    assert(systems.find(typeName) != systems.end() &&
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
};
