#pragma once
#include "componeng/components/component_serializer.hpp"
#include "componeng/core/types.hpp"
#include "json.hpp"

namespace componeng::components {

struct RigidBodyComponent {
  enum Type {
    Static,   // Immovable object, not affected by forces
    Dynamic,  // Affected by forces, can move and rotate
    Kinematic // Moves according to user velocities, not affected by forces
  };

  Type type;
  core::Vec3 velocity;
  float mass;
  float restitution;
};

template <> struct ComponentSerializer<RigidBodyComponent> {
  static nlohmann::json serialize(const RigidBodyComponent &component) {
    nlohmann::json rigidBodyJson;

    rigidBodyJson["type"] = static_cast<int>(component.type);
    rigidBodyJson["velocity"] = {component.velocity.x, component.velocity.y,
                                 component.velocity.z};
    rigidBodyJson["mass"] = component.mass;
    rigidBodyJson["restitution"] = component.restitution;

    return rigidBodyJson;
  }

  static RigidBodyComponent deserialize(const nlohmann::json &rigidBodyJson) {
    RigidBodyComponent component;

    int typeInt = rigidBodyJson.value("type", 0);
    component.type = static_cast<RigidBodyComponent::Type>(typeInt);

    if (rigidBodyJson.contains("velocity") &&
        rigidBodyJson["velocity"].is_array() &&
        rigidBodyJson["velocity"].size() == 3) {
      component.velocity.x = rigidBodyJson["velocity"][0];
      component.velocity.y = rigidBodyJson["velocity"][1];
      component.velocity.z = rigidBodyJson["velocity"][2];
    }

    component.mass = rigidBodyJson.value("mass", 1.0f);
    component.restitution = rigidBodyJson.value("restitution", 0.5f);

    return component;
  }
};

} // namespace componeng::components
