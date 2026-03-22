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
  std::queue<EntityID> freeIDs;
  std::array<EntityRecord, MAX_ENTITIES> entityRecords;
  uint32_t livingEntityCount;

public:
  EntityManager() : livingEntityCount(0) {
    for (EntityID id = 0; id < MAX_ENTITIES; ++id) {
      freeIDs.push(id);
    }
  }

  EntityID CreateEntity() {
    assert(livingEntityCount < MAX_ENTITIES &&
           "Too many entities in existence.");

    EntityID id = freeIDs.front();
    freeIDs.pop();
    ++livingEntityCount;

    return id;
  }

  void DestroyEntity(EntityID id) {
    assert(id < MAX_ENTITIES && "Entity out of range.");

    EntityRecord &record = entityRecords[id];
    record.row = 0;
    record.signature.reset();
    freeIDs.push(id);
    --livingEntityCount;
  }

  EntityRecord &GetRecord(EntityID id) {
    assert(id < MAX_ENTITIES && "Entity out of range.");

    return entityRecords[id];
  }
};
