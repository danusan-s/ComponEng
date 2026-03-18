#pragma once
#include "entity.hpp"
#include <cassert>
#include <unordered_map>

class IComponentArray {
public:
  virtual ~IComponentArray() = default;
  virtual void EntityDestroyed(EntityID id) = 0;
};

template <typename T> class ComponentArray : public IComponentArray {
private:
  std::array<T, MAX_ENTITIES> componentArray;
  std::unordered_map<EntityID, size_t> entityToIndexMap;
  std::unordered_map<size_t, EntityID> indexToEntityMap;
  size_t size;

public:
  ComponentArray() : size(0) {
  }

  void InsertData(EntityID id, T component) {
    assert(entityToIndexMap.find(id) == entityToIndexMap.end() &&
           "Component added to same entity more than once.");

    size_t newIndex = size;
    entityToIndexMap[id] = newIndex;
    indexToEntityMap[newIndex] = id;
    componentArray[newIndex] = component;
    ++size;
  }

  void RemoveData(EntityID id) {
    assert(entityToIndexMap.find(id) != entityToIndexMap.end() &&
           "Removing non-existent component.");

    size_t indexOfRemovedEntity = entityToIndexMap[id];
    size_t indexOfLastElement = size - 1;
    componentArray[indexOfRemovedEntity] = componentArray[indexOfLastElement];

    EntityID entityOfLastElement = indexToEntityMap[indexOfLastElement];
    entityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
    indexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;

    entityToIndexMap.erase(id);
    indexToEntityMap.erase(indexOfLastElement);

    --size;
  }

  T &GetData(EntityID id) {
    assert(entityToIndexMap.find(id) != entityToIndexMap.end() &&
           "Retrieving non-existent component.");

    return componentArray[entityToIndexMap[id]];
  }

  void EntityDestroyed(EntityID id) override {
    if (entityToIndexMap.find(id) != entityToIndexMap.end()) {
      RemoveData(id);
    }
  }
};
