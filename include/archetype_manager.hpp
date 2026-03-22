#pragma once

#include "archetype.hpp"
#include <stdexcept>
#include <vector>

class ArchetypeManager {
private:
  std::vector<Archetype> archetypes;
  std::unordered_map<Signature, ArchetypeID> signatureToArchetypeID;

public:
  // Returns existing archetype or creates one for this signature.
  Archetype &getOrCreate(const Signature &signature,
                         ComponentRegistry &componentRegistry) {
    auto it = signatureToArchetypeID.find(signature);
    if (it != signatureToArchetypeID.end()) {
      return archetypes[it->second];
    }

    if (archetypes.size() >= MAX_ARCHETYPES) {
      throw std::runtime_error("ArchetypeManager: MAX_ARCHETYPES exceeded");
    }

    ArchetypeID newID = archetypes.size();
    archetypes.emplace_back(newID, signature, componentRegistry);
    signatureToArchetypeID[signature] = newID;
    return archetypes.back();
  }

  Archetype *getBySignature(const Signature &signature) {
    auto it = signatureToArchetypeID.find(signature);
    if (it != signatureToArchetypeID.end()) {
      return &archetypes[it->second];
    }
    return nullptr;
  }

  Archetype &getByID(ArchetypeID id) {
    if (id >= archetypes.size()) {
      throw std::runtime_error("ArchetypeManager: Invalid ArchetypeID");
    }
    return archetypes[id];
  }

  std::vector<Archetype> &getArchetypes() {
    return archetypes;
  }

  std::size_t archetypeCount() const {
    return archetypes.size();
  }
};
