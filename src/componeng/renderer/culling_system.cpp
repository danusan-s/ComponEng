#include "componeng/renderer/culling_system.hpp"

#include "componeng/components/camera_component.hpp"
#include "componeng/components/collider_component.hpp"
#include "componeng/components/material_component.hpp"
#include "componeng/components/mesh_component.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/core/types.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/resources/main_camera.hpp"

namespace componeng::renderer {

struct FrustumPlane {
  core::Vec3 normal;
  float distance;
};

struct Frustum {
  FrustumPlane planes[6];
};

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

void CullingSystem::onUpdate(const ecs::SystemState &state) {
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
}

} // namespace componeng::renderer
