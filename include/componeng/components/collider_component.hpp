#pragma once
#include "componeng/components/component_serializer.hpp"
#include "componeng/components/transform_component.hpp"

namespace componeng::components {

enum class ColliderType { Box, Sphere };

struct ColliderComponent {
  ColliderType type;
  TransformComponent transform;
};

template <> struct ComponentSerializer<ColliderComponent> {
  static nlohmann::json serialize(const ColliderComponent &component) {
    nlohmann::json colliderJson;

    colliderJson["type"] = static_cast<int>(component.type);

    colliderJson["transform"] =
        ComponentSerializer<TransformComponent>::serialize(component.transform);

    return colliderJson;
  }

  static ColliderComponent deserialize(const nlohmann::json &colliderJson) {
    ColliderComponent component;

    int typeInt = colliderJson.value("type", 0);
    component.type = static_cast<ColliderType>(typeInt);

    if (colliderJson.contains("transform")) {
      const auto &transformComp =
          ComponentSerializer<TransformComponent>::deserialize(
              colliderJson["transform"]);
      component.transform = transformComp;
    }

    return component;
  }
};

} // namespace componeng::components
