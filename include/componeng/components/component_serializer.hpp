#pragma once

#include "componeng/utils/logger.hpp"
#include "json.hpp"
#include <glm/glm.hpp>
#include <stdexcept>

namespace componeng::components {

template <typename T> struct ComponentSerializer {
  static nlohmann::json serialize(const T &component) {
    LOG_ERROR("Serialization not implemented for this component type. %s",
              typeid(T).name());
    return nlohmann::json();
  }

  static T deserialize(const nlohmann::json &componentJson) {
    LOG_ERROR("Deserialization not implemented for this component type. %s",
              typeid(T).name());
    return T();
  }
};

static inline std::array<float, 16> SerializeMat4(const glm::mat4 &m) {
  std::array<float, 16> data;

  const float *ptr = &m[0][0]; // GLM guarantees contiguous storage

  for (int i = 0; i < 16; i++)
    data[i] = ptr[i];

  return data;
}

static inline glm::mat4 DeserializeMat4(const std::vector<float> &data) {
  if (data.size() != 16)
    throw std::runtime_error("Invalid data size for deserializing glm::mat4");

  glm::mat4 m;

  float *ptr = &m[0][0]; // GLM guarantees contiguous storage

  for (int i = 0; i < 16; i++)
    ptr[i] = data[i];

  return m;
}

} // namespace componeng::components
