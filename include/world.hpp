#pragma once
#include "archetype.hpp"
#include "archetype_manager.hpp"
#include "component_registry.hpp"
#include "entity_manager.hpp"
#include "input_state.hpp"
#include "query.hpp"
#include "system_manager.hpp"
#include "types.hpp"
#include <memory>
#include <vector>

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
  InputState *inputState;

  void Init(InputState *inputState) {
    componentRegistry = std::make_unique<ComponentRegistry>();
    entityManager = std::make_unique<EntityManager>();
    systemManager = std::make_unique<SystemManager>();
    archetypeManager = std::make_unique<ArchetypeManager>();
    this->inputState = inputState;
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

  template <typename T> void AddComponent(EntityID entity, T component = T{}) {
    ComponentID componentID = componentRegistry->RegisterComponent<T>();
    EntityRecord &record = entityManager->GetRecord(entity);

    Signature oldSig = record.signature;

    if (oldSig.test(componentID)) {
      throw std::runtime_error("Entity already has component");
    }

    Archetype *oldArchetype = archetypeManager->getBySignature(oldSig);

    Signature newSig = oldSig.set(componentID);
    Archetype &newArchetype =
        archetypeManager->getOrCreate(newSig, *componentRegistry.get());
    newArchetype.AddEntity(entity);
    std::size_t newRow = newArchetype.GetRowForEntity(entity);

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

    T *slot = static_cast<T *>(newArchetype.GetColumn(componentID).at(newRow));
    *slot = std::move(component);

    record.signature = newSig;
    record.row = newRow;
  }

  template <typename T> void RemoveComponent(EntityID entity) {
    ComponentID componentID = componentRegistry->GetComponentID<T>();
    EntityRecord &record = entityManager->GetRecord(entity);

    Signature oldSig = record.signature;

    if (!oldSig.test(componentID)) {
      throw std::runtime_error("Entity does not have component");
    }

    Archetype *oldArchetype = archetypeManager->getBySignature(oldSig);

    Signature newSig = oldSig.reset(componentID);
    Archetype &newArchetype =
        archetypeManager->getOrCreate(newSig, *componentRegistry.get());
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
    std::vector<Archetype> &archetypes = archetypeManager->getArchetypes();
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
