#include "componeng/renderer/render_system.hpp"

#include "componeng/components/camera_component.hpp"
#include "componeng/components/light_component.hpp"
#include "componeng/components/material_component.hpp"
#include "componeng/components/mesh_component.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/core/debug_ui.hpp"
#include "componeng/core/types.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/api/irender_device.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/renderer/material.hpp"
#include "componeng/resources/main_camera.hpp"
#include "componeng/utils/logger.hpp"

namespace componeng::renderer {

static constexpr core::Vec3 DEFAULT_LIGHT_DIR = core::Vec3(-0.2f, 1.0f, -0.3f);
static constexpr core::Vec3 DEFAULT_LIGHT_COLOR = core::Vec3(1.0f, 1.0f, 1.0f);

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

static void setShaderUniforms(const Shader &shader, const Material &material) {
  // Set material uniforms
  for (const auto &[name, value] : material.getUniforms()) {
    const char *iname = name;
    std::visit(
        [&shader, iname](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, float>) {
            shader.setFloat(iname, arg);
          } else if constexpr (std::is_same_v<T, core::Vec2>) {
            shader.setVector2f(iname, arg);
          } else if constexpr (std::is_same_v<T, core::Vec3>) {
            shader.setVector3f(iname, arg);
          } else if constexpr (std::is_same_v<T, core::Vec4>) {
            shader.setVector4f(iname, arg);
          } else if constexpr (std::is_same_v<T, core::Mat4>) {
            shader.setMatrix4(iname, arg);
          }
        },
        value);
  }
}

void RenderSystem::onCreate(const ecs::SystemState &state) {
  api::IRenderDevice &renderDevice = state.world->getRenderDevice();
  m_batches = std::make_unique<BatchMap>(renderDevice);
}

void RenderSystem::onUpdate(const ecs::SystemState &state) {
  api::IRenderDevice &renderDevice = state.world->getRenderDevice();
  renderDevice.clear(0.0f, 0.0f, 0.0f, 1.0f);

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

  core::Vec3 lightDir = DEFAULT_LIGHT_DIR;
  core::Vec3 lightColor = DEFAULT_LIGHT_COLOR;

  state.world->query<components::DirectionalLightComponent>().each(
      [&](components::DirectionalLightComponent &l) {
        lightDir = l.direction;
        lightColor = l.color;
      });

  const auto &batches = m_batches->getMap();
  for (const auto &pair : batches) {
    const DrawKey &key = pair.first;
    const BatchData &data = pair.second;

    const auto &assetManager = state.world->get_resource<AssetManager>();
    auto &material = assetManager.getMaterial(key.materialID);
    const Shader &shader = assetManager.getShader(material.getShaderID());
    const Texture2D &texture = assetManager.getTexture(material.getTextureID());

    const Mesh &model = assetManager.getMesh(key.meshID);

    shader.use();
    shader.setMatrix4("viewProj", viewProj);
    shader.setVector3f("lightDir", lightDir);
    shader.setVector3f("lightColor", lightColor);
    shader.setVector3f("cameraPos", cameraPos.x, cameraPos.y, cameraPos.z);
    shader.setFloat("time", state.world->time);

    setShaderUniforms(shader, material);

    texture.bind();
    model.getImpl().bind();

    renderDevice.setupInstanceAttributes(*data.instanceBuffer);

    data.instanceBuffer->setSubData(0, data.instanceDatas.data(),
                                    data.instanceDatas.size() *
                                        sizeof(InstanceData));

    renderDevice.drawIndexedInstanced(model.indexCount(),
                                      data.instanceDatas.size());

    renderDevice.unbindInstanceAttributes();

    ++drawCalls;
  }

  int instancesRendered = 0;
  for (const auto &pair : batches) {
    instancesRendered += pair.second.instanceDatas.size();
  }

  m_batches->clear();

  core::DebugUI::addValue("Instances Rendered", instancesRendered);
  core::DebugUI::addValue("Draw Calls", drawCalls);
}

void RenderSystem::onDestroy(const ecs::SystemState &state) {
  m_batches.reset();
}

} // namespace componeng::renderer
