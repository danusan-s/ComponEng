#include "systems/render_system.hpp"
#include "components/camera_component.hpp"
#include "components/material_component.hpp"
#include "components/mesh_component.hpp"
#include "components/transform_component.hpp"
#include "resource_manager.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "world.hpp"

static constexpr Vec3 DEFAULT_LIGHT_POS = Vec3(10.0f, 10.0f, 10.0f);
static constexpr Vec3 DEFAULT_LIGHT_COLOR = Vec3(1.0f, 1.0f, 1.0f);

static Mat4 GetModelMatrix(const TransformComponent &transform) {
  Mat4 model = Mat4(1.0f);
  model = translate(model, transform.position);
  model = rotate(model, transform.rotation.x, Vec3(1.0f, 0.0f, 0.0f));
  model = rotate(model, transform.rotation.y, Vec3(0.0f, 1.0f, 0.0f));
  model = rotate(model, transform.rotation.z, Vec3(0.0f, 0.0f, 1.0f));
  model = scale(model, transform.scale);
  return model;
}

void OpenGLRenderSystem::Update(float deltaTime) {
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  Mat4 viewProj = this->world->mainCameraData.projectionMatrix *
                  this->world->mainCameraData.viewMatrix;

  world->query<TransformComponent, MeshComponent, MaterialComponent>().each(
      [&](TransformComponent &transform, MeshComponent &mesh,
          MaterialComponent &material) {
        const Shader &shader = ResourceManager::GetShader(material.shaderName);
        const Texture2D &texture =
            ResourceManager::GetTexture(material.textureName);

        shader.Use();
        shader.SetMatrix4("viewProj", viewProj);
        shader.SetMatrix4("model", GetModelMatrix(transform));
        shader.SetVector3f("objectColor", material.color);
        shader.SetVector3f("lightPos", DEFAULT_LIGHT_POS);
        shader.SetVector3f("lightColor", DEFAULT_LIGHT_COLOR);

        texture.Bind();
        glBindVertexArray(mesh.VAO);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
      });
}
