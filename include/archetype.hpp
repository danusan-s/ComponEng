#pragma once

#include "component_registry.hpp"
#include "entity.hpp"
#include <cstring>
#include <vector>

class ComponentColumn {
private:
  std::vector<std::byte> componentArray; // raw byte array to store components

public:
  size_t stride;
  size_t count;

  ComponentColumn(size_t componentSize) : count(0), stride(componentSize) {
    componentArray.resize(stride * MAX_ENTITIES);
  }

  void *at(std::size_t index) {
    return componentArray.data() + index * stride;
  }

  template <typename T> T &Get(size_t row) {
    return *reinterpret_cast<T *>(at(row));
  }

  void AddEmpty() {
    componentArray.resize(componentArray.size() + stride);
  }

  void Remove(size_t row) {
    if (row >= count) {
      throw std::runtime_error("ComponentColumn: Invalid row index");
    }
    if (row < count - 1) {
      std::memcpy(at(row), at(count - 1),
                  stride); // move last component to removed spot
    }
    --count;
  }
};

class Archetype {
private:
  std::vector<ComponentColumn> componentColumns;

  uint8_t indexMap[MAX_COMPONENTS] = {
      0}; // maps ComponentID to column index in componentColumns
  std::unordered_map<EntityID, size_t>
      entityToRow; // maps EntityID to row index in columns
  std::unordered_map<size_t, EntityID>
      rowToEntity; // maps row index to EntityID for reverse lookup

public:
  ArchetypeID id;
  Signature signature;

  Archetype(ArchetypeID id, Signature signature, ComponentRegistry &registry)
      : id(id), signature(signature) {
    for (size_t i = 0; i < MAX_COMPONENTS; ++i) {
      if (signature.test(i)) {
        ComponentInfo &info =
            registry.GetComponentInfo(i); // validate component ID
        indexMap[i] = componentColumns.size();
        componentColumns.emplace_back(info.size);
      }
    }
  }

  void AddEntity(EntityID entity) {
    size_t row = componentColumns.empty() ? 0 : componentColumns[0].count;
    for (auto col : componentColumns) {
      col.AddEmpty();
    }
    entityToRow[entity] = row;
    rowToEntity[row] = entity;
  }

  EntityID RemoveEntity(EntityID entity) {
    auto it = entityToRow.find(entity);
    EntityID lastRowEntity = rowToEntity[componentColumns[0].count - 1];
    rowToEntity.erase(componentColumns[0].count - 1);

    if (it == entityToRow.end()) {
      throw std::runtime_error("Archetype: Entity not found in archetype");
    }
    size_t row = it->second;
    for (auto col : componentColumns) {
      col.Remove(row);
    }
    entityToRow.erase(it);
    rowToEntity[row] = lastRowEntity;
    return lastRowEntity;
  }

  ComponentColumn &GetColumn(ComponentID c) {
    if (!signature.test(c)) {
      throw std::runtime_error("Archetype: Component not in archetype");
    }
    return componentColumns[indexMap[c]];
  }

  template <typename T> T &Get(ComponentID c, std::size_t row) {
    return GetColumn(c).Get<T>(row);
  }

  size_t GetRowForEntity(EntityID id) {
    if (entityToRow.find(id) == entityToRow.end()) {
      throw std::runtime_error("Archetype: Entity not found in archetype");
    }
    return entityToRow[id];
  }

  size_t GetEntityCount() const {
    return componentColumns.empty() ? 0 : componentColumns[0].count;
  }
};
