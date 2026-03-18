#pragma once
#include "component_array.hpp"
#include "entity.hpp"
#include <memory>
#include <unordered_map>

class ComponentManager {
private:
  std::unordered_map<const char *, ComponentTypeID> componentTypes;
  std::unordered_map<const char *, std::shared_ptr<IComponentArray>>
      componentArrays;
  ComponentTypeID nextComponentType;

  template <typename T> std::shared_ptr<ComponentArray<T>> GetComponentArray();

public:
  ComponentManager() : nextComponentType(0) {
  }

  template <typename T> void RegisterComponent() {
    const char *typeName = typeid(T).name();

    assert(componentTypes.find(typeName) == componentTypes.end() &&
           "Registering component type more than once.");

    componentTypes.insert({typeName, nextComponentType});
    componentArrays.insert({typeName, std::make_shared<ComponentArray<T>>()});

    ++nextComponentType;
  }

  template <typename T> ComponentTypeID GetComponentType() {
    const char *typeName = typeid(T).name();

    assert(componentTypes.find(typeName) != componentTypes.end() &&
           "Component not registered before use.");

    return componentTypes[typeName];
  }

  template <typename T> void AddComponent(EntityID entity, T component) {
    GetComponentArray<T>()->InsertData(entity, component);
  }

  template <typename T> void RemoveComponent(EntityID entity) {
    GetComponentArray<T>()->RemoveData(entity);
  }

  template <typename T> T &GetComponent(EntityID entity) {
    return GetComponentArray<T>()->GetData(entity);
  }

  void EntityDestroyed(EntityID entity) {
    for (auto const &pair : componentArrays) {
      auto const &component = pair.second;

      component->EntityDestroyed(entity);
    }
  }
};
