#include "componeng/ecs/world.hpp"
#include "componeng/events/entity_event.hpp"

namespace componeng::ecs {

void World::init() {
  time = 0.0f;
}

EntityID World::createEntity() {
  EntityID entity = m_entityManager.createEntity();
  m_eventBus.emit<events::EntityCreateEvent>({.entity = entity});
  return entity;
}

void World::destroyEntity(EntityID entity) {
  m_eventBus.emit<events::EntityDestroyEvent>({.entity = entity});
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
  m_eventBus.swapBuffers();
}

void World::destroySystems() {
  m_systemManager.destroyAll(this);
}

EventBus &World::eventBus() {
  return m_eventBus;
}

ThreadPool &World::threadPool() {
  return m_threadPool;
}

void World::setWindowHandle(void *handle) {
  m_windowHandle = handle;
}

void *World::getWindowHandle() const {
  return m_windowHandle;
}

void World::setRenderDevice(renderer::api::IRenderDevice *device) {
  m_renderDevice = device;
}

renderer::api::IRenderDevice *World::getRenderDevice() const {
  return m_renderDevice;
}

} // namespace componeng::ecs
