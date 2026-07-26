#pragma once
#include "componeng/components/component_serializer.hpp"
#include "componeng/renderer/asset_manager.hpp"

namespace componeng::components {

struct MeshComponent {
  renderer::MeshID meshID;        // 4 bytes
  const char *meshName = nullptr; // Optional: store name for serialization
  bool visible = true;            // 1 byte
};

template <> struct ComponentSerializer<MeshComponent> {
  static nlohmann::json serialize(const MeshComponent &component) {
    nlohmann::json meshJson;

    meshJson["meshName"] =
        component.meshName ? std::string(component.meshName) : "";
    meshJson["visible"] = component.visible;

    return meshJson;
  }

  static MeshComponent deserialize(const nlohmann::json &meshJson) {
    MeshComponent component;

    component.meshID =
        renderer::MeshID(); // Will be set later based on meshName
    component.visible = meshJson.value("visible", true);

    std::string meshName = meshJson.value("meshName", std::string());
    if (!meshName.empty()) {
      component.meshName = strdup(meshName.c_str());
    }

    return component;
  }
};

} // namespace componeng::components
