#pragma once

#include "componeng/components/component_serializer.hpp"
#include "componeng/core/types.hpp"

namespace componeng::components {

struct TransformComponent {
  static constexpr const char* component_name = "TransformComponent";

  core::Vec3 position; // 12 bytes
  core::Vec3 rotation; // 12 bytes (Euler angles in radians)
  core::Vec3 scale;    // 12 bytes (default to (1, 1, 1) for no scaling)
};

template <> struct ComponentSerializer<TransformComponent> {
  static nlohmann::json serialize(const TransformComponent &component) {
    nlohmann::json transformJson;

    transformJson["position"] = {component.position.x, component.position.y,
                                 component.position.z};
    transformJson["rotation"] = {component.rotation.x, component.rotation.y,
                                 component.rotation.z};
    transformJson["scale"] = {component.scale.x, component.scale.y,
                              component.scale.z};

    return transformJson;
  }

  static TransformComponent deserialize(const nlohmann::json &transformJson) {
    TransformComponent component;
    if (transformJson.contains("position") &&
        transformJson["position"].is_array() &&
        transformJson["position"].size() == 3) {
      component.position.x = transformJson["position"][0];
      component.position.y = transformJson["position"][1];
      component.position.z = transformJson["position"][2];
    }

    if (transformJson.contains("rotation") &&
        transformJson["rotation"].is_array() &&
        transformJson["rotation"].size() == 3) {
      component.rotation.x = transformJson["rotation"][0];
      component.rotation.y = transformJson["rotation"][1];
      component.rotation.z = transformJson["rotation"][2];
    }

    if (transformJson.contains("scale") && transformJson["scale"].is_array() &&
        transformJson["scale"].size() == 3) {
      component.scale.x = transformJson["scale"][0];
      component.scale.y = transformJson["scale"][1];
      component.scale.z = transformJson["scale"][2];
    }

    return component;
  }
};

} // namespace componeng::components
