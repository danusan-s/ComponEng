#pragma once

#include "ecs/archetype.hpp"
#include "ecs/entity.hpp"
#include <stdexcept>

class ArchetypeManager {
private:
  std::array<Archetype, MAX_ARCHETYPES> m_archetypes;
  std::unordered_map<Signature, ArchetypeID> m_signatureToArchetypeID;
  size_t m_archetypeCount = 0;

public:
  Archetype& getOrCreate(const Signature& signature,
                         ComponentRegistry* componentRegistry) {
    if (signature == Signature(0)) {
      throw std::runtime_error("ArchetypeManager: signature cannot be empty");
    }

    auto it = m_signatureToArchetypeID.find(signature);
    if (it != m_signatureToArchetypeID.end()) {
      return m_archetypes[it->second];
    }

    if (m_archetypeCount >= MAX_ARCHETYPES) {
      throw std::runtime_error("ArchetypeManager: MAX_ARCHETYPES exceeded");
    }

    ArchetypeID newID = m_archetypeCount++;
    m_archetypes[newID].init(signature, componentRegistry);
    m_signatureToArchetypeID[signature] = newID;
    return m_archetypes[newID];
  }

  Archetype* getBySignature(const Signature& signature) {
    auto it = m_signatureToArchetypeID.find(signature);
    if (it != m_signatureToArchetypeID.end()) {
      return &m_archetypes[it->second];
    }
    return nullptr;
  }

  Archetype& getByID(ArchetypeID id) {
    if (id >= m_archetypeCount) {
      throw std::runtime_error("ArchetypeManager: Invalid ArchetypeID");
    }
    return m_archetypes[id];
  }

  std::array<Archetype, MAX_ARCHETYPES>& getArchetypes() {
    return m_archetypes;
  }
};
