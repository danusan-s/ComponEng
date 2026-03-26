#pragma once
#include "core/logger.hpp"
#include "ecs/archetype.hpp"
#include "ecs/archetype_manager.hpp"
#include "ecs/component_registry.hpp"
#include "ecs/entity.hpp"
#include "ecs/entity_manager.hpp"
#include "ecs/query.hpp"
#include "ecs/system_manager.hpp"
#include <memory>

class World {
private:
  std::unique_ptr<ComponentRegistry> componentRegistry;
  std::unique_ptr<EntityManager> entityManager;
  std::unique_ptr<SystemManager> systemManager;
  std::unique_ptr<ArchetypeManager> archetypeManager;

  std::unordered_map<ComponentID, std::vector<uint8_t>> singletons;

public:
  float time = 0.0f;

  void Init() {
    componentRegistry = std::make_unique<ComponentRegistry>();
    entityManager = std::make_unique<EntityManager>();
    systemManager = std::make_unique<SystemManager>();
    archetypeManager = std::make_unique<ArchetypeManager>();
    time = 0.0f;
  }

  EntityID CreateEntity() {
    return entityManager->CreateEntity();
  }

  void DestroyEntity(EntityID entity) {
    archetypeManager->getBySignature(entityManager->GetRecord(entity).signature)
        ->RemoveEntity(entity);
    entityManager->DestroyEntity(entity);
  }

  template <typename T> void RegisterComponent() {
    componentRegistry->RegisterComponent<T>();
  }

  template <typename... Components> void RegisterComponents() {
    (this->RegisterComponent<Components>(), ...);
  }

  template <typename T> T &GetComponent(EntityID entity) {
    EntityRecord &record = entityManager->GetRecord(entity);
    ComponentID componentID = componentRegistry->GetComponentID<T>();

    if (!record.signature.test(componentID)) {
      throw std::runtime_error("Entity does not have component");
    }

    Archetype *archetype = archetypeManager->getBySignature(record.signature);
    return archetype->Get<T>(componentID, record.row);
  }

  template <typename T> void AddComponent(EntityID entity, T &&component) {
    AddComponents(entity, std::forward<T>(component));
  }

  template <typename... Ts>
  void AddComponents(EntityID entity, Ts &&...components) {
    EntityRecord &record = entityManager->GetRecord(entity);

    Signature oldSig = record.signature;
    Archetype *oldArchetype = archetypeManager->getBySignature(oldSig);

    if ((oldSig & componentRegistry->MakeSignature<Ts...>()).any()) {
      throw std::runtime_error("Entity already has component");
    }

    Signature newSig = oldSig;
    newSig = oldSig | componentRegistry->MakeSignature<Ts...>();

    Archetype &newArchetype =
        archetypeManager->getOrCreate(newSig, componentRegistry.get());
    newArchetype.AddEntity(entity);

    std::size_t newRow = newArchetype.GetRowForEntity(entity);

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
        auto &src = oldArchetype->GetColumn(c);
        auto &dst = newArchetype.GetColumn(c);
        std::memcpy(dst.at(newRow), src.at(oldRow), src.stride);
      }
      EntityID moved = oldArchetype->RemoveEntity(entity);
      if (moved != entity) {
        entityManager->GetRecord(moved).row = oldRow;
      }
    }

    (new (newArchetype.GetColumn(componentRegistry->GetComponentID<Ts>())
              .at(newRow)) Ts(std::forward<Ts>(components)),
     ...);

    record.signature = newSig;
    record.row = newRow;
  }

  template <typename T> void RemoveComponent(EntityID entity) {
    ComponentID componentID = componentRegistry->GetComponentID<T>();
    EntityRecord &record = entityManager->GetRecord(entity);

    Signature oldSig = record.signature;
    Archetype *oldArchetype = archetypeManager->getBySignature(oldSig);

    if (!oldSig.test(componentID)) {
      throw std::runtime_error("Entity does not have component");
    }

    Signature newSig = oldSig;
    newSig.reset(componentID);

    Archetype &newArchetype =
        archetypeManager->getOrCreate(newSig, componentRegistry.get());
    newArchetype.AddEntity(entity);
    std::size_t newRow = newArchetype.GetRowForEntity(entity);

    std::size_t oldRow = record.row;
    for (ComponentID c = 0; c < MAX_COMPONENTS; ++c) {
      if (!newSig.test(c))
        continue;
      auto &src = oldArchetype->GetColumn(c);
      auto &dst = newArchetype.GetColumn(c);
      std::memcpy(dst.at(newRow), src.at(oldRow), src.stride);
    }
    EntityID moved = oldArchetype->RemoveEntity(entity);
    if (moved != entity) {
      entityManager->GetRecord(moved).row = oldRow;
    }

    record.signature = newSig;
    record.row = newRow;
  }

  template <typename T> bool HasComponent(EntityID entity) {
    ComponentID componentID = componentRegistry->GetComponentID<T>();
    EntityRecord &record = entityManager->GetRecord(entity);
    return record.signature.test(componentID);
  }

  template <typename T> T &GetSingleton() {
    ComponentID componentID = componentRegistry->GetComponentID<T>();
    auto it = singletons.find(componentID);
    if (it == singletons.end()) {
      throw std::runtime_error("Singleton not found");
    }
    return *reinterpret_cast<T *>(singletons[componentID].data());
  }

  template <typename T> void SetSingleton(const T &value) {
    ComponentID componentID = componentRegistry->GetComponentID<T>();
    std::vector<uint8_t> &storage = singletons[componentID];
    if (storage.empty()) {
      storage.resize(sizeof(T));
    }
    *reinterpret_cast<T *>(storage.data()) = value;
  }

  template <typename T> bool HasSingleton() {
    ComponentID componentID = componentRegistry->GetComponentID<T>();
    return singletons.find(componentID) != singletons.end();
  }

  template <typename... Ts> Query<Ts...> query() {
    std::array<Archetype, MAX_ARCHETYPES> &archetypes =
        archetypeManager->getArchetypes();
    return Query<Ts...>(archetypes, *componentRegistry.get());
  }

  template <typename T>
  std::shared_ptr<T>
  RegisterSystem(SystemGroup group = SystemGroup::Simulation) {
    return systemManager->RegisterSystem<T>(group);
  }

  void CreateSystems() {
    systemManager->CreateAll(this);
  }

  void UpdateSystems(float deltaTime) {
    systemManager->UpdateAll(this, deltaTime);
  }

  void DestroySystems() {
    systemManager->DestroyAll(this);
  }
};
