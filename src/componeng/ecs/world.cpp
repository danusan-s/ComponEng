#include "componeng/ecs/world.hpp"

#include "componeng/events/entity_event.hpp"
#include "componeng/input/action_state.hpp"
#include "componeng/input/input_state.hpp"
#include "componeng/physics/physics_config.hpp"
#include "componeng/utils/logger.hpp"

namespace componeng::ecs {

void World::init() {
  setResource(input::InputState());
  setResource(input::ActionState());
  setResource(physics::PhysicsConfig());
  time = 0.0f;
}

EntityID World::createEntity() {
  EntityID entity = m_entityManager.createEntity();
  m_eventBus.emit<events::EntityCreateEvent>({.entity = entity});
  return entity;
}

void World::destroyEntity(EntityID entity) {
  m_eventBus.emit<events::EntityDestroyEvent>({.entity = entity});
  EntityRecord &record = m_entityManager.getRecord(entity);
  Archetype *currArchetype =
      m_archetypeManager.getBySignature(record.signature);
  if (currArchetype) {
    // Archetype storage is swap-remove: the entity previously occupying the
    // last row is moved into the freed row, so its record must be repointed
    // or every later access reads the wrong (or an out-of-bounds) row.
    std::size_t freedRow = record.row;
    EntityID moved = currArchetype->removeEntityAtRow(freedRow);
    if (moved != INVALID_ENTITY) {
      m_entityManager.getRecord(moved).row = freedRow;
    }
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

ThreadPool &World::threadPool() {
  return m_threadPool;
}

void World::setWindowHandle(void *handle) {
  m_windowHandle = handle;
}

void *World::getWindowHandle() const {
  return m_windowHandle;
}

renderer::api::IRenderDevice &World::getRenderDevice() {
  return *getResource<std::unique_ptr<renderer::api::IRenderDevice>>().get();
}

} // namespace componeng::ecs
