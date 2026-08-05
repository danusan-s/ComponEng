#pragma once

#include "componeng/ecs/archetype/component_column.hpp"
#include "componeng/ecs/component_registry.hpp"
#include "componeng/ecs/entity.hpp"

#include <stdexcept>
#include <vector>

namespace componeng::ecs {

/**
 * @brief An archetype groups all entities that share the exact same set of
 * component types.
 *
 * Each archetype holds one ComponentColumn per component type, so iterating
 * over entities touches contiguous memory for each component — this is the
 * core cache-friendly data layout of the ECS.
 *
 * Entity-to-row mappings (m_entityToRow / m_rowToEntity) allow O(1) lookup
 * in both directions.
 */
class Archetype {
private:
  std::vector<ComponentColumn> m_columns;

  uint8_t m_indexMap[MAX_COMPONENTS] = {0};
  std::unordered_map<EntityID, size_t> m_entityToRow;
  std::unordered_map<size_t, EntityID> m_rowToEntity;

public:
  Signature m_signature;

  void init(Signature signature, ComponentRegistry &registry) {
    this->m_signature = signature;

    // build columns for each component in signature
    for (size_t i = 0; i < MAX_COMPONENTS; ++i) {
      if (signature.test(i)) {
        ComponentInfo &info = registry.getComponentInfo(i);
        m_indexMap[i] = m_columns.size();
        m_columns.emplace_back(info.size, info.alignment, info.destructor);
      }
    }
  }

  void addEntity(EntityID entity) {
    size_t row = m_columns.empty() ? 0 : m_columns[0].m_count;
    for (auto &col : m_columns) {
      col.pushBackEmpty();
    }
    m_entityToRow[entity] = row;
    m_rowToEntity[row] = entity;
  }

  /**
   * Swap and remove to last row for each component column, then erase the
   * entity from mappings.
   * @return the EntityID of the entity that was swapped into the removed
   * entity's row, or INVALID_ENTITY if no swap occurred.
   */
  EntityID removeEntity(EntityID entity) {
    auto it = m_entityToRow.find(entity);
    if (it == m_entityToRow.end()) {
      throw std::runtime_error("Archetype: Entity not found in archetype");
    }
    size_t row = it->second;
    size_t lastRow = m_columns[0].m_count - 1;

    if (row == lastRow) {
      for (auto &col : m_columns) {
        col.remove(row);
      }
      m_rowToEntity.erase(lastRow);
      m_entityToRow.erase(it);
      return INVALID_ENTITY;
    }

    EntityID lastRowEntity = m_rowToEntity[lastRow];
    for (auto &col : m_columns) {
      col.remove(row);
    }
    m_entityToRow[lastRowEntity] = row;
    m_rowToEntity.erase(lastRow);
    m_rowToEntity[row] = lastRowEntity;
    m_entityToRow.erase(it);
    return lastRowEntity;
  }

  ComponentColumn &getColumn(ComponentID c) {
    if (!m_signature.test(c)) {
      throw std::runtime_error("Archetype: Component not in archetype");
    }
    return m_columns[m_indexMap[c]];
  }

  template <typename T> T &get(ComponentID c, std::size_t row) {
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

} // namespace componeng::ecs
