#pragma once
#include "componeng/components/component_serializer.hpp"
#include "componeng/renderer/asset_manager.hpp"

namespace componeng::components {

struct MeshComponent {
  static constexpr const char *component_name = "MeshComponent";

  std::string meshName;

  // Runtime data (not serialized)
  renderer::MeshID meshID = 0;
  bool visible = true;
};

#define MESH_COMPONENT_FIELDS(F, ctx) \
  F(string, meshName, ctx)

SERIALIZABLE_COMPONENT(MeshComponent, MESH_COMPONENT_FIELDS)

#undef MESH_COMPONENT_FIELDS

} // namespace componeng::components
