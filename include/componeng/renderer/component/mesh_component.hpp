#pragma once

#include "componeng/core/types.hpp"
#include "componeng/ecs/component_serializer.hpp"

namespace componeng::renderer {

struct MeshComponent {
  static constexpr const char *component_name = "MeshComponent";

  core::Name meshName;

  // Runtime data (not serialized)
  core::HandleID meshID = 0;
  bool visible = true;
};

} // namespace componeng::renderer

namespace componeng::ecs {

#define MESH_COMPONENT_FIELDS(F, ctx) F(meshName, ctx)

SERIALIZABLE_COMPONENT(renderer::MeshComponent, MESH_COMPONENT_FIELDS)

#undef MESH_COMPONENT_FIELDS

} // namespace componeng::ecs
