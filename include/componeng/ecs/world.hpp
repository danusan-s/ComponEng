#pragma once

#include "componeng/ecs/archetype.hpp"
#include "componeng/ecs/archetype_manager.hpp"
#include "componeng/ecs/component_registry.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/ecs/entity_manager.hpp"
#include "componeng/ecs/query.hpp"
#include "componeng/ecs/system_manager.hpp"
#include "componeng/ecs/thread_pool.hpp"
#include "componeng/events/event_bus.hpp"
#include "componeng/renderer/api/irender_device.hpp"
#include "componeng/resources/action_state.hpp"
#include "componeng/resources/input_state.hpp"
#include "componeng/resources/resource_manager.hpp"
#include "componeng/utils/logger.hpp"
#include <memory>

namespace componeng::ecs {

/**
 * @brief Central ECS world that ties together all subsystems.
 *
 * Provides the public API for entity/component lifecycle management,
 * system registration/execution, singleton components, and typed queries.
 * Internally delegates to EntityManager, ComponentRegistry, ArchetypeManager,
 * SystemManager, and ThreadPool.
 */
class World {
private:
  ComponentRegistry m_componentRegistry;
  EntityManager m_entityManager;
  SystemManager m_systemManager;
  ArchetypeManager m_archetypeManager;
  ThreadPool m_threadPool;
  events::EventBus m_eventBus;
  resources::ResourceManager m_resourceManager;

  void *m_windowHandle = nullptr;
  renderer::api::IRenderDevice *m_renderDevice = nullptr;

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
    auto &inputState = get_resource<resources::InputState>();
    auto &actionState = get_resource<resources::ActionState>();
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

    if (!record.signature.test(componentID)) {
      throw std::runtime_error("Entity does not have component");
    }

    Archetype *archetype = m_archetypeManager.getBySignature(record.signature);
    return archetype->get<T>(componentID, record.row);
  }

  template <typename T> void addComponent(EntityID entity, T component) {
    addComponents(entity, std::move(component));
  }

  template <typename... Ts>
  void addComponents(EntityID entity, Ts &&...components) {
    EntityRecord &record = m_entityManager.getRecord(entity);

    Signature oldSig = record.signature;
    Archetype *oldArchetype = m_archetypeManager.getBySignature(oldSig);

    if ((oldSig & m_componentRegistry.makeSignature<Ts...>()).any()) {
      throw std::runtime_error("Entity already has component");
    }

    Signature newSig = oldSig;
    newSig = oldSig | m_componentRegistry.makeSignature<Ts...>();

    Archetype &newArchetype =
        m_archetypeManager.getOrCreate(newSig, m_componentRegistry);
    newArchetype.addEntity(entity);

    std::size_t newRow = newArchetype.getRowForEntity(entity);

    if (oldArchetype == nullptr) {
      LOG_INFO("Adding first component to entity %llu",
               (unsigned long long)entity);
    } else {
      LOG_INFO("Moving entity %llu to new archetype %s from old archetype %s",
               (unsigned long long)entity, newSig.to_string().c_str(),
               oldSig.to_string().c_str());
    }

    if (oldArchetype) {
      std::size_t oldRow = record.row;
      for (ComponentID c = 0; c < MAX_COMPONENTS; ++c) {
        if (!oldSig.test(c))
          continue;
        auto &src = oldArchetype->getColumn(c);
        auto &dst = newArchetype.getColumn(c);
        std::memcpy(dst.at(newRow), src.at(oldRow), src.m_stride);
      }
      EntityID moved = oldArchetype->removeEntity(entity);
      if (moved != entity) {
        m_entityManager.getRecord(moved).row = oldRow;
      }
    }

    (new (newArchetype.getColumn(m_componentRegistry.getComponentID<Ts>())
              .at(newRow)) Ts(std::forward<Ts>(components)),
     ...);

    record.signature = newSig;
    record.row = newRow;
  }

  template <typename T> void removeComponent(EntityID entity) {
    ComponentID componentID = m_componentRegistry.getComponentID<T>();
    EntityRecord &record = m_entityManager.getRecord(entity);

    Signature oldSig = record.signature;
    Archetype *oldArchetype = m_archetypeManager.getBySignature(oldSig);

    if (!oldSig.test(componentID)) {
      throw std::runtime_error("Entity does not have component");
    }

    Signature newSig = oldSig;
    newSig.reset(componentID);

    Archetype &newArchetype =
        m_archetypeManager.getOrCreate(newSig, m_componentRegistry);
    newArchetype.addEntity(entity);
    std::size_t newRow = newArchetype.getRowForEntity(entity);

    std::size_t oldRow = record.row;
    for (ComponentID c = 0; c < MAX_COMPONENTS; ++c) {
      if (!newSig.test(c))
        continue;
      auto &src = oldArchetype->getColumn(c);
      auto &dst = newArchetype.getColumn(c);
      std::memcpy(dst.at(newRow), src.at(oldRow), src.m_stride);
    }
    EntityID moved = oldArchetype->removeEntity(entity);
    if (moved != entity) {
      m_entityManager.getRecord(moved).row = oldRow;
    }

    record.signature = newSig;
    record.row = newRow;
  }

  template <typename T> bool hasComponent(EntityID entity) {
    ComponentID componentID = m_componentRegistry.getComponentID<T>();
    EntityRecord &record = m_entityManager.getRecord(entity);
    return record.signature.test(componentID);
  }

  template <typename T> void set_resource(T &&value) {
    m_resourceManager.set<T>(std::forward<T>(value));
  }

  template <typename T> T &get_resource() {
    return m_resourceManager.get<T>();
  }

  template <typename T> void emit_event(const T &event) {
    m_eventBus.emit<T>(event);
  }

  template <typename T> const std::vector<T> &get_events() {
    return m_eventBus.getEvents<T>();
  }

  template <typename... Ts> Query<Ts...> query() {
    std::array<Archetype, MAX_ARCHETYPES> &archetypes =
        m_archetypeManager.getArchetypes();
    return Query<Ts...>(archetypes, m_componentRegistry);
  }

  template <typename T>
  std::shared_ptr<T>
  registerSystem(SystemGroup group = SystemGroup::Simulation) {
    return m_systemManager.registerSystem<T>(group);
  }
};

} // namespace componeng::ecs
