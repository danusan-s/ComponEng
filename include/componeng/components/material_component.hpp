#pragma once

#include "componeng/components/component_serializer.hpp"
#include "componeng/core/types.hpp"
#include "componeng/renderer/asset_manager.hpp"

namespace componeng::components {

struct MaterialComponent {
  static constexpr const char *component_name = "MaterialComponent";

  core::Vec3 color;
  core::Name textureName;
  core::Name shaderName;

  // Runtime data (not serialized)
  core::HandleID textureID = 0;
  core::HandleID shaderID = 0;
};

#define MATERIAL_COMPONENT_FIELDS(F, ctx)                                      \
  F(color, ctx)                                                                \
  F(textureName, ctx)                                                          \
  F(shaderName, ctx)

SERIALIZABLE_COMPONENT(MaterialComponent, MATERIAL_COMPONENT_FIELDS)

#undef MATERIAL_COMPONENT_FIELDS

} // namespace componeng::components
