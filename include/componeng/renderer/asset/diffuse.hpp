#pragma once

#include "componeng/components/transform_component.hpp"
#include "material.hpp"

namespace componeng::renderer {

class DiffuseMaterial : public IMaterial {
public:
  DiffuseMaterial(core::HandleID shaderID, core::HandleID textureID)
      : IMaterial(shaderID, textureID) {
    api::VertexLayout layout;
    layout.attributes = {
        {"modelMatrix[0]", 0, 4, false},
        {"modelMatrix[1]", 16, 4, false},
        {"modelMatrix[2]", 32, 4, false},
        {"modelMatrix[3]", 48, 4, false},
    };
    layout.stride = 64; // 4x4 matrix = 16 floats * 4 bytes = 64 bytes
    setVertexLayout(layout);
  }

  core::UniformMap buildInstanceData(ecs::World &world,
                                     ecs::EntityID entity) const override {
    core::UniformMap instanceData;
    const auto &transform =
        world.getComponent<components::TransformComponent>(entity);
    core::Mat4 modelMatrix = core::Mat4(1.0f);
    modelMatrix = translate(modelMatrix, transform.position);
    modelMatrix =
        rotate(modelMatrix, transform.rotation.x, core::Vec3(1.0f, 0.0f, 0.0f));
    modelMatrix =
        rotate(modelMatrix, transform.rotation.y, core::Vec3(0.0f, 1.0f, 0.0f));
    modelMatrix =
        rotate(modelMatrix, transform.rotation.z, core::Vec3(0.0f, 0.0f, 1.0f));
    modelMatrix = scale(modelMatrix, transform.scale);
    instanceData["modelMatrix"] = modelMatrix;
    return instanceData;
  }
};

} // namespace componeng::renderer
