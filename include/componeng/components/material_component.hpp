#pragma once
#include "componeng/components/component_serializer.hpp"
#include "componeng/core/types.hpp"
#include "componeng/renderer/asset_manager.hpp"

namespace componeng::components {

struct MaterialComponent {
  static constexpr const char* component_name = "MaterialComponent";

  core::Vec3 color;                  // 12 bytes
  renderer::TextureID textureID;     // 4 bytes
  renderer::ShaderID shaderID;       // 4 bytes
  const char *textureName = nullptr; // Optional: store name for serialization
  const char *shaderName = nullptr;  // Optional: store name for serialization
};

template <> struct ComponentSerializer<MaterialComponent> {
  static nlohmann::json serialize(const MaterialComponent &component) {
    nlohmann::json materialJson;

    materialJson["color"] = {component.color.x, component.color.y,
                             component.color.z};
    materialJson["textureName"] =
        component.textureName ? std::string(component.textureName) : "";
    materialJson["shaderName"] =
        component.shaderName ? std::string(component.shaderName) : "";

    return materialJson;
  }

  static MaterialComponent deserialize(const nlohmann::json &materialJson) {
    MaterialComponent component;

    if (materialJson.contains("color") && materialJson["color"].is_array() &&
        materialJson["color"].size() == 3) {
      component.color.x = materialJson["color"][0];
      component.color.y = materialJson["color"][1];
      component.color.z = materialJson["color"][2];
    }

    component.textureID = renderer::TextureID();
    component.shaderID = renderer::ShaderID();

    std::string textureName = materialJson.value("textureName", std::string());
    std::string shaderName = materialJson.value("shaderName", std::string());

    // Store names for later lookup during deserialization
    if (!textureName.empty()) {
      component.textureName = strdup(textureName.c_str());
    }
    if (!shaderName.empty()) {
      component.shaderName = strdup(shaderName.c_str());
    }

    return component;
  }
};

} // namespace componeng::components
