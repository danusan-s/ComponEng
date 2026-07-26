#pragma once
#include "componeng/components/component_serializer.hpp"
#include "componeng/core/types.hpp"

namespace componeng::components {

struct CameraComponent {
  float fov;                       // 4 bytes
  float aspectRatio;               // 4 bytes
  float nearPlane;                 // 4 bytes
  float farPlane;                  // 4 bytes
  core::Mat4 viewProjectionMatrix; // 64 bytes
};

template <> struct ComponentSerializer<CameraComponent> {
  static nlohmann::json serialize(const CameraComponent &component) {
    nlohmann::json cameraJson;

    cameraJson["fov"] = component.fov;
    cameraJson["aspectRatio"] = component.aspectRatio;
    cameraJson["nearPlane"] = component.nearPlane;
    cameraJson["farPlane"] = component.farPlane;
    cameraJson["viewProjectionMatrix"] =
        SerializeMat4(component.viewProjectionMatrix);

    return cameraJson;
  }

  static CameraComponent deserialize(const nlohmann::json &cameraJson) {
    CameraComponent component;

    component.fov = cameraJson.value("fov", 60.0f);
    component.aspectRatio = cameraJson.value("aspectRatio", 16.0f / 9.0f);
    component.nearPlane = cameraJson.value("nearPlane", 0.1f);
    component.farPlane = cameraJson.value("farPlane", 1000.0f);

    if (cameraJson.contains("viewProjectionMatrix")) {
      const auto &matrixData = cameraJson["viewProjectionMatrix"];
      if (matrixData.is_array() && matrixData.size() == 16) {
        component.viewProjectionMatrix =
            DeserializeMat4(matrixData.get<std::vector<float>>());
      }
    }

    return component;
  }
};

} // namespace componeng::components
