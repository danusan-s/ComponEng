#include "componeng/renderer/culling/culling_system.hpp"
#include "componeng/renderer/culling/bounds.hpp"
#include "componeng/renderer/culling/frustum.hpp"

#include "componeng/camera/camera_component.hpp"
#include "componeng/camera/main_camera.hpp"
#include "componeng/core/transform_component.hpp"
#include "componeng/core/types.hpp"
#include "componeng/ecs/world.hpp"
#include "componeng/renderer/asset_manager.hpp"
#include "componeng/renderer/component/material_component.hpp"
#include "componeng/renderer/component/mesh_component.hpp"
#include "componeng/renderer/component/procedural_instances_component.hpp"

namespace componeng::renderer {

void CullingSystem::onCreate(const ecs::SystemState &state) {
  // The queries below name this component; registering is idempotent.
  state.world->registerComponent<ProceduralInstancesComponent>();
}

void CullingSystem::onUpdate(const ecs::SystemState &state) {
  ecs::EntityID mainCameraID =
      state.world->getResource<camera::MainCamera>().entity;

  core::Mat4 &viewProj =
      state.world->getComponent<camera::CameraComponent>(mainCameraID)
          .viewProjectionMatrix;

  Frustum frustum(viewProj);
  const AssetManager &assets = state.world->getResource<AssetManager>();

  // The result goes into `culled`, leaving the application's `visible` flag
  // alone. BatchingSystem resolves names to ids on an entity's first frame;
  // until then there is nothing to measure, so it is drawn.
  auto test = [&](core::TransformComponent &t, renderer::MeshComponent &m,
                  renderer::MaterialComponent &mat) {
    if (m.meshID == 0 || mat.materialID == 0) {
      m.culled = false;
      return;
    }

    const Mesh &mesh = assets.getMesh(m.meshID);
    const float padding = assets.getMaterial(mat.materialID).getBoundsPadding();
    const Aabb world = transformAabb(mesh.bounds(), t.toMatrix(), padding);
    m.culled = !frustum.isBoxInFrustum(world.min, world.max);
  };

  // Every renderable entity is tested against the frustum using its mesh's own
  // bounds under its transform, so culling works with or without a physics
  // collider.
  state.world
      ->query<core::TransformComponent, renderer::MeshComponent,
              renderer::MaterialComponent>()
      .exclude<ProceduralInstancesComponent>()
      .eachParallel(state.world->threadPool(), test);

  // Procedural entities place their instances in the shader, so the mesh's
  // bounds only mean something if they opted in.
  state.world
      ->query<core::TransformComponent, renderer::MeshComponent,
              renderer::MaterialComponent, ProceduralInstancesComponent>()
      .each([&](core::TransformComponent &t, renderer::MeshComponent &m,
                renderer::MaterialComponent &mat,
                ProceduralInstancesComponent &procedural) {
        if (procedural.cull) {
          test(t, m, mat);
        } else {
          m.culled = false;
        }
      });
}

} // namespace componeng::renderer
