#include "componeng/components/camera_component.hpp"
#include "componeng/components/collider_component.hpp"
#include "componeng/core/debug_ui.hpp"
#include "componeng/core/types.hpp"
#include "componeng/ecs/entity.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/api/irender_device.hpp"
#include "componeng/renderer/asset_manager.hpp"

#include "componeng/components/material_component.hpp"
#include "componeng/components/mesh_component.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/renderer/opengl/gl_render_device.hpp"
#include "componeng/renderer/render_system.hpp"
#include "componeng/resources/main_camera.hpp"

namespace componeng::renderer {

static constexpr core::Vec3 DEFAULT_LIGHT_POS =
    core::Vec3(1000.0f, 1000.0f, 1000.0f);
static constexpr core::Vec3 DEFAULT_LIGHT_COLOR = core::Vec3(1.0f, 1.0f, 1.0f);

struct FrustumPlane {
  core::Vec3 normal;
  float distance;
};

struct Frustum {
  FrustumPlane planes[6];
};

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

static bool isBoxInFrustum(const Frustum &frustum, const core::Vec3 &boxMin,
                           const core::Vec3 &boxMax) {
  for (int i = 0; i < 6; i++) {
    const FrustumPlane &plane = frustum.planes[i];
    core::Vec3 positiveVertex;
    positiveVertex.x = (plane.normal.x >= 0) ? boxMax.x : boxMin.x;
    positiveVertex.y = (plane.normal.y >= 0) ? boxMax.y : boxMin.y;
    positiveVertex.z = (plane.normal.z >= 0) ? boxMax.z : boxMin.z;

    if (dot(plane.normal, positiveVertex) + plane.distance < 0) {
      return false;
    }
  }
  return true;
}

static Frustum generateFrustum(const core::Mat4 &m) {
  Frustum f;

  f.planes[0].normal.x = m[0][3] + m[0][0];
  f.planes[0].normal.y = m[1][3] + m[1][0];
  f.planes[0].normal.z = m[2][3] + m[2][0];
  f.planes[0].distance = m[3][3] + m[3][0];

  f.planes[1].normal.x = m[0][3] - m[0][0];
  f.planes[1].normal.y = m[1][3] - m[1][0];
  f.planes[1].normal.z = m[2][3] - m[2][0];
  f.planes[1].distance = m[3][3] - m[3][0];

  f.planes[2].normal.x = m[0][3] + m[0][1];
  f.planes[2].normal.y = m[1][3] + m[1][1];
  f.planes[2].normal.z = m[2][3] + m[2][1];
  f.planes[2].distance = m[3][3] + m[3][1];

  f.planes[3].normal.x = m[0][3] - m[0][1];
  f.planes[3].normal.y = m[1][3] - m[1][1];
  f.planes[3].normal.z = m[2][3] - m[2][1];
  f.planes[3].distance = m[3][3] - m[3][1];

  f.planes[4].normal.x = m[0][3] + m[0][2];
  f.planes[4].normal.y = m[1][3] + m[1][2];
  f.planes[4].normal.z = m[2][3] + m[2][2];
  f.planes[4].distance = m[3][3] + m[3][2];

  f.planes[5].normal.x = m[0][3] - m[0][2];
  f.planes[5].normal.y = m[1][3] - m[1][2];
  f.planes[5].normal.z = m[2][3] - m[2][2];
  f.planes[5].distance = m[3][3] - m[3][2];

  for (int i = 0; i < 6; i++) {
    float len = glm::length(f.planes[i].normal);
    f.planes[i].normal /= len;
    f.planes[i].distance /= len;
  }

  return f;
}

static void populateBatch(const components::TransformComponent &t,
                          const components::MeshComponent &m,
                          const components::MaterialComponent &mat,
                          BatchMap &batches) {
  DrawKey key{m.meshID, mat.textureID, mat.shaderID};
  batches.add(key, {getModelMatrix(t), mat.color});
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

  Frustum frustum = generateFrustum(viewProj);

  int drawCalls = 0;

  state.world
      ->query<components::TransformComponent, components::MeshComponent,
              components::MaterialComponent, components::ColliderComponent>()
      .eachParallel(
          state.world->threadPool(),
          [&](components::TransformComponent &t, components::MeshComponent &m,
              components::MaterialComponent &mat,
              components::ColliderComponent &col) {
            core::Vec3 center = col.transform.position + t.position;
            core::Vec3 worldMin = center - col.transform.scale * t.scale;
            core::Vec3 worldMax = center + col.transform.scale * t.scale;
            m.visible = isBoxInFrustum(frustum, worldMin, worldMax);
          });

  state.world
      ->query<components::TransformComponent, components::MeshComponent,
              components::MaterialComponent>()
      .each([&](components::TransformComponent &t, components::MeshComponent &m,
                components::MaterialComponent &mat) {
        if (m.visible)
          populateBatch(t, m, mat, *m_batches.get());
      });

  const auto &batches = m_batches->getMap();
  for (const auto &pair : batches) {
    const DrawKey &key = pair.first;
    const BatchData &data = pair.second;

    const auto &assetManager = state.world->get_resource<AssetManager>();
    const Shader &shader = assetManager.getShader(key.shaderID);
    const Texture2D &texture = assetManager.getTexture(key.textureID);
    const Mesh &model = assetManager.getMesh(key.meshID);

    shader.use();
    shader.setMatrix4("viewProj", viewProj);
    shader.setVector3f("lightPos", DEFAULT_LIGHT_POS.x, DEFAULT_LIGHT_POS.y,
                       DEFAULT_LIGHT_POS.z);
    shader.setVector3f("lightColor", DEFAULT_LIGHT_COLOR.x,
                       DEFAULT_LIGHT_COLOR.y, DEFAULT_LIGHT_COLOR.z);
    shader.setVector3f("cameraPos", cameraPos.x, cameraPos.y, cameraPos.z);
    shader.setFloat("time", state.world->time);

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
