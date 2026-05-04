#pragma once

#include "componeng/ecs/component_registry.hpp"
#include "componeng/ecs/entity.hpp"
#include <cstdlib>
#include <cstring>
#include <vector>

/**
 * @brief A growable, aligned block of raw bytes.
 *
 * Allocates memory via std::aligned_alloc to guarantee proper alignment for
 * extended-alignment types. Supports manual growth, move semantics, and
 * disallows copies.
 */
class AlignedBuffer {
private:
  std::byte *m_data = nullptr;
  size_t m_size = 0;
  size_t m_capacity = 0;
  size_t m_alignment;

public:
  explicit AlignedBuffer(size_t alignment) : m_alignment(alignment) {
  }

  ~AlignedBuffer() {
    std::free(m_data);
  }

  AlignedBuffer(AlignedBuffer &&other) noexcept
      : m_data(other.m_data), m_size(other.m_size),
        m_capacity(other.m_capacity), m_alignment(other.m_alignment) {
    other.m_data = nullptr;
    other.m_size = 0;
    other.m_capacity = 0;
  }

  AlignedBuffer &operator=(AlignedBuffer &&other) noexcept {
    if (this != &other) {
      std::free(m_data);
      m_data = other.m_data;
      m_size = other.m_size;
      m_capacity = other.m_capacity;
      m_alignment = other.m_alignment;
      other.m_data = nullptr;
      other.m_size = 0;
      other.m_capacity = 0;
    }
    return *this;
  }

  AlignedBuffer(const AlignedBuffer &) = delete;
  AlignedBuffer &operator=(const AlignedBuffer &) = delete;

  void reserve(size_t newCapacity) {
    if (newCapacity <= m_capacity)
      return;
    size_t allocSize = newCapacity;
    std::byte *newData =
        static_cast<std::byte *>(std::aligned_alloc(m_alignment, allocSize));
    if (m_data) {
      std::memcpy(newData, m_data, m_size);
      std::free(m_data);
    }
    m_data = newData;
    m_capacity = newCapacity;
  }

  void resize(size_t newSize) {
    reserve(newSize);
    m_size = newSize;
  }

  void append(const void *src, size_t count) {
    size_t oldSize = m_size;
    resize(m_size + count);
    if (src) {
      std::memcpy(m_data + oldSize, src, count);
    }
  }

  std::byte *data() {
    return m_data;
  }
  const std::byte *data() const {
    return m_data;
  }
  size_t size() const {
    return m_size;
  }
  size_t capacity() const {
    return m_capacity;
  }
};

/**
 * @brief A contiguous, type-erased column storing one component type for all
 * entities in an archetype.
 *
 * Components are stored as raw bytes with a known stride (sizeof(T) rounded up
 * to a multiple of alignof(T)). Removal uses swap-remove for O(1) performance,
 * calling the stored destructor callback for non-trivial types to avoid
 * resource leaks.
 */
class ComponentColumn {
private:
  AlignedBuffer m_buffer;
  void (*m_destructor)(void *) = nullptr;

public:
  size_t m_stride;
  size_t m_count;

  ComponentColumn(size_t componentSize, size_t alignment,
                  void (*destructor)(void *) = nullptr)
      : m_buffer(alignment), m_count(0), m_stride(componentSize),
        m_destructor(destructor) {
    if (m_stride % alignment != 0) {
      m_stride = (m_stride / alignment + 1) * alignment;
    }
  }

  ~ComponentColumn() {
    if (m_destructor) {
      for (size_t i = 0; i < m_count; ++i) {
        m_destructor(at(i));
      }
    }
  }

  ComponentColumn(ComponentColumn &&) = default;
  ComponentColumn &operator=(ComponentColumn &&) = default;

  ComponentColumn(const ComponentColumn &) = delete;
  ComponentColumn &operator=(const ComponentColumn &) = delete;

  void *at(std::size_t index) {
    return m_buffer.data() + index * m_stride;
  }

  const void *at(std::size_t index) const {
    return m_buffer.data() + index * m_stride;
  }

  template <typename T> T &get(size_t row) {
    return *reinterpret_cast<T *>(at(row));
  }

  void pushBackEmpty() {
    if (m_count >= m_buffer.capacity() / m_stride) {
      size_t newCapacity =
          m_buffer.capacity() == 0 ? 8 : m_buffer.capacity() * 2;
      m_buffer.resize(newCapacity * m_stride);
    }
    ++m_count;
  }

  void remove(size_t row) {
    if (row >= m_count) {
      throw std::runtime_error("ComponentColumn: Invalid row index");
    }
    if (row < m_count - 1) {
      if (m_destructor)
        m_destructor(at(row));
      std::memcpy(at(row), at(m_count - 1), m_stride);
    }
    --m_count;
  }
};

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

  void init(Signature signature, ComponentRegistry *registry) {
    this->m_signature = signature;

    // build columns for each component in signature
    for (size_t i = 0; i < MAX_COMPONENTS; ++i) {
      if (signature.test(i)) {
        ComponentInfo &info = registry->getComponentInfo(i);
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
