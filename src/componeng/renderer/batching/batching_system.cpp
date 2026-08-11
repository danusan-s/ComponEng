#include "componeng/renderer/batching/batching_system.hpp"

#include "componeng/camera/camera_component.hpp"
#include "componeng/camera/main_camera.hpp"
#include "componeng/core/transform_component.hpp"
#include "componeng/core/types.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/asset/material.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/renderer/backend/api/irender_device.hpp"
#include "componeng/renderer/batching/render_queue.hpp"
#include "componeng/renderer/component/material_component.hpp"
#include "componeng/renderer/component/mesh_component.hpp"

namespace componeng::renderer {

void BatchingSystem::onCreate(const ecs::SystemState &state) {
  m_batches = std::make_unique<BatchMap>();
  state.world->setResource(RenderQueue());
}

void BatchingSystem::onUpdate(const ecs::SystemState &state) {
  const AssetManager &assetManager = state.world->getResource<AssetManager>();

  state.world
      ->query<core::TransformComponent, renderer::MeshComponent,
              renderer::MaterialComponent>()
      .eachWithEntity([&](ecs::EntityID entity, core::TransformComponent &,
                          renderer::MeshComponent &m,
                          renderer::MaterialComponent &mat) {
        if (mat.materialID == 0) {
          mat.materialID = assetManager.getMaterialID(
              mat.materialName.empty() ? core::Name("default_diffuse")
                                       : mat.materialName);
        }
        IMaterial &material = assetManager.getMaterial(mat.materialID);
        if (mat.textureID == 0 || mat.shaderID == 0) {
          mat.textureID = material.getTextureID();
          mat.shaderID = material.getShaderID();
        }
        if (m.meshID == 0) {
          m.meshID = assetManager.getMeshID(
              m.meshName.empty() ? core::Name("cube") : m.meshName);
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
    batch.instanceDatas.reserve(entities.size());

    for (ecs::EntityID entity : entities) {
      std::vector<float> instanceData =
          material.buildInstanceDataFloats(*state.world, entity);
      batch.instanceDatas.push_back(std::move(instanceData));
    }

    renderQueue.addBatch(std::move(batch));
  }

  m_batches->clear();
}

void BatchingSystem::onDestroy(const ecs::SystemState &) {
  m_batches.reset();
}

} // namespace componeng::renderer
