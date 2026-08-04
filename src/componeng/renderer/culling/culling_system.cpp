#include "componeng/renderer/culling/culling_system.hpp"
#include "componeng/renderer/culling/frustum.hpp"

#include "componeng/camera/camera_component.hpp"
#include "componeng/camera/main_camera.hpp"
#include "componeng/components/material_component.hpp"
#include "componeng/components/mesh_component.hpp"
#include "componeng/components/transform_component.hpp"
#include "componeng/core/types.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/physics/collider_component.hpp"

namespace componeng::renderer {

void CullingSystem::onUpdate(const ecs::SystemState &state) {
  ecs::EntityID mainCameraID =
      state.world->get_resource<camera::MainCamera>().entity;

  core::Vec3 &cameraPos =
      state.world->getComponent<components::TransformComponent>(mainCameraID)
          .position;
  core::Mat4 &viewProj =
      state.world->getComponent<camera::CameraComponent>(mainCameraID)
          .viewProjectionMatrix;

  Frustum frustum(viewProj);

  int drawCalls = 0;

  state.world
      ->query<components::TransformComponent, components::MeshComponent,
              components::MaterialComponent, physics::ColliderComponent>()
      .eachParallel(
          state.world->threadPool(),
          [&](components::TransformComponent &t, components::MeshComponent &m,
              components::MaterialComponent &mat,
              physics::ColliderComponent &col) {
            core::Vec3 center = col.transform.position + t.position;
            core::Vec3 worldMin = center - col.transform.scale * t.scale;
            core::Vec3 worldMax = center + col.transform.scale * t.scale;
            m.visible = frustum.isBoxInFrustum(worldMin, worldMax);
          });
}

} // namespace componeng::renderer
