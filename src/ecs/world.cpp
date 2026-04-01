#include "ecs/world.hpp"

void World::init() {
  m_componentRegistry = std::make_unique<ComponentRegistry>();
  m_entityManager = std::make_unique<EntityManager>();
  m_systemManager = std::make_unique<SystemManager>();
  m_archetypeManager = std::make_unique<ArchetypeManager>();
  time = 0.0f;
}

EntityID World::createEntity() {
  return m_entityManager->createEntity();
}

void World::destroyEntity(EntityID entity) {
  Archetype* currArchetype = m_archetypeManager->getBySignature(
      m_entityManager->getRecord(entity).signature);
  if (currArchetype) {
    currArchetype->removeEntity(entity);
  }
  m_entityManager->destroyEntity(entity);
}

void World::createSystems() {
  m_systemManager->createAll(this);
}

void World::updateSystems(float deltaTime) {
  m_systemManager->updateAll(this, deltaTime);
}

void World::destroySystems() {
  m_systemManager->destroyAll(this);
}
