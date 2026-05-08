#include "componeng/ecs/world.hpp"

namespace componeng::ecs {

void World::init() {
  time = 0.0f;
}

EntityID World::createEntity() {
  return m_entityManager.createEntity();
}

void World::destroyEntity(EntityID entity) {
  Archetype *currArchetype = m_archetypeManager.getBySignature(
      m_entityManager.getRecord(entity).signature);
  if (currArchetype) {
    currArchetype->removeEntity(entity);
  }
  m_entityManager.destroyEntity(entity);
}

void World::createSystems() {
  m_systemManager.createAll(this);
}

void World::updateSystems(float deltaTime) {
  m_systemManager.updateAll(this, deltaTime);
}

void World::destroySystems() {
  m_systemManager.destroyAll(this);
}

} // namespace componeng::ecs
