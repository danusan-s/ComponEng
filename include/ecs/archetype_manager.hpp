#pragma once

#include "ecs/archetype.hpp"
#include "ecs/entity.hpp"
#include <stdexcept>

class ArchetypeManager {
private:
  std::array<Archetype, MAX_ARCHETYPES> archetypes;
  std::unordered_map<Signature, ArchetypeID> signatureToArchetypeID;
  size_t archetypeCount = 0;

public:
  // Returns existing archetype or creates one for this signature.
  Archetype &getOrCreate(const Signature &signature,
                         ComponentRegistry *componentRegistry) {
    if (signature == Signature(0)) {
      throw std::runtime_error("ArchetypeManager: signature cannot be empty");
    }

    auto it = signatureToArchetypeID.find(signature);
    if (it != signatureToArchetypeID.end()) {
      return archetypes[it->second];
    }

    if (archetypeCount >= MAX_ARCHETYPES) {
      throw std::runtime_error("ArchetypeManager: MAX_ARCHETYPES exceeded");
    }

    ArchetypeID newID = archetypeCount++;
    archetypes[newID].Init(signature, componentRegistry);
    signatureToArchetypeID[signature] = newID;
    return archetypes[newID];
  }

  Archetype *getBySignature(const Signature &signature) {
    auto it = signatureToArchetypeID.find(signature);
    if (it != signatureToArchetypeID.end()) {
      return &archetypes[it->second];
    }
    return nullptr;
  }

  Archetype &getByID(ArchetypeID id) {
    if (id >= archetypeCount) {
      throw std::runtime_error("ArchetypeManager: Invalid ArchetypeID");
    }
    return archetypes[id];
  }

  std::array<Archetype, MAX_ARCHETYPES> &getArchetypes() {
    return archetypes;
  }
};
