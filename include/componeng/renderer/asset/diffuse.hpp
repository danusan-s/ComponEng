#pragma once

#include "componeng/core/transform_component.hpp"
#include "componeng/renderer/component/material_component.hpp"
#include "material.hpp"

namespace componeng::renderer {

class DiffuseMaterial : public IMaterial {
public:
  // Vertex layout is not declared here — AssetManager::registerMaterial
  // reflects it straight off the compiled shader's active attributes.
  using IMaterial::IMaterial;

  void buildInstanceData(ecs::World &world, ecs::EntityID entity,
                         core::UniformMap &outInstanceData) const override {
    const auto &transform =
        world.getComponent<core::TransformComponent>(entity);
    // TransformComponent::rotation is in degrees; glm::rotate takes radians.
    core::Mat4 modelMatrix = core::Mat4(1.0f);
    modelMatrix = translate(modelMatrix, transform.position);
    modelMatrix = rotate(modelMatrix, core::radians(transform.rotation.x),
                         core::Vec3(1.0f, 0.0f, 0.0f));
    modelMatrix = rotate(modelMatrix, core::radians(transform.rotation.y),
                         core::Vec3(0.0f, 1.0f, 0.0f));
    modelMatrix = rotate(modelMatrix, core::radians(transform.rotation.z),
                         core::Vec3(0.0f, 0.0f, 1.0f));
    modelMatrix = scale(modelMatrix, transform.scale);

    outInstanceData["instanceModel"] = modelMatrix;
    outInstanceData["instanceColor"] =
        world.hasComponent<renderer::ColorComponent>(entity)
            ? world.getComponent<renderer::ColorComponent>(entity).color
            : core::Vec4(1.0f);
  }
};

} // namespace componeng::renderer
