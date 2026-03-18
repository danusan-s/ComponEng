#pragma once
#include "entity.hpp"
#include <array>
#include <assert.h>
#include <queue>

class EntityManager {
private:
  std::queue<EntityID> freeIDs;
  std::array<Signature, MAX_ENTITIES> entitySignatures;
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

    entitySignatures[id].reset();
    freeIDs.push(id);
    --livingEntityCount;
  }

  void SetSignature(EntityID id, Signature signature) {
    assert(id < MAX_ENTITIES && "Entity out of range.");

    entitySignatures[id] = signature;
  }

  Signature GetSignature(EntityID id) {
    assert(id < MAX_ENTITIES && "Entity out of range.");

    return entitySignatures[id];
  }
};
