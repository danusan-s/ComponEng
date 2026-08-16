#pragma once

#include "componeng/ecs/archetype/archetype.hpp"
#include "componeng/ecs/archetype/archetype_manager.hpp"
#include "componeng/ecs/component_registry.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/ecs/entity_manager.hpp"
#include "componeng/ecs/query.hpp"
#include "componeng/ecs/resource_manager.hpp"
#include "componeng/ecs/system_manager.hpp"
#include "componeng/ecs/thread_pool.hpp"
#include "componeng/events/event_bus.hpp"
#include "componeng/input/action_state.hpp"
#include "componeng/input/input_state.hpp"
#include "componeng/renderer/backend/api/irender_device.hpp"
#include "componeng/utils/logger.hpp"

#include <memory>

namespace componeng::ecs {

class SceneSerializer;

/**
 * @brief Central ECS world that ties together all subsystems.
 *
 * Provides the public API for entity/component lifecycle management,
 * system registration/execution, singleton components, and typed queries.
 * Internally delegates to EntityManager, ComponentRegistry, ArchetypeManager,
 * SystemManager, and ThreadPool.
 */
class World {
  friend class SceneSerializer;

private:
  ComponentRegistry m_componentRegistry;
  EntityManager m_entityManager;
  SystemManager m_systemManager;
  ArchetypeManager m_archetypeManager;
  ThreadPool m_threadPool;
  events::EventBus m_eventBus;
  ResourceManager m_resourceManager;

  void *m_windowHandle = nullptr;
  renderer::api::IRenderDevice *m_renderDevice = nullptr;

  /** @return the entity's archetype, or nullptr if it has no components. */
  Archetype *archetypeOf(const EntityRecord &record) {
    return record.archetypeId == INVALID_ARCHETYPE
               ? nullptr
               : &m_archetypeManager.getByID(record.archetypeId);
  }

  /** @return the entity's component signature; empty if it has none. */
  Signature signatureOf(const EntityRecord &record) {
    Archetype *archetype = archetypeOf(record);
    return archetype ? archetype->m_signature : Signature{};
  }

  void migrateEntityToArchetype(std::size_t oldRow, Signature oldSig,
                                Signature newSig, Archetype *oldArchetype,
                                Archetype &newArchetype, std::size_t newRow) {
    if (!oldArchetype)
      return;

    const Signature carried = oldSig & newSig;
    for (ComponentID c = 0; c < MAX_COMPONENTS; ++c) {
      if (!carried.test(c))
        continue;
      auto &src = oldArchetype->getColumn(c);
      auto &dst = newArchetype.getColumn(c);
      std::memcpy(dst.at(newRow), src.at(oldRow), src.m_stride);
    }

    EntityID moved = oldArchetype->removeEntityAtRow(oldRow);
    if (moved != INVALID_ENTITY) {
      m_entityManager.getRecord(moved).row = oldRow;
    }
  }

public:
  double time = 0.0f;

  void init();
  EntityID createEntity();
  void destroyEntity(EntityID entity);
  void createSystems();
  void updateSystems(float deltaTime);
  void destroySystems();
  ThreadPool &threadPool();
  renderer::api::IRenderDevice &getRenderDevice();
  void setWindowHandle(void *handle);
  void *getWindowHandle() const;

  void swapInputBuffers() {
    auto &inputState = getResource<input::InputState>();
    auto &actionState = getResource<input::ActionState>();
    inputState.previous_state = inputState.current_state;
    actionState.swapBuffers();
  }

  template <typename T> void registerComponent() {
    m_componentRegistry.registerComponent<T>();
  }

  template <typename... Components> void registerComponents() {
    (this->registerComponent<Components>(), ...);
  }

  template <typename T> T &getComponent(EntityID entity) {
    EntityRecord &record = m_entityManager.getRecord(entity);
    ComponentID componentID = m_componentRegistry.getComponentID<T>();

    Archetype *archetype = archetypeOf(record);
    if (!archetype || !archetype->m_signature.test(componentID)) {
      throw std::runtime_error("Entity does not have component");
    }

    return archetype->get<T>(componentID, record.row);
  }

  template <typename T> void addComponent(EntityID entity, T component) {
    addComponents(entity, std::move(component));
  }

