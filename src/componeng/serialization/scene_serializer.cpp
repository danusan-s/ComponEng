#include "componeng/serialization/scene_serializer.hpp"

#include "componeng/camera/main_camera.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/utils/logger.hpp"

#include <cstdint>
#include <fstream>
#include <unordered_map>
#include <vector>

namespace componeng::serialization {

namespace {
thread_local const std::unordered_map<ecs::EntityID, EntityLocalID>
    *t_runtimeToLocal = nullptr;
thread_local const std::unordered_map<EntityLocalID, ecs::EntityID>
    *t_localToRuntime = nullptr;

}

void beginEntityReferenceSave(
    const std::unordered_map<ecs::EntityID, EntityLocalID> &runtimeToLocal) {
  t_runtimeToLocal = &runtimeToLocal;
}

void endEntityReferenceSave() {
  t_runtimeToLocal = nullptr;
}

EntityLocalID serializeEntityReference(ecs::EntityID entity) {
  if (entity == ecs::INVALID_ENTITY) {
    return 0;
  }
  if (!t_runtimeToLocal) {
    throw std::runtime_error("Entity reference serialized outside a scene save");
  }
  auto it = t_runtimeToLocal->find(entity);
  if (it == t_runtimeToLocal->end()) {
    throw std::runtime_error("Entity reference points outside the saved scene");
  }
  return it->second;
}

void beginEntityReferenceLoad(
    const std::unordered_map<EntityLocalID, ecs::EntityID> &localToRuntime) {
  t_localToRuntime = &localToRuntime;
}

void endEntityReferenceLoad() {
  t_localToRuntime = nullptr;
}

ecs::EntityID deserializeEntityReference(const nlohmann::json &json) {
  if (json.is_number_unsigned()) {
    EntityLocalID localID = json.get<EntityLocalID>();
    if (localID == 0) {
      return ecs::INVALID_ENTITY;
    }
    if (!t_localToRuntime) {
      throw std::runtime_error("Entity reference deserialized outside a scene load");
    }
    auto it = t_localToRuntime->find(localID);
    if (it == t_localToRuntime->end()) {
      throw std::runtime_error("Unknown local entity reference");
    }
    return it->second;
  }
  if (json.is_number_integer()) {
    EntityLocalID localID = json.get<EntityLocalID>();
    if (localID == 0) {
      return ecs::INVALID_ENTITY;
    }
    if (t_localToRuntime) {
      auto it = t_localToRuntime->find(localID);
      if (it != t_localToRuntime->end()) {
        return it->second;
      }
    }
  }
  throw std::runtime_error("Invalid local entity reference");
}

bool SceneSerializer::save(ecs::World &world, const std::string &filename) {
  std::array<ecs::Archetype, ecs::MAX_ARCHETYPES> &archetypes =
      world.m_archetypeManager.getArchetypes();
  std::unordered_map<ecs::EntityID, EntityLocalID> runtimeToLocal;

  EntityLocalID localID = 1;
  for (ecs::Archetype &archetype : archetypes) {
    for (std::size_t i = 0; i < archetype.getEntityCount(); ++i) {
      ecs::EntityID entity = archetype.getEntityForRow(i);
      if (runtimeToLocal.find(entity) == runtimeToLocal.end()) {
        runtimeToLocal[entity] = localID++;
      }
    }
  }

  nlohmann::json sceneJson;
  sceneJson["entities"] = nlohmann::json::array();

  beginEntityReferenceSave(runtimeToLocal);
  for (ecs::Archetype &archetype : archetypes) {
    if (archetype.getEntityCount() == 0)
      continue;

    const ecs::Signature &signature = archetype.m_signature;

    // For each entity populate a JSON object with its components
    for (std::size_t i = 0; i < archetype.getEntityCount(); ++i) {
      ecs::EntityID entity = archetype.getEntityForRow(i);
      nlohmann::json entityJson;
      entityJson["_entityId"] = runtimeToLocal.at(entity);

      for (ecs::ComponentID c = 0; c < ecs::MAX_COMPONENTS; ++c) {
        if (!signature.test(c))
          continue;

        const ecs::ComponentInfo &info =
            world.m_componentRegistry.getComponentInfo(c);
        if (!info.serializer) {
          LOG_ERROR(
              "No serializer for component ID %d (%s), skipping serialization",
              c, info.name);
          continue;
        }

        const void *componentPtr = archetype.readColumn(c).at(i);
        entityJson[info.name] = info.serializer(componentPtr);
      }

      if (world.hasResource<camera::MainCamera>() &&
          entity == world.getResource<camera::MainCamera>().entity) {
        entityJson["_isMainCamera"] = true;
        entityJson["_mainCamera"] = runtimeToLocal.at(entity);
      }

      sceneJson["entities"].push_back(entityJson);
    }
  }
  endEntityReferenceSave();

  std::ofstream file(filename);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open file %s for saving scene", filename.c_str());
    return false;
  }

  file << sceneJson.dump(4);
  file.close();
  LOG_INFO("Scene saved to %s", filename.c_str());
  return true;
}

