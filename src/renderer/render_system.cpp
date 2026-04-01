#include "components/camera_component.hpp"
#include "components/collider_component.hpp"
#include "core/debug_ui.hpp"
#include "core/logger.hpp"
#include "core/types.hpp"
#include "ecs/entity.hpp"
#include "ecs/world.hpp"
#include "renderer/resource_manager.hpp"

#include "components/material_component.hpp"
#include "components/mesh_component.hpp"
#include "components/transform_component.hpp"
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

struct DrawKey {
  std::string meshName;
  std::string textureName;
  std::string shaderName;

  bool operator==(const DrawKey& other) const {
    return meshName == other.meshName && textureName == other.textureName &&
           shaderName == other.shaderName;
  }
};

struct DrawKeyHash {
  std::size_t operator()(const DrawKey& k) const {
    return std::hash<std::string>()(k.meshName) ^
           (std::hash<std::string>()(k.textureName) << 1) ^
           (std::hash<std::string>()(k.shaderName) << 2);
  }
};

struct InstanceData {
  Mat4 modelMatrix;
  Vec3 color;
};

struct BatchData {
  GLuint instanceVBO = 0;
  std::vector<InstanceData> instanceDatas;
};

static std::unordered_map<DrawKey, BatchData, DrawKeyHash> g_batches;

static Mat4 getModelMatrix(const TransformComponent& transform) {
  Mat4 model = Mat4(1.0f);
  model = translate(model, transform.position);
  model = rotate(model, transform.rotation.x, Vec3(1.0f, 0.0f, 0.0f));
  model = rotate(model, transform.rotation.y, Vec3(0.0f, 1.0f, 0.0f));
  model = rotate(model, transform.rotation.z, Vec3(0.0f, 0.0f, 1.0f));
  model = scale(model, transform.scale);
  return model;
}

static bool isBoxInFrustum(const Frustum& frustum, const Vec3& boxMin,
                           const Vec3& boxMax) {
  for (int i = 0; i < 6; i++) {
    const Plane& plane = frustum.planes[i];
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

static Frustum generateFrustum(const Mat4& m) {
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

static void populateBatch(const TransformComponent& t, const MeshComponent& m,
                          const MaterialComponent& mat) {
  DrawKey key{m.meshName, mat.textureName, mat.shaderName};
  if (g_batches.find(key) == g_batches.end()) {
    g_batches[key] = BatchData();
    const Mesh& mesh = ResourceManager::getMesh(key.meshName);
    glGenBuffers(1, &g_batches[key].instanceVBO);
    glBindVertexArray(mesh.m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_batches[key].instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_ENTITIES * sizeof(InstanceData), nullptr,
                 GL_DYNAMIC_DRAW);

    for (int i = 0; i < 4; ++i) {
      glVertexAttribPointer(
          3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
          (void*)(offsetof(InstanceData, modelMatrix) + sizeof(Vec4) * i));
      glEnableVertexAttribArray(3 + i);
      glVertexAttribDivisor(3 + i, 1);
    }
    glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                          (void*)offsetof(InstanceData, color));
    glEnableVertexAttribArray(7);
    glVertexAttribDivisor(7, 1);
    glBindVertexArray(0);

    LOG_INFO("Created batch for mesh %s, material %s, shader %s "
             "with VBO %u",
             key.meshName.c_str(), key.textureName.c_str(),
             key.shaderName.c_str(), g_batches[key].instanceVBO);
  }
  g_batches[key].instanceDatas.push_back({getModelMatrix(t), mat.color});
}

void OpenGLRenderSystem::onUpdate(const SystemState& state) {
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  EntityID mainCameraID =
      state.world->getSingleton<MainCameraSingleton>().entity;

  Vec3& cameraPos =
      state.world->getComponent<TransformComponent>(mainCameraID).position;
  Mat4& viewProj = state.world->getComponent<CameraComponent>(mainCameraID)
                       .viewProjectionMatrix;

  Frustum frustum = generateFrustum(viewProj);

  int drawCalls = 0;

  for (auto& pair : g_batches) {
    pair.second.instanceDatas.clear();
  }

  state.world
      ->query<TransformComponent, MeshComponent, MaterialComponent,
              ColliderComponent>()
      .each([&](TransformComponent& t, MeshComponent& m, MaterialComponent& mat,
                ColliderComponent& col) {
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
        if (!isBoxInFrustum(frustum, worldMin, worldMax))
          return;
        populateBatch(t, m, mat);
      });

  state.world->query<TransformComponent, MeshComponent, MaterialComponent>()
      .exclude<ColliderComponent>()
      .each([&](TransformComponent& t, MeshComponent& m,
                MaterialComponent& mat) { populateBatch(t, m, mat); });

  for (const auto& pair : g_batches) {
    const DrawKey& key = pair.first;
    const BatchData& data = pair.second;

    const Shader& shader = ResourceManager::getShader(key.shaderName);
    const Texture2D& texture = ResourceManager::getTexture(key.textureName);
    const Mesh& model = ResourceManager::getMesh(key.meshName);

    shader.use();
    shader.setMatrix4("viewProj", viewProj);
    shader.setVector3f("lightPos", DEFAULT_LIGHT_POS);
    shader.setVector3f("lightColor", DEFAULT_LIGHT_COLOR);
    shader.setVector3f("cameraPos", cameraPos);
    shader.setFloat("time", state.world->time);

    texture.bind();
    glBindVertexArray(model.m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, data.instanceVBO);

    glBindBuffer(GL_ARRAY_BUFFER, data.instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    data.instanceDatas.size() * sizeof(InstanceData),
                    data.instanceDatas.data());

    glDrawElementsInstanced(GL_TRIANGLES, model.m_indices.size(), GL_UNSIGNED_INT,
                            0, data.instanceDatas.size());

    glBindVertexArray(0);
    ++drawCalls;
  }

  int instancesRendered = 0;
  for (const auto& pair : g_batches) {
    instancesRendered += pair.second.instanceDatas.size();
  }
  DebugUI::addValue("Instances Rendered", instancesRendered);
  DebugUI::addValue("Draw Calls", drawCalls);
}

void OpenGLRenderSystem::onDestroy(const SystemState& state) {
  for (auto& pair : g_batches) {
    if (pair.second.instanceVBO != 0) {
      glDeleteBuffers(1, &pair.second.instanceVBO);
      pair.second.instanceVBO = 0;
    }
  }
}
