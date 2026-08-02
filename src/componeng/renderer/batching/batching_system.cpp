#include "componeng/renderer/batching/batching_system.hpp"

#include "componeng/components/camera_component.hpp"
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
#include "componeng/resources/main_camera.hpp"

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

static void populateBatch(const components::TransformComponent &t,
                          core::HandleID meshID, core::HandleID materialID,
                          BatchMap &batches) {
  DrawKey key{meshID, materialID};
  batches.add(key, {getModelMatrix(t)});
}

void BatchingSystem::onCreate(const ecs::SystemState &state) {
  api::IRenderDevice &renderDevice = state.world->getRenderDevice();
  m_batches = std::make_unique<BatchMap>(renderDevice);
  state.world->set_resource(RenderQueue());
}

void BatchingSystem::onUpdate(const ecs::SystemState &state) {
  ecs::EntityID mainCameraID =
      state.world->get_resource<resources::MainCamera>().entity;

  core::Vec3 &cameraPos =
      state.world->getComponent<components::TransformComponent>(mainCameraID)
          .position;
  core::Mat4 &viewProj =
      state.world->getComponent<components::CameraComponent>(mainCameraID)
          .viewProjectionMatrix;

  int drawCalls = 0;

  state.world
      ->query<components::TransformComponent, components::MeshComponent,
              components::MaterialComponent>()
      .each([&](components::TransformComponent &t, components::MeshComponent &m,
                components::MaterialComponent &mat) {
        if (mat.materialID == 0) {
          mat.materialID =
              state.world->get_resource<AssetManager>().getMaterialID(
                  mat.materialName.empty() ? "default_diffuse"
                                           : mat.materialName.c_str());
        }
        if (mat.textureID == 0 || mat.shaderID == 0) {
          const AssetManager &assetManager =
              state.world->get_resource<AssetManager>();
          Material &material = assetManager.getMaterial(mat.materialID);
          mat.textureID = material.getTextureID();
          mat.shaderID = material.getShaderID();
        }
        if (m.meshID == 0) {
          m.meshID = state.world->get_resource<AssetManager>().getMeshID(
              m.meshName.empty() ? "cube" : m.meshName.c_str());
        }
        if (m.visible)
          populateBatch(t, m.meshID, mat.materialID, *m_batches.get());
      });

  auto &renderQueue = state.world->get_resource<RenderQueue>();

  renderQueue.batches = std::move(m_batches->getMap());

  m_batches->clear();
}

void BatchingSystem::onDestroy(const ecs::SystemState &state) {
  m_batches.reset();
}

} // namespace componeng::renderer