bool SceneSerializer::load(ecs::World &world, const std::string &filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open file %s for loading scene", filename.c_str());
    return false;
  }

  nlohmann::json sceneJson;
  try {
    file >> sceneJson;
  } catch (const nlohmann::json::parse_error &e) {
    LOG_ERROR("Failed to parse scene %s: %s", filename.c_str(), e.what());
    return false;
  }
  file.close();

  if (!sceneJson.contains("entities") || !sceneJson["entities"].is_array()) {
    LOG_ERROR("Invalid scene format: missing 'entities' array");
    return false;
  }

  ecs::EntityID mainCameraEntity = ecs::INVALID_ENTITY;
  std::unordered_map<EntityLocalID, ecs::EntityID> localToRuntime;
  EntityLocalID nextLocalID = 1;
  std::vector<ecs::EntityID> loadedRuntimeEntities;
  std::vector<EntityLocalID> loadedLocalIDs;

  for (const auto &entityJson : sceneJson["entities"]) {
    EntityLocalID localID = nextLocalID++;
    if (entityJson.contains("_entityId")) {
      const auto &idJson = entityJson["_entityId"];
      if (!idJson.is_number_unsigned() && !idJson.is_number_integer()) {
        LOG_ERROR("Invalid _entityId in scene, skipping entity");
        loadedRuntimeEntities.push_back(ecs::INVALID_ENTITY);
        loadedLocalIDs.push_back(0);
        continue;
      }
      localID = idJson.get<EntityLocalID>();
      if (localID == 0) {
        LOG_ERROR("Invalid zero _entityId in scene, skipping entity");
        loadedRuntimeEntities.push_back(ecs::INVALID_ENTITY);
        loadedLocalIDs.push_back(0);
        continue;
      }
    }
    if (localToRuntime.find(localID) != localToRuntime.end()) {
      LOG_ERROR("Duplicate local entity ID %llu in scene, skipping entity",
                static_cast<unsigned long long>(localID));
      loadedRuntimeEntities.push_back(ecs::INVALID_ENTITY);
      loadedLocalIDs.push_back(0);
      continue;
    }

    ecs::EntityID entity = world.createEntity();
    localToRuntime[localID] = entity;
    loadedRuntimeEntities.push_back(entity);
    loadedLocalIDs.push_back(localID);
  }

  beginEntityReferenceLoad(localToRuntime);
  for (std::size_t entityIndex = 0;
       entityIndex < sceneJson["entities"].size(); ++entityIndex) {
    if (entityIndex >= loadedRuntimeEntities.size()) {
      continue;
    }
    const auto &entityJson = sceneJson["entities"][entityIndex];
    ecs::EntityID entity = loadedRuntimeEntities[entityIndex];
    EntityLocalID localID = loadedLocalIDs[entityIndex];
    if (entity == ecs::INVALID_ENTITY) {
      continue;
    }
    LOG_INFO("Created entity %llu from scene",
             static_cast<unsigned long long>(entity));

    for (auto it = entityJson.begin(); it != entityJson.end(); ++it) {
      const std::string &componentName = it.key();
      const nlohmann::json &componentData = it.value();
      LOG_INFO("Adding component %s to entity", componentName.c_str());

      if (componentName == "_entityId" || componentName == "_isMainCamera" ||
          componentName == "_mainCamera") {
        continue;
      }

      // Find component ID by name using O(1) lookup
      ecs::ComponentID componentID;
      try {
        componentID =
            world.m_componentRegistry.getComponentIDByName(componentName);
      } catch (const std::runtime_error &) {
        LOG_ERROR("Unknown component type '%s' in scene, skipping",
                  componentName.c_str());
        continue;
      }

      const ecs::ComponentInfo &info =
          world.m_componentRegistry.getComponentInfo(componentID);
      if (!info.deserializer) {
        LOG_ERROR("No deserializer for component '%s' (ID %d), skipping",
                  componentName.c_str(), componentID);
        continue;
      }

      // Scene data is untrusted; a malformed field must skip its component
      // rather than abort the whole load.
      void *componentPtr = nullptr;
      try {
        componentPtr = info.deserializer(componentData);
      } catch (const std::exception &e) {
        LOG_ERROR("Failed to deserialize component '%s': %s, skipping",
                  componentName.c_str(), e.what());
        continue;
      }

      // addComponentById memcpys the bytes into archetype storage, so the
      // heap object the deserializer handed back must be released here --
      // including if that call throws.
      try {
        world.addComponentById(entity, componentID, componentPtr);
      } catch (...) {
        if (info.deleter) {
          info.deleter(componentPtr);
        }
        throw;
      }
      if (info.deleter) {
        info.deleter(componentPtr);
      }
    }

    if (entityJson.contains("_isMainCamera") &&
        entityJson["_isMainCamera"].get<bool>()) {
      mainCameraEntity = entity;
    }
    if (entityJson.contains("_mainCamera")) {
      EntityLocalID cameraLocalID =
          entityJson["_mainCamera"].get<EntityLocalID>();
      if (cameraLocalID == localID) {
        mainCameraEntity = entity;
      }
    }
  }
  endEntityReferenceLoad();

  if (mainCameraEntity != ecs::INVALID_ENTITY) {
    world.setResource(camera::MainCamera{mainCameraEntity});
    LOG_INFO("Main camera set to entity %llu",
             static_cast<unsigned long long>(mainCameraEntity));
  }

  LOG_INFO("Scene loaded from %s", filename.c_str());
  return true;
}

} // namespace componeng::serialization