  template <typename... Ts>
  void addComponents(EntityID entity, Ts &&...components) {
    EntityRecord &record = m_entityManager.getRecord(entity);

    Archetype *oldArchetype = archetypeOf(record);
    Signature oldSig = oldArchetype ? oldArchetype->m_signature : Signature{};

    if ((oldSig & m_componentRegistry.makeSignature<Ts...>()).any()) {
      throw std::runtime_error("Entity already has component");
    }

    Signature newSig = oldSig | m_componentRegistry.makeSignature<Ts...>();

    ArchetypeID newArchetypeId =
        m_archetypeManager.getOrCreate(newSig, m_componentRegistry);
    Archetype &newArchetype = m_archetypeManager.getByID(newArchetypeId);
    std::size_t newRow = newArchetype.addEntity(entity);

    if (oldArchetype == nullptr) {
      LOG_INFO("Adding first component to entity %llu",
               static_cast<unsigned long long>(entity));
    } else {
      LOG_INFO("Moving entity %llu to new archetype %s from old archetype %s",
               static_cast<unsigned long long>(entity),
               newSig.to_string().c_str(), oldSig.to_string().c_str());
    }

    migrateEntityToArchetype(record.row, oldSig, newSig, oldArchetype,
                             newArchetype, newRow);

    (new (newArchetype.getColumn(m_componentRegistry.getComponentID<Ts>())
              .at(newRow)) Ts(std::forward<Ts>(components)),
     ...);

    record.archetypeId = newArchetypeId;
    record.row = static_cast<std::uint32_t>(newRow);
  }

  void addComponentById(EntityID entity, ComponentID componentID,
                        const void *componentData) {
    EntityRecord &record = m_entityManager.getRecord(entity);

    Archetype *oldArchetype = archetypeOf(record);
    Signature oldSig = oldArchetype ? oldArchetype->m_signature : Signature{};

    if (oldSig.test(componentID)) {
      throw std::runtime_error("Entity already has component");
    }

    Signature newSig = oldSig;
    newSig.set(componentID);

    ArchetypeID newArchetypeId =
        m_archetypeManager.getOrCreate(newSig, m_componentRegistry);
    Archetype &newArchetype = m_archetypeManager.getByID(newArchetypeId);
    std::size_t newRow = newArchetype.addEntity(entity);

    const ComponentInfo &info =
        m_componentRegistry.getComponentInfo(componentID);

    migrateEntityToArchetype(record.row, oldSig, newSig, oldArchetype,
                             newArchetype, newRow);

    std::memcpy(newArchetype.getColumn(componentID).at(newRow), componentData,
                info.size);

    record.archetypeId = newArchetypeId;
    record.row = static_cast<std::uint32_t>(newRow);
  }

  template <typename T> void removeComponent(EntityID entity) {
    ComponentID componentID = m_componentRegistry.getComponentID<T>();
    EntityRecord &record = m_entityManager.getRecord(entity);

    Archetype *oldArchetype = archetypeOf(record);
    Signature oldSig = oldArchetype ? oldArchetype->m_signature : Signature{};

    if (!oldSig.test(componentID)) {
      throw std::runtime_error("Entity does not have component");
    }

    Signature newSig = oldSig;
    newSig.reset(componentID);

    // Dropping the last component leaves no signature, and an empty signature
    // has no archetype -- the entity simply stops belonging to one.
    if (newSig.none()) {
      EntityID moved = oldArchetype->removeEntityAtRow(record.row);
      if (moved != INVALID_ENTITY) {
        m_entityManager.getRecord(moved).row = record.row;
      }
      record.archetypeId = INVALID_ARCHETYPE;
      record.row = 0;
      return;
    }

    ArchetypeID newArchetypeId =
        m_archetypeManager.getOrCreate(newSig, m_componentRegistry);
    Archetype &newArchetype = m_archetypeManager.getByID(newArchetypeId);
    std::size_t newRow = newArchetype.addEntity(entity);

    migrateEntityToArchetype(record.row, oldSig, newSig, oldArchetype,
                             newArchetype, newRow);

    record.archetypeId = newArchetypeId;
    record.row = static_cast<std::uint32_t>(newRow);
  }

  template <typename T> bool hasComponent(EntityID entity) {
    ComponentID componentID = m_componentRegistry.getComponentID<T>();
    EntityRecord &record = m_entityManager.getRecord(entity);
    Archetype *archetype = archetypeOf(record);
    return archetype && archetype->m_signature.test(componentID);
  }

  template <typename T> void setResource(T &&value) {
    m_resourceManager.set<T>(std::forward<T>(value));
  }

  template <typename T> T &getResource() {
    return m_resourceManager.get<T>();
  }

  template <typename T> bool hasResource() {
    return m_resourceManager.contains<T>();
  }

  template <typename T> void emitEvent(const T &event) {
    m_eventBus.emit<T>(event);
  }

  template <typename T> const std::vector<T> &getEvents() {
    return m_eventBus.getEvents<T>();
  }

  template <typename... Ts> Query<Ts...> query() {
    std::array<Archetype, MAX_ARCHETYPES> &archetypes =
        m_archetypeManager.getArchetypes();
    return Query<Ts...>(archetypes, m_archetypeManager.count(),
                        m_componentRegistry);
  }

  template <typename T>
  std::shared_ptr<T>
  registerSystem(SystemGroup group = SystemGroup::Simulation) {
    return m_systemManager.registerSystem<T>(group);
  }

  template <typename T, typename... Ts> void addSystemDependencies() {
    m_systemManager.addSystemDependencies<T, Ts...>();
  }
};

} // namespace componeng::ecs
