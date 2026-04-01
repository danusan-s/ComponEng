#pragma once

#include "entity.hpp"
#include <array>
#include <assert.h>
#include <queue>

struct EntityRecord {
  std::size_t row;
  Signature signature;
};

class EntityManager {
private:
  std::queue<EntityID> m_freeIDs;
  std::array<EntityRecord, MAX_ENTITIES> m_entityRecords;
  uint32_t m_livingEntityCount;

public:
  EntityManager() : m_livingEntityCount(0) {
    for (EntityID id = 0; id < MAX_ENTITIES; ++id) {
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

    EntityRecord& record = m_entityRecords[id];
    record.row = 0;
    record.signature.reset();
    m_freeIDs.push(id);
    --m_livingEntityCount;
  }

  EntityRecord& getRecord(EntityID id) {
    assert(id < MAX_ENTITIES && "Entity out of range.");

    return m_entityRecords[id];
  }
};
