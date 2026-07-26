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

// =============================================================================
// SERIALIZABLE_COMPONENT macro — generates ComponentSerializer<T> specialization
//
// Each component defines a field list macro like:
//
//   #define MY_COMPONENT_FIELDS(F, ctx) \
//     F(float, someFloat, 0.0f, ctx) \
//     F(Vec3, someVec3, ctx) \
//     F(string, someString, ctx) \
//     F(enum, someEnum, 0, ctx) \
//     F(Mat4, viewProj, ctx) \
//     F(Nested, child, ChildType, ctx)
//
// Types without defaults:    F(type, name, ctx)
// Types with defaults:       F(type, name, default, ctx)
// Nested components:         F(Nested, name, ComponentType, ctx)
//
// Then: SERIALIZABLE_COMPONENT(MyComponent, MY_COMPONENT_FIELDS)
// =============================================================================

#define SERIALIZE_FIELD(type, name, ...) SERIALIZE_FIELD_##type(name, __VA_ARGS__)
#define DESERIALIZE_FIELD(type, name, ...) DESERIALIZE_FIELD_##type(name, __VA_ARGS__)

#define SERIALIZE_FIELD_float(name, default, ctx)        j[#name] = ctx.name;
#define DESERIALIZE_FIELD_float(name, default, ctx)       ctx.name = j.value(#name, default);

#define SERIALIZE_FIELD_int(name, default, ctx)          j[#name] = ctx.name;
#define DESERIALIZE_FIELD_int(name, default, ctx)         ctx.name = j.value(#name, default);

#define SERIALIZE_FIELD_bool(name, default, ctx)          j[#name] = ctx.name;
#define DESERIALIZE_FIELD_bool(name, default, ctx)        ctx.name = j.value(#name, default);

#define SERIALIZE_FIELD_string(name, ctx)                 j[#name] = ctx.name;
#define DESERIALIZE_FIELD_string(name, ctx)                ctx.name = j.value(#name, std::string());

#define SERIALIZE_FIELD_Vec3(name, ctx) \
  j[#name] = {ctx.name.x, ctx.name.y, ctx.name.z};

#define DESERIALIZE_FIELD_Vec3(name, ctx) \
  if (j.contains(#name) && j[#name].is_array() && j[#name].size() == 3) { \
    ctx.name.x = j[#name][0]; \
    ctx.name.y = j[#name][1]; \
    ctx.name.z = j[#name][2]; \
  }

#define SERIALIZE_FIELD_Mat4(name, ctx) \
  j[#name] = SerializeMat4(ctx.name);

#define DESERIALIZE_FIELD_Mat4(name, ctx) \
  if (j.contains(#name) && j[#name].is_array()) { \
    ctx.name = DeserializeMat4(j[#name].get<std::vector<float>>()); \
  }

#define SERIALIZE_FIELD_enum(name, default, ctx) \
  j[#name] = static_cast<int>(ctx.name);

#define DESERIALIZE_FIELD_enum(name, default, ctx) \
  ctx.name = static_cast<decltype(ctx.name)>(j.value(#name, static_cast<int>(default)));

#define SERIALIZE_FIELD_Nested(name, NestedType, ctx) \
  j[#name] = ComponentSerializer<NestedType>::serialize(ctx.name);

#define DESERIALIZE_FIELD_Nested(name, NestedType, ctx) \
  if (j.contains(#name)) { \
    ctx.name = ComponentSerializer<NestedType>::deserialize(j[#name]); \
  }

#define SERIALIZABLE_COMPONENT(Type, Fields) \
template <> struct ComponentSerializer<Type> { \
  static nlohmann::json serialize(const Type &component) { \
    nlohmann::json j; \
    Fields(SERIALIZE_FIELD, component) \
    return j; \
  } \
  static Type deserialize(const nlohmann::json &j) { \
    Type component; \
    Fields(DESERIALIZE_FIELD, component) \
    return component; \
  } \
};

} // namespace componeng::components
