#include "componeng/ecs/world.hpp"
#include "componeng/events/entity_event.hpp"
#include "componeng/resources/action_state.hpp"
#include "componeng/resources/input_state.hpp"
#include "componeng/resources/main_camera.hpp"
#include "componeng/utils/logger.hpp"

#include <fstream>

namespace componeng::ecs {

void World::init() {
  set_resource(resources::InputState());
  set_resource(resources::ActionState());
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
  return *get_resource<std::unique_ptr<renderer::api::IRenderDevice>>().get();
}

void World::saveScene(const std::string &filename) {
  std::array<Archetype, MAX_ARCHETYPES> &archetypes =
      m_archetypeManager.getArchetypes();

  nlohmann::json sceneJson;
  sceneJson["entities"] = nlohmann::json::array();

  for (Archetype &archetype : archetypes) {
    if (archetype.getEntityCount() == 0)
      continue;

    const Signature &signature = archetype.m_signature;

    // For each entity populate a JSON object with its components
    for (size_t i = 0; i < archetype.getEntityCount(); ++i) {
      EntityID entity = archetype.getEntityForRow(i);
      nlohmann::json entityJson;

      for (ComponentID c = 0; c < MAX_COMPONENTS; ++c) {
        if (!signature.test(c))
          continue;

        const ComponentInfo &info = m_componentRegistry.getComponentInfo(c);
        if (!info.serializer) {
          LOG_ERROR(
              "No serializer for component ID %d (%s), skipping serialization",
              c, info.name);
          continue;
        }

        const void *componentPtr = archetype.getColumn(c).at(i);
        entityJson[info.name] = info.serializer(componentPtr);
      }

      if (has_resource<resources::MainCamera>() &&
          entity == get_resource<resources::MainCamera>().entity) {
        entityJson["_isMainCamera"] = true;
      }

      sceneJson["entities"].push_back(entityJson);
    }
  }

  std::ofstream file(filename);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open file %s for saving scene", filename.c_str());
    return;
  }

  file << sceneJson.dump(4);
  file.close();
  LOG_INFO("Scene saved to %s", filename.c_str());
}

void World::loadScene(const std::string &filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open file %s for loading scene", filename.c_str());
    return;
  }

  nlohmann::json sceneJson;
  file >> sceneJson;
  file.close();

  if (!sceneJson.contains("entities") || !sceneJson["entities"].is_array()) {
    LOG_ERROR("Invalid scene format: missing 'entities' array");
    return;
  }

  ecs::EntityID mainCameraEntity = ecs::INVALID_ENTITY;

  for (const auto &entityJson : sceneJson["entities"]) {
    EntityID entity = createEntity();
    LOG_INFO("Created entity %d from scene", (unsigned long long)entity);

    for (auto it = entityJson.begin(); it != entityJson.end(); ++it) {
      const std::string &componentName = it.key();
      const nlohmann::json &componentData = it.value();
      LOG_INFO("Adding component %s to entity", componentName.c_str());

      if (componentName == "_isMainCamera") {
        continue;
      }

      // Find component ID by name using O(1) lookup
      ComponentID componentID;
      try {
        componentID = m_componentRegistry.getComponentIDByName(componentName);
      } catch (const std::runtime_error &) {
        LOG_ERROR("Unknown component type '%s' in scene, skipping",
                  componentName.c_str());
        continue;
      }

      const ComponentInfo &info =
          m_componentRegistry.getComponentInfo(componentID);
      if (!info.deserializer) {
        LOG_ERROR("No deserializer for component '%s' (ID %d), skipping",
                  componentName.c_str(), componentID);
        continue;
      }

      void *componentPtr = info.deserializer(componentData);
      addComponentById(entity, componentID, componentPtr);
    }

    if (entityJson.contains("_isMainCamera") &&
        entityJson["_isMainCamera"].get<bool>()) {
      mainCameraEntity = entity;
    }
  }

  if (mainCameraEntity != ecs::INVALID_ENTITY) {
    set_resource(resources::MainCamera{mainCameraEntity});
    LOG_INFO("Main camera set to entity %d",
             (unsigned long long)mainCameraEntity);
  }

  LOG_INFO("Scene loaded from %s", filename.c_str());
}

} // namespace componeng::ecs
