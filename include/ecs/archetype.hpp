#pragma once

#include "ecs/component_registry.hpp"
#include "ecs/entity.hpp"
#include <cstring>
#include <vector>

class ComponentColumn {
private:
  std::vector<std::byte> m_componentArray;

public:
  size_t m_stride;
  size_t m_count;

  ComponentColumn(size_t componentSize) : m_count(0), m_stride(componentSize) {
  }

  void* at(std::size_t index) {
    return m_componentArray.data() + index * m_stride;
  }

  template <typename T> T& get(size_t row) {
    return *reinterpret_cast<T*>(at(row));
  }

  void addEmpty() {
    m_componentArray.resize(m_componentArray.size() + m_stride);
    ++m_count;
  }

  void remove(size_t row) {
    if (row >= m_count) {
      throw std::runtime_error("ComponentColumn: Invalid row index");
    }
    if (row < m_count - 1) {
      std::memcpy(at(row), at(m_count - 1),
                  m_stride);
    }
    --m_count;
  }
};

class Archetype {
private:
  std::vector<ComponentColumn> m_columns;

  uint8_t m_indexMap[MAX_COMPONENTS] = {0};
  std::unordered_map<EntityID, size_t> m_entityToRow;
  std::unordered_map<size_t, EntityID> m_rowToEntity;

public:
  Signature m_signature;

  void init(Signature signature, ComponentRegistry* registry) {
    this->m_signature = signature;

    for (size_t i = 0; i < MAX_COMPONENTS; ++i) {
      if (signature.test(i)) {
        ComponentInfo& info = registry->getComponentInfo(i);
        m_indexMap[i] = m_columns.size();
        m_columns.emplace_back(info.size);
      }
    }
  }

  void addEntity(EntityID entity) {
    size_t row = m_columns.empty() ? 0 : m_columns[0].m_count;
    for (auto& col : m_columns) {
      col.addEmpty();
    }
    m_entityToRow[entity] = row;
    m_rowToEntity[row] = entity;
  }

  EntityID removeEntity(EntityID entity) {
    auto it = m_entityToRow.find(entity);
    EntityID lastRowEntity = m_rowToEntity[m_columns[0].m_count - 1];
    m_rowToEntity.erase(m_columns[0].m_count - 1);

    if (it == m_entityToRow.end()) {
      throw std::runtime_error("Archetype: Entity not found in archetype");
    }
    size_t row = it->second;
    for (auto& col : m_columns) {
      col.remove(row);
    }
    m_entityToRow.erase(it);
    m_rowToEntity[row] = lastRowEntity;
    return lastRowEntity;
  }

  ComponentColumn& getColumn(ComponentID c) {
    if (!m_signature.test(c)) {
      throw std::runtime_error("Archetype: Component not in archetype");
    }
    return m_columns[m_indexMap[c]];
  }

  template <typename T> T& get(ComponentID c, std::size_t row) {
    return getColumn(c).get<T>(row);
  }

  size_t getRowForEntity(EntityID id) {
    if (m_entityToRow.find(id) == m_entityToRow.end()) {
      throw std::runtime_error("Archetype: Entity not found in archetype");
    }
    return m_entityToRow[id];
  }

  EntityID getEntityForRow(size_t row) {
    if (m_rowToEntity.find(row) == m_rowToEntity.end()) {
      throw std::runtime_error("Archetype: Row index out of bounds");
    }
    return m_rowToEntity[row];
  }

  size_t getEntityCount() const {
    return m_columns.empty() ? 0 : m_columns[0].m_count;
  }
};
