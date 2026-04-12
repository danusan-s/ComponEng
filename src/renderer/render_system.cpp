#include "components/camera_component.hpp"
#include "components/collider_component.hpp"
#include "core/debug_ui.hpp"
#include "core/types.hpp"
#include "ecs/entity.hpp"
#include "ecs/world.hpp"
#include "renderer/resource_manager.hpp"

#include "components/material_component.hpp"
#include "components/mesh_component.hpp"
#include "components/transform_component.hpp"
#include "renderer/opengl/gl_render_device.hpp"
#include "renderer/render_system.hpp"

static constexpr Vec3 DEFAULT_LIGHT_POS = Vec3(1000.0f, 1000.0f, 1000.0f);
static constexpr Vec3 DEFAULT_LIGHT_COLOR = Vec3(1.0f, 1.0f, 1.0f);

struct FrustumPlane {
  Vec3 normal;
  float distance;
};

struct Frustum {
  Plane planes[6];
};

static Mat4 getModelMatrix(const TransformComponent &transform) {
  Mat4 model = Mat4(1.0f);
  model = translate(model, transform.position);
  model = rotate(model, transform.rotation.x, Vec3(1.0f, 0.0f, 0.0f));
  model = rotate(model, transform.rotation.y, Vec3(0.0f, 1.0f, 0.0f));
  model = rotate(model, transform.rotation.z, Vec3(0.0f, 0.0f, 1.0f));
  model = scale(model, transform.scale);
  return model;
}

static bool isBoxInFrustum(const Frustum &frustum, const Vec3 &boxMin,
                           const Vec3 &boxMax) {
  for (int i = 0; i < 6; i++) {
    const Plane &plane = frustum.planes[i];
    Vec3 positiveVertex;
    positiveVertex.x = (plane.normal.x >= 0) ? boxMax.x : boxMin.x;
    positiveVertex.y = (plane.normal.y >= 0) ? boxMax.y : boxMin.y;
    positiveVertex.z = (plane.normal.z >= 0) ? boxMax.z : boxMin.z;

    if (dot(plane.normal, positiveVertex) + plane.distance < 0) {
      return false;
    }
  }
  return true;
}

static Frustum generateFrustum(const Mat4 &m) {
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

static void populateBatch(const TransformComponent &t, const MeshComponent &m,
                          const MaterialComponent &mat, BatchMap &batches) {
  DrawKey key{m.meshID, mat.textureID, mat.shaderID};
  batches.add(key, {getModelMatrix(t), mat.color});
}

void RenderSystem::onCreate(const SystemState &state) {
  m_device = std::make_unique<GLRenderDevice>();
  m_device->init(state.world->getWindowHandle());
  m_batches = std::make_unique<BatchMap>(*m_device);
}

void RenderSystem::onUpdate(const SystemState &state) {
  m_device->clear(0.0f, 0.0f, 0.0f, 1.0f);

  EntityID mainCameraID =
      state.world->getSingleton<MainCameraSingleton>().entity;

  Vec3 &cameraPos =
      state.world->getComponent<TransformComponent>(mainCameraID).position;
  Mat4 &viewProj = state.world->getComponent<CameraComponent>(mainCameraID)
                       .viewProjectionMatrix;

  Frustum frustum = generateFrustum(viewProj);

  int drawCalls = 0;

  state.world
      ->query<TransformComponent, MeshComponent, MaterialComponent,
              ColliderComponent>()
      .eachParallel(state.world->threadPool(), [&](TransformComponent &t,
                                                   MeshComponent &m,
                                                   MaterialComponent &mat,
                                                   ColliderComponent &col) {
        Vec3 center, worldMin, worldMax;
        if (col.type == ColliderType::AABB) {
          center = t.position + std::get<AABB>(col.shape).localCenter;
          worldMin = center - std::get<AABB>(col.shape).halfExtents;
          worldMax = center + std::get<AABB>(col.shape).halfExtents;
        } else if (col.type == ColliderType::Sphere) {
          center = t.position + std::get<Sphere>(col.shape).localCenter;
          float radius = std::get<Sphere>(col.shape).radius;
          worldMin = center - Vec3(radius);
          worldMax = center + Vec3(radius);
        } else {
          center = t.position;
          worldMin = center - t.scale;
          worldMax = center + t.scale;
        }
        m.visible = isBoxInFrustum(frustum, worldMin, worldMax);
      });

  state.world->query<TransformComponent, MeshComponent, MaterialComponent>()
      .each(
          [&](TransformComponent &t, MeshComponent &m, MaterialComponent &mat) {
            if (m.visible)
              populateBatch(t, m, mat, *m_batches.get());
          });

  const auto &batches = m_batches->getMap();
  for (const auto &pair : batches) {
    const DrawKey &key = pair.first;
    const BatchData &data = pair.second;

    const Shader &shader = ResourceManager::getShader(key.shaderID);
    const Texture2D &texture = ResourceManager::getTexture(key.textureID);
    const Mesh &model = ResourceManager::getMesh(key.meshID);

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

    m_device->setupInstanceAttributes(*data.instanceBuffer);

    data.instanceBuffer->setSubData(0, data.instanceDatas.data(),
                                    data.instanceDatas.size() *
                                        sizeof(InstanceData));

    m_device->drawIndexedInstanced(model.indexCount(),
                                   data.instanceDatas.size());

    m_device->unbindInstanceAttributes();

    ++drawCalls;
  }

  int instancesRendered = 0;
  for (const auto &pair : batches) {
    instancesRendered += pair.second.instanceDatas.size();
  }

  m_batches->clear();

  DebugUI::addValue("Instances Rendered", instancesRendered);
  DebugUI::addValue("Draw Calls", drawCalls);
}

void RenderSystem::onDestroy(const SystemState &state) {
  m_batches.reset();
  m_device.reset();
}
