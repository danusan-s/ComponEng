#include "core/debug_ui.hpp"
#include "core/logger.hpp"
#include "core/types.hpp"
#include "ecs/entity.hpp"
#include "ecs/world.hpp"
#include "renderer/resource_manager.hpp"

#include "components/bounding_box_component.hpp"
#include "components/material_component.hpp"
#include "components/mesh_component.hpp"
#include "components/transform_component.hpp"
#include "systems/render_system.hpp"

static constexpr Vec3 DEFAULT_LIGHT_POS = Vec3(10.0f, 10.0f, 10.0f);
static constexpr Vec3 DEFAULT_LIGHT_COLOR = Vec3(1.0f, 1.0f, 1.0f);

struct Plane {
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

  bool operator==(const DrawKey &other) const {
    return meshName == other.meshName && textureName == other.textureName &&
           shaderName == other.shaderName;
  }
};

struct DrawKeyHash {
  std::size_t operator()(const DrawKey &k) const {
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

static std::unordered_map<DrawKey, BatchData, DrawKeyHash> batches;

static Mat4 GetModelMatrix(const TransformComponent &transform) {
  Mat4 model = Mat4(1.0f);
  model = translate(model, transform.position);
  model = rotate(model, transform.rotation.x, Vec3(1.0f, 0.0f, 0.0f));
  model = rotate(model, transform.rotation.y, Vec3(0.0f, 1.0f, 0.0f));
  model = rotate(model, transform.rotation.z, Vec3(0.0f, 0.0f, 1.0f));
  model = scale(model, transform.scale);
  return model;
}

static bool IsBoxInFrustum(const Frustum &frustum, const Vec3 &boxMin,
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

static Frustum GenerateFrustum(const Mat4 &m) {
  Frustum f;

  // Left
  f.planes[0].normal.x = m[0][3] + m[0][0];
  f.planes[0].normal.y = m[1][3] + m[1][0];
  f.planes[0].normal.z = m[2][3] + m[2][0];
  f.planes[0].distance = m[3][3] + m[3][0];

  // Right
  f.planes[1].normal.x = m[0][3] - m[0][0];
  f.planes[1].normal.y = m[1][3] - m[1][0];
  f.planes[1].normal.z = m[2][3] - m[2][0];
  f.planes[1].distance = m[3][3] - m[3][0];

  // Bottom
  f.planes[2].normal.x = m[0][3] + m[0][1];
  f.planes[2].normal.y = m[1][3] + m[1][1];
  f.planes[2].normal.z = m[2][3] + m[2][1];
  f.planes[2].distance = m[3][3] + m[3][1];

  // Top
  f.planes[3].normal.x = m[0][3] - m[0][1];
  f.planes[3].normal.y = m[1][3] - m[1][1];
  f.planes[3].normal.z = m[2][3] - m[2][1];
  f.planes[3].distance = m[3][3] - m[3][1];

  // Near
  f.planes[4].normal.x = m[0][3] + m[0][2];
  f.planes[4].normal.y = m[1][3] + m[1][2];
  f.planes[4].normal.z = m[2][3] + m[2][2];
  f.planes[4].distance = m[3][3] + m[3][2];

  // Far
  f.planes[5].normal.x = m[0][3] - m[0][2];
  f.planes[5].normal.y = m[1][3] - m[1][2];
  f.planes[5].normal.z = m[2][3] - m[2][2];
  f.planes[5].distance = m[3][3] - m[3][2];

  // Normalize
  for (int i = 0; i < 6; i++) {
    float len = glm::length(f.planes[i].normal);
    f.planes[i].normal /= len;
    f.planes[i].distance /= len;
  }

  return f;
}

void OpenGLRenderSystem::Update(float deltaTime) {
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  Mat4 viewProj = this->world->mainCameraData.projectionMatrix *
                  this->world->mainCameraData.viewMatrix;

  Frustum frustum = GenerateFrustum(viewProj);

  int drawCalls = 0;

  for (auto &pair : batches) {
    pair.second.instanceDatas.clear();
  }

  world
      ->query<TransformComponent, MeshComponent, MaterialComponent,
              BoundingBoxComponent>()
      .each([&](TransformComponent &t, MeshComponent &m, MaterialComponent &mat,
                BoundingBoxComponent &bbox) {
        Vec3 worldMin = t.position + bbox.min;
        Vec3 worldMax = t.position + bbox.max;
        if (!IsBoxInFrustum(frustum, worldMin, worldMax)) {
          return;
        }
        DrawKey key{m.meshName, mat.textureName, mat.shaderName};
        if (batches.find(key) == batches.end()) {
          batches[key] = BatchData();
          const Mesh &mesh = ResourceManager::GetMesh(key.meshName);
          glGenBuffers(1, &batches[key].instanceVBO);
          glBindVertexArray(mesh.VAO);
          glBindBuffer(GL_ARRAY_BUFFER, batches[key].instanceVBO);
          glBufferData(GL_ARRAY_BUFFER, MAX_ENTITIES * sizeof(InstanceData),
                       nullptr, GL_DYNAMIC_DRAW);

          // Model matrix (4 vec4)
          for (int i = 0; i < 4; ++i) {
            glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE,
                                  sizeof(InstanceData),
                                  (void *)(offsetof(InstanceData, modelMatrix) +
                                           sizeof(Vec4) * i));
            glEnableVertexAttribArray(3 + i);
            glVertexAttribDivisor(3 + i, 1);
          }
          glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                                (void *)offsetof(InstanceData, color));
          glEnableVertexAttribArray(7);
          glVertexAttribDivisor(7, 1);
          glBindVertexArray(0);

          LOG_INFO("Created batch for mesh %s, material %s, shader %s with VBO %u",
              key.meshName.c_str(), key.textureName.c_str(),
              key.shaderName.c_str(), batches[key].instanceVBO);
        }
        batches[key].instanceDatas.push_back({GetModelMatrix(t), mat.color});
      });

  for (const auto &pair : batches) {
    const DrawKey &key = pair.first;
    const BatchData &data = pair.second;

    const Shader &shader = ResourceManager::GetShader(key.shaderName);
    const Texture2D &texture = ResourceManager::GetTexture(key.textureName);
    const Mesh &model = ResourceManager::GetMesh(key.meshName);

    shader.Use();
    shader.SetMatrix4("viewProj", viewProj);
    shader.SetVector3f("lightPos", DEFAULT_LIGHT_POS);
    shader.SetVector3f("lightColor", DEFAULT_LIGHT_COLOR);

    texture.Bind();
    glBindVertexArray(model.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, data.instanceVBO);

    // Upload instance data
    glBindBuffer(GL_ARRAY_BUFFER, data.instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    data.instanceDatas.size() * sizeof(InstanceData),
                    data.instanceDatas.data());

    glDrawElementsInstanced(GL_TRIANGLES, model.indices.size(), GL_UNSIGNED_INT,
                            0, data.instanceDatas.size());

    glBindVertexArray(0);
    ++drawCalls;
  }

  DebugUI::AddValue("Instances Rendered", batches.size());
  DebugUI::AddValue("Draw Calls", drawCalls);
}
