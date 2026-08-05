#include "componeng/renderer/batching/batching_system.hpp"

#include "componeng/camera/camera_component.hpp"
#include "componeng/camera/main_camera.hpp"
#include "componeng/components/material_component.hpp"
#include "componeng/components/mesh_component.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/core/types.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/asset/material.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/renderer/backend/api/irender_device.hpp"
#include "componeng/renderer/batching/render_queue.hpp"

namespace componeng::renderer {

static core::Mat4
getModelMatrix(const components::TransformComponent &transform) {
  core::Mat4 model = core::Mat4(1.0f);
  model = translate(model, transform.position);
  model = rotate(model, transform.rotation.x, core::Vec3(1.0f, 0.0f, 0.0f));
  model = rotate(model, transform.rotation.y, core::Vec3(0.0f, 1.0f, 0.0f));
  model = rotate(model, transform.rotation.z, core::Vec3(0.0f, 0.0f, 1.0f));
  model = scale(model, transform.scale);
  return model;
}

void BatchingSystem::onCreate(const ecs::SystemState &state) {
  m_batches = std::make_unique<BatchMap>();
  state.world->setResource(RenderQueue());
}

void BatchingSystem::onUpdate(const ecs::SystemState &state) {
  ecs::EntityID mainCameraID =
      state.world->getResource<camera::MainCamera>().entity;

  core::Vec3 &cameraPos =
      state.world->getComponent<components::TransformComponent>(mainCameraID)
          .position;
  core::Mat4 &viewProj =
      state.world->getComponent<camera::CameraComponent>(mainCameraID)
          .viewProjectionMatrix;
  const AssetManager &assetManager = state.world->getResource<AssetManager>();

  int drawCalls = 0;

  state.world
      ->query<components::TransformComponent, components::MeshComponent,
              components::MaterialComponent>()
      .eachWithEntity([&](ecs::EntityID entity,
                          components::TransformComponent &t,
                          components::MeshComponent &m,
                          components::MaterialComponent &mat) {
        if (mat.materialID == 0) {
          mat.materialID = assetManager.getMaterialID(
              mat.materialName.empty() ? "default_diffuse"
                                       : mat.materialName.c_str());
        }
        IMaterial &material = assetManager.getMaterial(mat.materialID);
        if (mat.textureID == 0 || mat.shaderID == 0) {
          mat.textureID = material.getTextureID();
          mat.shaderID = material.getShaderID();
        }
        if (m.meshID == 0) {
          m.meshID = assetManager.getMeshID(
              m.meshName.empty() ? "cube" : m.meshName.c_str());
        }

        if (m.visible)
          m_batches->add(m.meshID, mat.materialID, ecs::EntityID{entity});
      });

  auto &renderQueue = state.world->getResource<RenderQueue>();

  for (const auto &[key, entities] : m_batches->getMap()) {
    RenderBatch batch;
    batch.meshID = key.meshID;
    batch.materialID = key.materialID;

    IMaterial &material = assetManager.getMaterial(batch.materialID);
    batch.vertexLayout = material.getVertexLayout();

    for (ecs::EntityID entity : entities) {
      std::vector<float> instanceData =
          material.buildInstanceDataFloats(*state.world, entity);
      batch.instanceDatas.push_back(std::move(instanceData));
    }

    renderQueue.addBatch(std::move(batch));
  }

  m_batches->clear();
}

void BatchingSystem::onDestroy(const ecs::SystemState &state) {
  m_batches.reset();
}

} // namespace componeng::renderer
