#pragma once

#include "entity.hpp"
#include <array>
#include <assert.h>
#include <queue>

/**
 * @brief Metadata stored per entity: its archetype row and component signature.
 */
struct EntityRecord {
  std::size_t row;
  Signature signature;
};

/**
 * @brief Manages entity creation, destruction, and ID recycling.
 *
 * Maintains a pool of free entity IDs and a fixed-size array of EntityRecords.
 * When an entity is destroyed its ID is returned to the free pool for reuse.
 */
class EntityManager {
private:
  std::queue<EntityID> m_freeIDs;
  std::array<EntityRecord, MAX_ENTITIES> m_entityRecords;
  uint32_t m_livingEntityCount;

public:
  EntityManager() : m_livingEntityCount(0) {
    for (EntityID id = 1; id < MAX_ENTITIES; ++id) {
      m_freeIDs.push(id);
    }
  }

  EntityID createEntity() {
    assert(m_livingEntityCount < MAX_ENTITIES &&
           "Too many entities in existence.");

    EntityID id = m_freeIDs.front();
    m_freeIDs.pop();
    ++m_livingEntityCount;

    return id;
  }

  void destroyEntity(EntityID id) {
    assert(id < MAX_ENTITIES && "Entity out of range.");

    EntityRecord &record = m_entityRecords[id];
    record.row = 0;
    record.signature.reset();
    m_freeIDs.push(id);
    --m_livingEntityCount;
  }

  EntityRecord &getRecord(EntityID id) {
    assert(id < MAX_ENTITIES && "Entity out of range.");

    return m_entityRecords[id];
  }
};
