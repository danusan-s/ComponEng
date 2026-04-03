#pragma once

#include "core/logger.hpp"
#include "ecs/archetype.hpp"
#include "ecs/archetype_manager.hpp"
#include "ecs/component_registry.hpp"
#include "ecs/entity.hpp"
#include "ecs/entity_manager.hpp"
#include "ecs/query.hpp"
#include "ecs/system_manager.hpp"
#include "ecs/thread_pool.hpp"
#include <memory>

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
  std::unique_ptr<ComponentRegistry> m_componentRegistry;
  std::unique_ptr<EntityManager> m_entityManager;
  std::unique_ptr<SystemManager> m_systemManager;
  std::unique_ptr<ArchetypeManager> m_archetypeManager;
  std::unique_ptr<ThreadPool> m_threadPool;

  std::unordered_map<ComponentID, std::vector<uint8_t>> m_singletons;

public:
  double time = 0.0f;

  void init();
  EntityID createEntity();
  void destroyEntity(EntityID entity);
  void createSystems();
  void updateSystems(float deltaTime);
  void destroySystems();

  template <typename T> void registerComponent() {
    m_componentRegistry->registerComponent<T>();
  }

  template <typename... Components> void registerComponents() {
    (this->registerComponent<Components>(), ...);
  }

  template <typename T> T &getComponent(EntityID entity) {
    EntityRecord &record = m_entityManager->getRecord(entity);
    ComponentID componentID = m_componentRegistry->getComponentID<T>();

    if (!record.signature.test(componentID)) {
      throw std::runtime_error("Entity does not have component");
    }

    Archetype *archetype = m_archetypeManager->getBySignature(record.signature);
    return archetype->get<T>(componentID, record.row);
  }

  template <typename T> void addComponent(EntityID entity, T &&component) {
    addComponents(entity, std::forward<T>(component));
  }

  template <typename... Ts>
  void addComponents(EntityID entity, Ts &&...components) {
    EntityRecord &record = m_entityManager->getRecord(entity);

    Signature oldSig = record.signature;
    Archetype *oldArchetype = m_archetypeManager->getBySignature(oldSig);

    if ((oldSig & m_componentRegistry->makeSignature<Ts...>()).any()) {
      throw std::runtime_error("Entity already has component");
    }

    Signature newSig = oldSig;
    newSig = oldSig | m_componentRegistry->makeSignature<Ts...>();

    Archetype &newArchetype =
        m_archetypeManager->getOrCreate(newSig, m_componentRegistry.get());
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
        m_entityManager->getRecord(moved).row = oldRow;
      }
    }

    (new (newArchetype.getColumn(m_componentRegistry->getComponentID<Ts>())
              .at(newRow)) Ts(std::forward<Ts>(components)),
     ...);

    record.signature = newSig;
    record.row = newRow;
  }

  template <typename T> void removeComponent(EntityID entity) {
    ComponentID componentID = m_componentRegistry->getComponentID<T>();
    EntityRecord &record = m_entityManager->getRecord(entity);

    Signature oldSig = record.signature;
    Archetype *oldArchetype = m_archetypeManager->getBySignature(oldSig);

    if (!oldSig.test(componentID)) {
      throw std::runtime_error("Entity does not have component");
    }

    Signature newSig = oldSig;
    newSig.reset(componentID);

    Archetype &newArchetype =
        m_archetypeManager->getOrCreate(newSig, m_componentRegistry.get());
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
      m_entityManager->getRecord(moved).row = oldRow;
    }

    record.signature = newSig;
    record.row = newRow;
  }

  template <typename T> bool hasComponent(EntityID entity) {
    ComponentID componentID = m_componentRegistry->getComponentID<T>();
    EntityRecord &record = m_entityManager->getRecord(entity);
    return record.signature.test(componentID);
  }

  template <typename T> T &getSingleton() {
    ComponentID componentID = m_componentRegistry->getComponentID<T>();
    auto it = m_singletons.find(componentID);
    if (it == m_singletons.end()) {
      throw std::runtime_error("Singleton not found");
    }
    return *reinterpret_cast<T *>(m_singletons[componentID].data());
  }

  template <typename T> void setSingleton(const T &value) {
    ComponentID componentID = m_componentRegistry->getComponentID<T>();
    std::vector<uint8_t> &storage = m_singletons[componentID];
    if (storage.empty()) {
      storage.resize(sizeof(T));
    }
    *reinterpret_cast<T *>(storage.data()) = value;
  }

  template <typename T> bool hasSingleton() {
    ComponentID componentID = m_componentRegistry->getComponentID<T>();
    return m_singletons.find(componentID) != m_singletons.end();
  }

  template <typename... Ts> Query<Ts...> query() {
    std::array<Archetype, MAX_ARCHETYPES> &archetypes =
        m_archetypeManager->getArchetypes();
    return Query<Ts...>(archetypes, *m_componentRegistry.get());
  }

  template <typename T>
  std::shared_ptr<T>
  registerSystem(SystemGroup group = SystemGroup::Simulation) {
    return m_systemManager->registerSystem<T>(group);
  }

  ThreadPool &threadPool() {
    return *m_threadPool;
  }

  void setWindowHandle(void* handle) { m_windowHandle = handle; }
  void* getWindowHandle() const { return m_windowHandle; }

private:
  void* m_windowHandle = nullptr;
};
