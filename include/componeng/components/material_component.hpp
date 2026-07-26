#pragma once
#include "componeng/components/component_serializer.hpp"
#include "componeng/core/types.hpp"
#include "componeng/renderer/asset_manager.hpp"

namespace componeng::components {

struct MaterialComponent {
  static constexpr const char *component_name = "MaterialComponent";

  core::Vec3 color;
  std::string textureName;
  std::string shaderName;

  // Runtime data (not serialized)
  renderer::TextureID textureID = 0;
  renderer::ShaderID shaderID = 0;
};

#define MATERIAL_COMPONENT_FIELDS(F, ctx) \
  F(Vec3, color, ctx) \
  F(string, textureName, ctx) \
  F(string, shaderName, ctx)

SERIALIZABLE_COMPONENT(MaterialComponent, MATERIAL_COMPONENT_FIELDS)

#undef MATERIAL_COMPONENT_FIELDS

} // namespace componeng::components
