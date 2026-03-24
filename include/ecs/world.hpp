#pragma once
#include "core/logger.hpp"
#include "core/types.hpp"
#include "ecs/archetype.hpp"
#include "ecs/archetype_manager.hpp"
#include "ecs/component_registry.hpp"
#include "ecs/entity.hpp"
#include "ecs/entity_manager.hpp"
#include "ecs/query.hpp"
#include "ecs/system_manager.hpp"
#include <memory>

struct CameraData {
  Mat4 viewMatrix;
  Mat4 projectionMatrix;
};

class World {
private:
  std::unique_ptr<ComponentRegistry> componentRegistry;
  std::unique_ptr<EntityManager> entityManager;
  std::unique_ptr<SystemManager> systemManager;
  std::unique_ptr<ArchetypeManager> archetypeManager;

public:
  // Global cache
  CameraData mainCameraData;

  void Init() {
    componentRegistry = std::make_unique<ComponentRegistry>();
    entityManager = std::make_unique<EntityManager>();
    systemManager = std::make_unique<SystemManager>();
    archetypeManager = std::make_unique<ArchetypeManager>();
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

  template <typename T> void AddComponent(EntityID entity, T &&component) {
    AddComponents(entity, std::forward<T>(component));
  }

  template <typename... Ts>
  void AddComponents(EntityID entity, Ts &&...components) {
    EntityRecord &record = entityManager->GetRecord(entity);

    Signature oldSig = record.signature;
    Archetype *oldArchetype = archetypeManager->getBySignature(oldSig);

    if ((oldSig.test(componentRegistry->GetComponentID<Ts>()) || ...)) {
      throw std::runtime_error("Entity already has component");
    }

    Signature newSig = oldSig;
    newSig = oldSig | componentRegistry->MakeSignature<Ts...>();

    Archetype &newArchetype =
        archetypeManager->getOrCreate(newSig, componentRegistry.get());
    newArchetype.AddEntity(entity);

    std::size_t newRow = newArchetype.GetRowForEntity(entity);

    if (oldArchetype == nullptr) {
      LOG_INFO("Adding first component to entity %llu", (unsigned long long)entity);
    } else {
      LOG_INFO("Moving entity %llu to new archetype",
          (unsigned long long)entity);
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

  template <typename... Ts> Query<Ts...> query() {
    std::array<Archetype, MAX_ARCHETYPES> &archetypes =
        archetypeManager->getArchetypes();
    return Query<Ts...>(archetypes, *componentRegistry.get());
  }

  template <typename T> std::shared_ptr<T> RegisterSystem() {
    return systemManager->RegisterSystem<T>(*this);
  }

  void Update(float deltaTime) {
    systemManager->UpdateAll(deltaTime);
  }

  void Shutdown() {
    systemManager->ShutdownAll();
  }
};
