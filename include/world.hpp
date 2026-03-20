#pragma once
#include "component_manager.hpp"
#include "entity_manager.hpp"
#include "system_manager.hpp"
#include <memory>

class World {
private:
  std::unique_ptr<ComponentManager> componentManager;
  std::unique_ptr<EntityManager> entityManager;
  std::unique_ptr<SystemManager> systemManager;

public:
  EntityID cameraEntity;

  void Init() {
    componentManager = std::make_unique<ComponentManager>();
    entityManager = std::make_unique<EntityManager>();
    systemManager = std::make_unique<SystemManager>();
  }

  EntityID CreateEntity() {
    return entityManager->CreateEntity();
  }

  void DestroyEntity(EntityID entity) {
    entityManager->DestroyEntity(entity);
    componentManager->EntityDestroyed(entity);
    systemManager->EntityDestroyed(entity);
  }

  template <typename T> void RegisterComponent() {
    componentManager->RegisterComponent<T>();
  }

  template <typename T> void AddComponent(EntityID entity, T component) {
    componentManager->AddComponent<T>(entity, component);

    auto signature = entityManager->GetSignature(entity);
    signature.set(componentManager->GetComponentType<T>(), true);
    entityManager->SetSignature(entity, signature);

    systemManager->EntitySignatureChanged(entity, signature);
  }

  template <typename T> void RemoveComponent(EntityID entity) {
    componentManager->RemoveComponent<T>(entity);

    auto signature = entityManager->GetSignature(entity);
    signature.set(componentManager->GetComponentType<T>(), false);
    entityManager->SetSignature(entity, signature);

    systemManager->EntitySignatureChanged(entity, signature);
  }

  template <typename T> T &GetComponent(EntityID entity) {
    return componentManager->GetComponent<T>(entity);
  }

  template <typename T> std::shared_ptr<ComponentArray<T>> GetComponentArray() {
    return componentManager->GetComponentArray<T>();
  }

  template <typename T> ComponentTypeID GetComponentType() {
    return componentManager->GetComponentType<T>();
  }

  template <typename T> std::shared_ptr<T> RegisterSystem() {
    return systemManager->RegisterSystem<T>(*this);
  }

  template <typename T> void SetSystemSignature(Signature signature) {
    systemManager->SetSignature<T>(signature);
  }

  void Update(float deltaTime) {
    systemManager->UpdateAll(deltaTime);
  }
};
